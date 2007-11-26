/**************************************************************************
 * Copyright (c) Intel Corp. 2007.
 * All Rights Reserved.
 *
 * Intel funded Tungsten Graphics (http://www.tungstengraphics.com) to
 * develop this driver.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Make calls into closed source X server code. Ugh....
 */

#include "drmP.h"
#include "psb_drv.h"

static inline int psb_xhw_add(struct drm_psb_private *dev_priv,
			      struct psb_xhw_buf *buf)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&dev_priv->xhw_lock, irq_flags);
	atomic_set(&buf->done, 0);
	if (unlikely(!dev_priv->xhw_submit_ok)) {
		spin_unlock_irqrestore(&dev_priv->xhw_lock, irq_flags);
		DRM_ERROR("No Xpsb 3D extension available\n");
		return -EINVAL;
	}
	list_add_tail(&buf->head, &dev_priv->xhw_in);
	wake_up_interruptible(&dev_priv->xhw_queue);
	spin_unlock_irqrestore(&dev_priv->xhw_lock, irq_flags);
	return 0;
}

int psb_xhw_scene_info(struct drm_psb_private *dev_priv,
	               struct psb_xhw_buf *buf,
		       uint32_t w, 
		       uint32_t h,
		       uint32_t *hw_cookie,
		       uint32_t *bo_size,
		       uint32_t *clear_p_start,
		       uint32_t *clear_num_pages)
{
	struct drm_psb_xhw_arg *xa = &buf->arg;
	int ret;

	buf->issue_irq = 0;
	buf->copy_back = 1;
	xa->op = PSB_XHW_SCENE_INFO;
	xa->arg.si.w = w;
	xa->arg.si.h = h;

	ret = psb_xhw_add(dev_priv, buf);
	if (ret)
		return ret;

	(void) wait_event_timeout(dev_priv->xhw_caller_queue,
				  atomic_read(&buf->done),
				  DRM_HZ);

	if (!atomic_read(&buf->done))
		return -EBUSY;

	if (!xa->ret) {
		memcpy(hw_cookie, xa->cookie, sizeof(xa->cookie));
		*bo_size = xa->arg.si.size;
		*clear_p_start = xa->arg.si.clear_p_start;
		*clear_num_pages = xa->arg.si.clear_num_pages;
	}
	return xa->ret;
}

int psb_xhw_fire_raster(struct drm_psb_private *dev_priv,
			struct psb_xhw_buf *buf,
			uint32_t fire_flags)
{
	struct drm_psb_xhw_arg *xa = &buf->arg;

	buf->issue_irq = 0;
	buf->copy_back = 0;
	xa->op = PSB_XHW_FIRE_RASTER;
	xa->arg.sb.fire_flags = 0;

	return psb_xhw_add(dev_priv, buf);
}

int psb_xhw_scene_bind_fire(struct drm_psb_private *dev_priv,
			    struct psb_xhw_buf *buf,
			    uint32_t fire_flags,
			    uint32_t hw_context,
	                    uint32_t *cookie,
	                    uint32_t offset,
	                    uint32_t engine,
	                    uint32_t flags)
{
	struct drm_psb_xhw_arg *xa = &buf->arg;

	buf->issue_irq = 0;
	buf->copy_back = 0;
	xa->op = PSB_XHW_SCENE_BIND_FIRE;
	xa->arg.sb.fire_flags = fire_flags;
	xa->arg.sb.hw_context = hw_context;
	xa->arg.sb.offset = offset;
	xa->arg.sb.engine = engine;
	xa->arg.sb.flags = flags;
	memcpy(xa->cookie, cookie, sizeof(xa->cookie));

	return psb_xhw_add(dev_priv, buf);
}

int psb_xhw_reset_dpm(struct drm_psb_private *dev_priv,
		      struct psb_xhw_buf *buf)
{
	struct drm_psb_xhw_arg *xa = &buf->arg;
	int ret;

	buf->issue_irq = 0;
	buf->copy_back = 1;
	xa->op = PSB_XHW_RESET_DPM;

	ret = psb_xhw_add(dev_priv, buf);
	if (ret)
		return ret;

	(void) wait_event_timeout(dev_priv->xhw_caller_queue,
				  atomic_read(&buf->done),
				  DRM_HZ);

	if (!atomic_read(&buf->done))
		return -EBUSY;

	return xa->ret;
}

static int psb_xhw_terminate(struct drm_psb_private *dev_priv,
			     struct psb_xhw_buf *buf)
{
	struct drm_psb_xhw_arg *xa = &buf->arg;
	unsigned long irq_flags;

	buf->issue_irq = 0;
	buf->copy_back = 0;
	xa->op = PSB_XHW_TERMINATE;

	spin_lock_irqsave(&dev_priv->xhw_lock, irq_flags);
	dev_priv->xhw_submit_ok = 0;
	atomic_set(&buf->done, 0);
	list_add_tail(&buf->head, &dev_priv->xhw_in);
	spin_unlock_irqrestore(&dev_priv->xhw_lock, irq_flags);
	wake_up_interruptible(&dev_priv->xhw_queue);

	(void) wait_event_timeout(dev_priv->xhw_caller_queue,
				  atomic_read(&buf->done),
				  DRM_HZ / 10);

	if (!atomic_read(&buf->done)) {
		DRM_ERROR("Xpsb terminate timeout.\n");
		return -EBUSY;
	}

	return 0;
}

int psb_xhw_bin_mem_info(struct drm_psb_private *dev_priv,
			 struct psb_xhw_buf *buf,
			 uint32_t pages, 
			 uint32_t *hw_cookie,
			 uint32_t *size)
{
	struct drm_psb_xhw_arg *xa = &buf->arg;
	int ret;

	buf->issue_irq = 0;
	buf->copy_back = 1;
	xa->op = PSB_XHW_BIN_MEM_INFO;
	xa->arg.bi.pages = pages;

	ret = psb_xhw_add(dev_priv, buf);
	if (ret)
		return ret;

	(void) wait_event_timeout(dev_priv->xhw_caller_queue,
				  atomic_read(&buf->done),
				  DRM_HZ);

	if (!atomic_read(&buf->done))
		return -EBUSY;

	if (!xa->ret) {
		memcpy(hw_cookie, xa->cookie, sizeof(xa->cookie));
		*size = xa->arg.bi.size;
	}
	return xa->ret;
}

int psb_xhw_ta_oom(struct drm_psb_private *dev_priv,
		       struct psb_xhw_buf *buf,
		       uint32_t *cookie)
{
	struct drm_psb_xhw_arg *xa = &buf->arg;

	DRM_ERROR("Binner OOM\n");

	/*
	 * This calls the extensive closed source 
	 * OOM handler, which resolves the condition and
	 * sends a reply telling the scheduler what to do
	 * with the task.
	 */

	buf->issue_irq = 1;
	buf->copy_back = 1;
	xa->op = PSB_XHW_OOM;
	memcpy(xa->cookie, cookie, sizeof(xa->cookie));

	return psb_xhw_add(dev_priv, buf);
}

void psb_xhw_ta_oom_reply(struct drm_psb_private *dev_priv,
			      struct psb_xhw_buf *buf,
			      uint32_t *cookie,
			      uint32_t *bca,
			      uint32_t *rca,
			      uint32_t *flags)
{
	struct drm_psb_xhw_arg *xa = &buf->arg;

	/*
	 * Get info about how to schedule an OOM task.
	 */

	memcpy(cookie, xa->cookie, sizeof(xa->cookie));
	*bca = xa->arg.oom.bca;
	*rca = xa->arg.oom.rca;
	*flags = xa->arg.oom.flags;
}

void psb_xhw_takedown(struct drm_psb_private *dev_priv)
{
}

int psb_xhw_init(struct drm_device *dev)
{
  	struct drm_psb_private *dev_priv = 
		(struct drm_psb_private *) dev->dev_private;

	INIT_LIST_HEAD(&dev_priv->xhw_in);
	dev_priv->xhw_lock = SPIN_LOCK_UNLOCKED;
	atomic_set(&dev_priv->xhw_client, 0);
	init_waitqueue_head(&dev_priv->xhw_queue);
	init_waitqueue_head(&dev_priv->xhw_caller_queue);
	mutex_init(&dev_priv->xhw_mutex);
	dev_priv->xhw_on = 0;

	return 0;
}

static int psb_xhw_init_init(struct drm_psb_private *dev_priv,
			     struct drm_file *file_priv,
			     struct drm_psb_xhw_init_arg *arg)
{
	int ret;
	int is_iomem;

	if (atomic_add_unless(&dev_priv->xhw_client, 1, 1)) {
		unsigned long irq_flags;

		dev_priv->xhw_bo = 
			drm_lookup_buffer_object(file_priv,
						 arg->buffer_handle,
						 1);
		if (!dev_priv->xhw_bo) {
			ret = -EINVAL;
			goto out_err;
		}
		ret = drm_bo_kmap(dev_priv->xhw_bo, 0, 
				  dev_priv->xhw_bo->num_pages,
				  &dev_priv->xhw_kmap);
		if (ret) {
			DRM_ERROR("Failed mapping X server "
				  "communications buffer.\n");
			goto out_err0;
		}
		dev_priv->xhw = drm_bmo_virtual(&dev_priv->xhw_kmap, 
						&is_iomem);
		if (is_iomem) {
			DRM_ERROR("X server communications buffer"
				  "is in device memory.\n");
			ret = -EINVAL;
			goto out_err1;
		}
		dev_priv->xhw_file = file_priv;

		mutex_lock(&dev_priv->xhw_mutex);
		dev_priv->xhw_on = 1;
		mutex_unlock(&dev_priv->xhw_mutex);
		spin_lock_irqsave(&dev_priv->xhw_lock, irq_flags);
		dev_priv->xhw_submit_ok = 1;
		spin_unlock_irqrestore(&dev_priv->xhw_lock, irq_flags);
		return 0;
	} else {
		DRM_ERROR("Xhw is already initialized.\n");
		return -EBUSY;
	}
out_err1:
	dev_priv->xhw = NULL;
	drm_bo_kunmap(&dev_priv->xhw_kmap);
out_err0:
	drm_bo_usage_deref_unlocked(&dev_priv->xhw_bo);
out_err:
	atomic_dec(&dev_priv->xhw_client);
	return ret;
}

static void psb_xhw_queue_empty(struct drm_psb_private *dev_priv)
{
	struct psb_xhw_buf *cur_buf, *next;
	unsigned long irq_flags;

	spin_lock_irqsave(&dev_priv->xhw_lock, irq_flags);
	dev_priv->xhw_submit_ok = 0;

	list_for_each_entry_safe(cur_buf, next, 
				 &dev_priv->xhw_in, head) {
		list_del_init(&cur_buf->head);
		if (cur_buf->copy_back) {
			cur_buf->arg.ret = -EINVAL;
		}
		atomic_set(&cur_buf->done, 1);
	}
	spin_unlock_irqrestore(&dev_priv->xhw_lock, irq_flags);
	wake_up(&dev_priv->xhw_caller_queue);
}

void psb_xhw_init_takedown(struct drm_psb_private *dev_priv,
			   struct drm_file *file_priv,
			   int closing)
{

	if (dev_priv->xhw_file == file_priv && 
	    atomic_add_unless(&dev_priv->xhw_client, -1, 0)) {

		if (closing) 
			psb_xhw_queue_empty(dev_priv);
		else {
			struct psb_xhw_buf buf;

			psb_xhw_terminate(dev_priv, &buf);
			psb_xhw_queue_empty(dev_priv);
		}
		
		dev_priv->xhw = NULL;
		drm_bo_kunmap(&dev_priv->xhw_kmap);
		drm_bo_usage_deref_unlocked(&dev_priv->xhw_bo);
		dev_priv->xhw_file = NULL;
	}
}
	
int psb_xhw_init_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct drm_psb_xhw_init_arg *arg = 
		(struct drm_psb_xhw_init_arg *) data;
	struct drm_psb_private *dev_priv = 
		(struct drm_psb_private *) dev->dev_private;

	switch(arg->operation) {
	case PSB_XHW_INIT:
		return psb_xhw_init_init(dev_priv, file_priv, arg);
	case PSB_XHW_TAKEDOWN:
	        psb_xhw_init_takedown(dev_priv, file_priv, 0);
	}
	return 0;
}


static int psb_xhw_in_empty(struct drm_psb_private *dev_priv)
{
	int empty;
	unsigned long irq_flags;

	spin_lock_irqsave(&dev_priv->xhw_lock, irq_flags);
	empty = list_empty(&dev_priv->xhw_in);
	spin_unlock_irqrestore(&dev_priv->xhw_lock, irq_flags);
	return empty;
}

int psb_xhw_ioctl(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = 
		(struct drm_psb_private *) dev->dev_private;
	struct psb_xhw_buf *buf;
	struct drm_psb_xhw_arg *xa;
	unsigned long irq_flags;
	int ret;
	struct list_head *list;

	if (!dev_priv)
		return -EINVAL;


	if (mutex_lock_interruptible(&dev_priv->xhw_mutex))
		return -EAGAIN;
	
	if (!dev_priv->xhw_on) {
		mutex_unlock(&dev_priv->xhw_mutex);
		return -EINVAL;
	}

	buf = dev_priv->xhw_cur_buf;

	if (buf && buf->copy_back) {
		xa = &buf->arg;
		memcpy(xa, dev_priv->xhw, sizeof(*xa));
		atomic_set(&buf->done, 1);
		if (buf->issue_irq) {
			PSB_WSGX32(_PSB_CE_SW_EVENT, PSB_CR_EVENT_STATUS);
			PSB_RSGX32(PSB_CR_EVENT_STATUS);
		}
		wake_up(&dev_priv->xhw_caller_queue);
	}

	dev_priv->xhw_cur_buf = NULL;

	spin_lock_irqsave(&dev_priv->xhw_lock, irq_flags);
	while (list_empty(&dev_priv->xhw_in)) {
		spin_unlock_irqrestore(&dev_priv->xhw_lock, irq_flags);

		DRM_WAIT_ON(ret, dev_priv->xhw_queue, DRM_HZ,
			    !psb_xhw_in_empty(dev_priv));
		if (ret) {
			mutex_unlock(&dev_priv->xhw_mutex);
			return -EAGAIN;
		}
		spin_lock_irqsave(&dev_priv->xhw_lock, irq_flags);
	}

	list = dev_priv->xhw_in.next;
	list_del_init(list);
	spin_unlock_irqrestore(&dev_priv->xhw_lock, irq_flags);

	buf = list_entry(list, struct psb_xhw_buf, head);
	xa = &buf->arg;
	memcpy(dev_priv->xhw, xa, sizeof(*xa));

	if (unlikely(buf->copy_back))
		dev_priv->xhw_cur_buf = buf;
	else {
		atomic_set(&buf->done, 1);
		dev_priv->xhw_cur_buf = NULL;
	}

	if (xa->op == PSB_XHW_TERMINATE) {
		dev_priv->xhw_on = 0;
		wake_up(&dev_priv->xhw_caller_queue);
	}		

	mutex_unlock(&dev_priv->xhw_mutex);

	return 0;
}

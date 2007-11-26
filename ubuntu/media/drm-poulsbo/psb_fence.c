/**************************************************************************
 * 
 * Copyright 2006-2007 Tungsten Graphics, Inc., Cedar Park, TX., USA
 * Copyright (c) Intel Corp. 2007.
 * All Rights Reserved.
 * 
 * Intel funded Tungsten Graphics (http://www.tungstengraphics.com) to
 * develop this code.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * 
 **************************************************************************/
/*
 * Authors: Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 */

#include "drmP.h"
#include "psb_drv.h"

static void psb_flush_ta_fence(struct psb_scheduler *scheduler,
			       struct drm_fence_class_manager *fc)
{
	uint32_t cur_flag = 1;
	uint32_t flags = 0;
	uint32_t sequence = 0;
	struct psb_scheduler_seq *seq = scheduler->seq;
	uint32_t mask;
	uint32_t remaining = 0xFFFFFFFF;

	if (fc->pending_exe_flush) {
		if (!seq->reported) {
			drm_fence_handler(scheduler->dev, PSB_ENGINE_TA, 
					  seq->sequence, DRM_FENCE_TYPE_EXE, 0);
			seq->reported = 1;
		}
	}

	mask = fc->pending_flush;

	while (mask & remaining) {
		if (!(mask & cur_flag))
			goto skip;
		if (seq->reported)
		  goto skip;
		if (flags == 0)
			sequence = seq->sequence;
		else if (sequence != seq->sequence) {
			drm_fence_handler(scheduler->dev, PSB_ENGINE_TA, sequence, 
					  flags, 0);
			sequence = seq->sequence;
			flags = 0;
		}
		flags |= cur_flag;
		seq->reported = 1;
	skip:
		mask = fc->pending_flush;
		cur_flag <<= 1;
		remaining <<= 1;
		seq++;
	}

	if (flags) 
		drm_fence_handler(scheduler->dev, PSB_ENGINE_TA, sequence, 
				  flags, 0);	
}

static void psb_perform_flush(struct drm_device * dev, u32 class)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->class[class];
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	uint32_t diff;
	uint32_t sequence;

	if (!dev_priv)
		return;
	
	/*PSB_DEBUG_GENERAL("MSVDXFENCE: pending_exe_flush=%d last_exe_flush=%d sequence=%d wrap_diff=%d\n", 
		fc->pending_exe_flush, fc->last_exe_flush, sequence, driver->wrap_diff);*/

	if (class == PSB_ENGINE_TA) {
		psb_flush_ta_fence(&dev_priv->scheduler, fc);
		return;
	}

	if (fc->pending_exe_flush) {
	        if(class == PSB_ENGINE_VIDEO)
			sequence = dev_priv->msvdx_current_sequence;
		else
			sequence = dev_priv->comm[class << 4];

		/*
		 * First update fences with the current breadcrumb.
		 */

		diff = sequence - fc->last_exe_flush;
		if (diff < driver->wrap_diff && diff != 0) {
		        drm_fence_handler(dev, class, sequence, 
					  DRM_FENCE_TYPE_EXE, 0);
		}

		switch(class)
		{
		case PSB_ENGINE_2D:
			if (dev_priv->fence0_irq_on && !fc->pending_exe_flush) {
			psb_2D_irq_off(dev_priv);
			dev_priv->fence0_irq_on = 0;
			} else if (!dev_priv->fence0_irq_on && fc->pending_exe_flush) {
			psb_2D_irq_on(dev_priv);
			dev_priv->fence0_irq_on = 1;
			}
			break;
		case PSB_ENGINE_VIDEO:
		/*TBD fix this for video...!!!*/
		if (dev_priv->fence2_irq_on && !fc->pending_exe_flush) {
/*			psb_msvdx_irq_off(dev_priv);*/
			dev_priv->fence2_irq_on = 0;
			} else if (!dev_priv->fence2_irq_on && fc->pending_exe_flush) {
/*			psb_msvdx_irq_on(dev_priv);*/
			dev_priv->fence2_irq_on = 1;
			}
			break;
		default:
			return;
		}		
	}
}

void psb_fence_error(struct drm_device *dev,
		     uint32_t class,
		     uint32_t sequence,
		     uint32_t type,
		     int error)
{
	struct drm_fence_manager *fm = &dev->fm;
	unsigned long irq_flags;

  	BUG_ON(class >= PSB_NUM_ENGINES);
	write_lock_irqsave(&fm->lock, irq_flags);
	drm_fence_handler(dev, class, sequence, type, error);
	write_unlock_irqrestore(&fm->lock, irq_flags);
}

void psb_poke_flush(struct drm_device * dev, uint32_t class)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	unsigned long flags;

	BUG_ON(class >= PSB_NUM_ENGINES);
	spin_lock_irqsave(&dev_priv->scheduler.lock, flags);
	write_lock(&fm->lock);
	psb_perform_flush(dev, class);
	write_unlock(&fm->lock);
	spin_unlock_irqrestore(&dev_priv->scheduler.lock, flags);
}

int psb_fence_emit_sequence(struct drm_device * dev, uint32_t class, uint32_t flags,
			    uint32_t * sequence, uint32_t * native_type)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;
	int ret;

	if (!dev_priv)
		return -EINVAL;
	
	if (class >= PSB_NUM_ENGINES)
		return -EINVAL;

	switch(class)
	{
	  case PSB_ENGINE_2D:
		++dev_priv->sequence[class];
		ret = psb_blit_sequence(dev_priv);
		if (ret)
			return ret;
		break;
	  case PSB_ENGINE_VIDEO: /*MSVDX*/
		++dev_priv->sequence[class];
		break;
	}

	*sequence = dev_priv->sequence[class];
	*native_type = DRM_FENCE_TYPE_EXE;

	return 0;
}

uint32_t psb_fence_advance_sequence(struct drm_device *dev, uint32_t class)
{
	struct drm_psb_private *dev_priv = 
		(struct drm_psb_private *) dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	unsigned long flags;
	uint32_t sequence;

	write_lock_irqsave(&fm->lock, flags);
	sequence = ++dev_priv->sequence[class];
	write_unlock_irqrestore(&fm->lock, flags);
	return sequence;
}

void psb_fence_handler(struct drm_device * dev, uint32_t class)
{
	struct drm_fence_manager *fm = &dev->fm;

	write_lock(&fm->lock);
	psb_perform_flush(dev, class);
	write_unlock(&fm->lock);
}


int psb_fence_has_irq(struct drm_device * dev, uint32_t class, uint32_t flags)
{
	/*
	 * We have an irq that tells us when we have a new breadcrumb,
	 */

  if (((class == PSB_ENGINE_2D) ||
       (class == PSB_ENGINE_RASTERIZER) ||
       (class == PSB_ENGINE_TA) ||
	   (class == PSB_ENGINE_VIDEO)))
      return 1;

	return 0;
}

int psb_fence_timeout(struct drm_fence_object *fence)
{
	int ret = 0;
	struct drm_device *dev = fence->dev;
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;

	switch(fence->class)
	{
	case PSB_ENGINE_VIDEO:
		PSB_DEBUG_GENERAL("MSVDXFENCE: need to reset MSVDX hardware.....\n");
		dev_priv->msvdx_needs_reset = 1;
		if (dev_priv->msvdx_current_sequence < dev_priv->sequence[fence->class])
		{			
			dev_priv->msvdx_current_sequence++;

			PSB_DEBUG_GENERAL("MSVDXFENCE: incremented msvdx_current_sequence to :%d for faulting fence:0x%x, handle:0x%08lx\n", 
				dev_priv->msvdx_current_sequence, fence, fence->base.hash.key);
		}
		psb_fence_error(dev,PSB_ENGINE_VIDEO, dev_priv->msvdx_current_sequence, DRM_FENCE_TYPE_EXE, DRM_CMD_HANG);
		/*ret = -EAGAIN;*/
		ret = -EBUSY;
		break;
	case PSB_ENGINE_2D:
		break;
	default:
		break;
	}
	return ret;
}

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
 * Authors: Thomas Hellstrom <thomas-at-tungstengraphics.com>
 */

#include "drmP.h"
#include "psb_drv.h"
#include "psb_scene.h"


static int psb_clear_scene(struct psb_scene *scene)
{
	struct drm_bo_kmap_obj bmo;
	int is_iomem;
	void *addr;

	int ret = drm_bo_kmap(scene->hw_data, scene->clear_p_start,
			      scene->clear_num_pages, &bmo);

	PSB_DEBUG_RENDER("Scene clear\n");
	if (ret)
		return ret;
	
	addr = drm_bmo_virtual(&bmo, &is_iomem);
	BUG_ON(is_iomem);
	memset(addr, 0, scene->clear_num_pages << PAGE_SHIFT);
	drm_bo_kunmap(&bmo);
	
	return 0;
}

static void psb_destroy_scene_devlocked(struct psb_scene *scene)
{
	if (!scene)
		return;

	PSB_DEBUG_RENDER("Scene destroy\n");
	drm_bo_usage_deref_locked(&scene->hw_data);
	drm_free(scene, sizeof(*scene), DRM_MEM_DRIVER);
}

void psb_scene_unref_devlocked(struct psb_scene ** scene)
{
	struct psb_scene *tmp_scene = *scene;

	PSB_DEBUG_RENDER("Scene unref\n");
	*scene = NULL;
	if (atomic_dec_and_test(&tmp_scene->ref_count))
		psb_destroy_scene_devlocked(tmp_scene);
}

struct psb_scene *psb_scene_ref(struct psb_scene *src)
{
	PSB_DEBUG_RENDER("Scene ref\n");
	atomic_inc(&src->ref_count);
	return src;
}

static struct psb_scene *psb_alloc_scene(struct drm_device *dev, 
					 uint32_t w, uint32_t h)
{
	struct drm_psb_private *dev_priv = 
		(struct drm_psb_private *)dev->dev_private;
	int ret = -EINVAL;
	struct psb_scene *scene;
	uint32_t bo_size;
	struct psb_xhw_buf buf;

	PSB_DEBUG_RENDER("Alloc scene w %u h %u\n", w, h);

	scene = drm_calloc(1, sizeof(*scene), DRM_MEM_DRIVER);
	
	if (!scene) {
		DRM_ERROR("Out of memory allocating scene object.\n");
		return NULL;
	}

	scene->dev = dev;
	scene->w = w;
	scene->h = h;
	scene->hw_scene = NULL;
	atomic_set(&scene->ref_count, 1);

	ret = psb_xhw_scene_info(dev_priv, &buf, scene->w, scene->h,
				 scene->hw_cookie, &bo_size,
				 &scene->clear_p_start,
				 &scene->clear_num_pages);
	if (ret)
		goto out_err;
	
	ret = drm_buffer_object_create(dev, bo_size, drm_bo_type_kernel, 
				       DRM_PSB_FLAG_MEM_MMU | 
				       DRM_BO_FLAG_READ | 
				       PSB_BO_FLAG_SCENE |
				       DRM_BO_FLAG_WRITE, 
				       DRM_BO_HINT_DONT_FENCE,
				       0, 0, &scene->hw_data);
	if (ret)
		goto out_err;
	
	return scene;
out_err:
	drm_free(scene, sizeof(*scene), DRM_MEM_DRIVER);
	return NULL;
}


int psb_validate_scene_pool(struct psb_scene_pool *pool, uint64_t flags,
			    uint64_t mask, 
			    uint32_t hint,
			    uint32_t w,
			    uint32_t h,
			    int final_pass,
			    struct psb_scene **scene_p)
{
	struct drm_device *dev = pool->dev;
	struct drm_psb_private *dev_priv = 
		(struct drm_psb_private *)dev->dev_private;
	struct psb_scene *scene = pool->scenes[pool->cur_scene];
	int ret;
	unsigned long irq_flags;
	struct psb_scheduler *scheduler = &dev_priv->scheduler;

	PSB_DEBUG_RENDER("Validate scene pool. Scene %u\n",
			 pool->cur_scene);
	pool->w = w;
	pool->h = h;
	if (scene && (scene->w != pool->w || scene->h != pool->h)) {
		spin_lock_irqsave(&scheduler->lock, irq_flags);
		if (scene->flags & PSB_SCENE_FLAG_DIRTY) {
			spin_unlock_irqrestore(&scheduler->lock, irq_flags);
			DRM_ERROR("Trying to resize a dirty scene.\n");
			return -EINVAL;
		}
		spin_unlock_irqrestore(&scheduler->lock, irq_flags);
		mutex_lock(&dev->struct_mutex);
		psb_scene_unref_devlocked(&pool->scenes[pool->cur_scene]);
		mutex_unlock(&dev->struct_mutex);
		scene = NULL;
	}

	if (!scene) {
		pool->scenes[pool->cur_scene] = scene = 
			psb_alloc_scene(pool->dev, pool->w, pool->h);		

		if (!scene)
			return -ENOMEM;

		scene->flags = PSB_SCENE_FLAG_CLEARED;
	}

	/*
	 * FIXME: We need atomic bit manipulation here for the
	 * scheduler. For now use the spinlock.
	 */

	spin_lock_irqsave(&scheduler->lock, irq_flags);
	if (!(scene->flags & PSB_SCENE_FLAG_CLEARED)) {
		spin_unlock_irqrestore(&scheduler->lock, irq_flags);
		PSB_DEBUG_RENDER("Waiting to clear scene memory.\n");
		ret = drm_bo_wait(scene->hw_data, 0, 0, 0);
		if (ret)
			return ret;

		ret = psb_clear_scene(scene);

		if (ret)
			return ret;
		spin_lock_irqsave(&scheduler->lock, irq_flags);
		scene->flags |= PSB_SCENE_FLAG_CLEARED;
	}
	spin_unlock_irqrestore(&scheduler->lock, irq_flags);

	ret = drm_bo_do_validate(scene->hw_data, flags, mask, hint, 
				 PSB_ENGINE_TA, 0, NULL);

	if (ret)
		return ret;
	
	if (final_pass) {

		/* 
		 * Clear the scene on next use. Advance the scene counter.
		 */

	        spin_lock_irqsave(&scheduler->lock, irq_flags);
		scene->flags &= ~PSB_SCENE_FLAG_CLEARED;
	        spin_unlock_irqrestore(&scheduler->lock, irq_flags);		
		pool->cur_scene = (pool->cur_scene + 1) % 
			pool->num_scenes;
	}

	*scene_p = psb_scene_ref(scene);
	return 0;
}

static void psb_scene_pool_destroy_devlocked(struct psb_scene_pool *pool)
{
	int i;

	if (!pool)
		return;

	PSB_DEBUG_RENDER("Scene pool destroy.\n");
	for (i=0; i<pool->num_scenes; ++i) {
		PSB_DEBUG_RENDER("scenes %d is 0x%08lx\n", i, 
				 (unsigned long)pool->scenes[i]);
		if (pool->scenes[i]) 			
			psb_scene_unref_devlocked(&pool->scenes[i]);
	}
	drm_free(pool, sizeof(*pool), DRM_MEM_DRIVER);
}


void psb_scene_pool_unref_devlocked(struct psb_scene_pool **pool)
{
	struct psb_scene_pool *tmp_pool = *pool;
	struct drm_device *dev = tmp_pool->dev;

	PSB_DEBUG_RENDER("Scene pool unref\n");
	(void) dev;
	DRM_ASSERT_LOCKED(&dev->struct_mutex);
	*pool = NULL;
	if (--tmp_pool->ref_count == 0) 
		psb_scene_pool_destroy_devlocked(tmp_pool);
}

struct psb_scene_pool *psb_scene_pool_ref_devlocked(struct psb_scene_pool *src)
{
	++src->ref_count;
	return src;
}

/*
 * Callback for user object manager.
 */

static void psb_scene_pool_destroy(struct drm_file *priv, 
				   struct drm_user_object *base)
{
	struct psb_scene_pool *pool = 
		drm_user_object_entry(base, struct psb_scene_pool, user);
	
	PSB_DEBUG_RENDER("User scene pool deref\n");
	psb_scene_pool_unref_devlocked(&pool);
}

struct psb_scene_pool *psb_scene_pool_lookup_devlocked(struct drm_file *priv,
						       uint32_t handle, 
						       int check_owner)
{
	struct drm_user_object *uo;
	struct psb_scene_pool *pool;
	
	uo = drm_lookup_user_object(priv, handle);
	if (!uo || (uo->type != PSB_USER_OBJECT_SCENE_POOL)) {
		DRM_ERROR("Could not find scene pool object 0x%08x\n", handle);
		return NULL;
	}

	if (check_owner && priv != uo->owner) {
		if (!drm_lookup_ref_object(priv, uo, _DRM_REF_USE))
			return NULL;
	}
	
	pool = drm_user_object_entry(uo, struct psb_scene_pool, user);
	return psb_scene_pool_ref_devlocked(pool);
}
	
					     

struct psb_scene_pool *psb_scene_pool_alloc(struct drm_file *priv, 
					    int shareable,
					    uint32_t num_scenes,
					    uint32_t w, uint32_t h)
{
	struct drm_device *dev = priv->head->dev;
	struct psb_scene_pool *pool;
	int ret;

	PSB_DEBUG_RENDER("Scene pool alloc\n");
	pool = drm_calloc(1, sizeof(*pool), DRM_MEM_DRIVER);
	if (!pool) {
		DRM_ERROR("Out of memory allocating scene pool object.\n");
		return NULL;
	}
	pool->w = w;
	pool->h = h;
	pool->dev = dev;
	pool->num_scenes = num_scenes;

	mutex_lock(&dev->struct_mutex);
	ret = drm_add_user_object(priv, &pool->user, shareable);
	if (ret) 
		goto out_err;

	pool->user.type = PSB_USER_OBJECT_SCENE_POOL;
	pool->user.remove = &psb_scene_pool_destroy;
	pool->ref_count = 2;
	mutex_unlock(&dev->struct_mutex);
	return pool;
out_err:
	drm_free(pool, sizeof(*pool), DRM_MEM_DRIVER);
	return NULL;
}

#if 0

/*
 * Code to support multiple TA memory buffers.
 * Not used yet.
 */

	
static void psb_destroy_bin_mem_devlocked(struct psb_bin_mem *bin_mem)
{
	if (!bin_mem)
		return;

	drm_bo_usage_deref_locked(&bin_mem->hw_data);
	drm_bo_usage_deref_locked(&bin_mem->bin_memory);
	drm_free(bin_mem, sizeof(*bin_mem), DRM_MEM_DRIVER);
}
	
void psb_bin_mem_unref_devlocked(struct psb_bin_mem ** bin_mem)
{
	struct psb_bin_mem *tmp_bin_mem = *bin_mem;
	struct drm_device *dev = tmp_bin_mem->dev;

	(void) dev;
	DRM_ASSERT_LOCKED(&dev->struct_mutex);
	*bin_mem = NULL;
	if (--tmp_bin_mem->ref_count == 0) 
		psb_destroy_bin_mem_devlocked(tmp_bin_mem);
}

void psb_bin_mem_ref_devlocked(struct psb_bin_mem **dst, struct psb_bin_mem *src)
{
	struct drm_device *dev = src->dev;

	(void) dev;
	DRM_ASSERT_LOCKED(&dev->struct_mutex);
	*dst = src;
	++src->ref_count;
}

/*
 * Callback for user object manager.
 */

static void psb_bin_mem_object_destroy(struct drm_file *priv, struct drm_user_object *base)
{
	struct psb_bin_mem *bin_mem = 
		drm_user_object_entry(base, struct psb_bin_mem, user);
	
	psb_bin_mem_unref_devlocked(&bin_mem);
}


struct psb_bin_mem *psb_alloc_bin_mem(struct drm_file *priv, int shareable,
				      uint64_t pages)
{
	struct drm_device *dev = priv->head->dev;
	struct drm_psb_private *dev_priv = 
		(struct drm_psb_private *)dev->dev_private;
	int ret = -EINVAL;
	struct psb_bin_mem *bin_mem;
	uint32_t bo_size;


	bin_mem = drm_calloc(1, sizeof(*bin_mem), DRM_MEM_DRIVER);

	if (!bin_mem) {
		DRM_ERROR("Out of memory allocating bin memory.\n");
		return NULL;
	}

	ret = psb_xhw_bin_mem_info(dev_priv, &buf, pages, 
				   bin_mem->hw_cookie, &bo_size);
	if (ret)
		goto out_err0;

	bin_mem->dev = dev;
	ret = drm_buffer_object_create(dev, bo_size, drm_bo_type_kernel, 
				       DRM_PSB_FLAG_MEM_MMU | DRM_BO_FLAG_READ |
				       DRM_BO_FLAG_WRITE, DRM_BO_HINT_DONT_FENCE,
				       0, 0, &bin_mem->hw_data);
	if (ret)
		goto out_err0;

	ret = drm_buffer_object_create(dev, pages << PAGE_SHIFT, drm_bo_type_kernel, 
				       DRM_PSB_FLAG_MEM_RASTGEOM | DRM_BO_FLAG_READ |
				       DRM_BO_FLAG_WRITE, DRM_BO_HINT_DONT_FENCE,
				       0, 0, &bin_mem->bin_memory);
	if (ret)
		goto out_err1;

	mutex_lock(&dev->struct_mutex);
	ret = drm_add_user_object(priv, &bin_mem->user, shareable);
	bin_mem->ref_count = 1;
	if (ret) {
		psb_destroy_bin_mem_devlocked(bin_mem);		
		bin_mem = NULL;
	}
	mutex_unlock(&dev->struct_mutex);


	return bin_mem;
out_err0:
	drm_bo_usage_deref_unlocked(&bin_mem->hw_data);
out_err1:
	drm_free(bin_mem, sizeof(*bin_mem), DRM_MEM_DRIVER);
	return NULL;
}

#endif

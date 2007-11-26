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

#ifndef _PSB_SCENE_H_
#define _PSB_SCENE_H_

#define PSB_USER_OBJECT_SCENE_POOL    drm_driver_type0
#define PSB_USER_OBJECT_BIN_MEM       drm_driver_type1
#define PSB_MAX_NUM_SCENES            8

struct psb_hw_scene;
struct psb_hw_bin_mem;

struct psb_scene_pool{
	struct drm_device *dev;
	struct drm_user_object user;
	uint32_t ref_count;
	uint32_t w;
	uint32_t h;
	uint32_t cur_scene;
	struct psb_scene *scenes[PSB_MAX_NUM_SCENES];
        uint32_t num_scenes;
};
	

struct psb_scene{
	struct drm_device *dev;
	atomic_t ref_count;
	uint32_t hw_cookie[PSB_SCENE_HW_COOKIE_SIZE];
	uint32_t bo_size;
	uint32_t w;
	uint32_t h;
	struct psb_bin_mem *bin_mem;
	struct psb_hw_scene *hw_scene;
	struct drm_buffer_object *hw_data;
	uint32_t flags;
        uint32_t clear_p_start;
        uint32_t clear_num_pages;
};


struct psb_scene_entry {
	struct list_head head;
	struct psb_scene *scene;
};

struct psb_user_scene{
	struct drm_device *dev;
	struct drm_user_object user;
};
	

struct psb_bin_mem {
	struct drm_device *dev;
	struct drm_user_object user;
	uint32_t ref_count;
	uint32_t hw_cookie[PSB_BIN_MEM_HW_COOKIE_SIZE];
	uint32_t bo_size;
	struct drm_buffer_object *bin_memory;
	struct drm_buffer_object *hw_data;
	int is_deallocating;
	int deallocating_scheduled;
};

extern struct psb_scene_pool *psb_scene_pool_alloc(struct drm_file *priv, 
						   int shareable,
						   uint32_t num_scenes,
						   uint32_t w, uint32_t h);
extern void psb_scene_pool_unref_devlocked(struct psb_scene_pool **pool);
extern struct psb_scene_pool *psb_scene_pool_lookup_devlocked(struct drm_file *priv,
							      uint32_t handle, 
							      int check_owner);
extern int psb_validate_scene_pool(struct psb_scene_pool *pool, 
				   uint64_t flags,
				   uint64_t mask, 
				   uint32_t hint,
				   uint32_t w,
				   uint32_t h,
				   int final_pass,
				   struct psb_scene **scene_p);
extern void psb_scene_unref_devlocked(struct psb_scene ** scene);
extern struct psb_scene *psb_scene_ref(struct psb_scene *src);


static inline uint32_t psb_scene_pool_handle(struct psb_scene_pool *pool)
{
	return pool->user.hash.key;
}



#endif

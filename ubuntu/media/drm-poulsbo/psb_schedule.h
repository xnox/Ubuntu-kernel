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

#ifndef _PSB_SCHEDULE_H_
#define _PSB_SCHEDULE_H_

#include "drmP.h"

enum psb_task_type {
	psb_ta_midscene_task,
	psb_ta_task,
	psb_raster_task,
	psb_freescene_task
};


#define PSB_MAX_TA_CMDS 48
#define PSB_MAX_RASTER_CMDS 48

struct psb_xhw_buf {
	struct list_head head;
	int issue_irq;
	int copy_back;
	atomic_t done;
	struct drm_psb_xhw_arg arg;
	
};

struct psb_task {
	enum psb_task_type task_type;
        uint32_t engine;
	uint32_t sequence;
	struct list_head head;
	struct psb_scene *scene;
	uint32_t ta_cmds[PSB_MAX_TA_CMDS];
        uint32_t raster_cmds[PSB_MAX_RASTER_CMDS];
	uint32_t ta_cmd_size;
	uint32_t raster_cmd_size;

        struct drm_buffer_object *scene_buffer;
	uint32_t ta_complete_action;
	uint32_t raster_complete_action;
	uint32_t hw_cookie;
	uint32_t flags;
        struct psb_xhw_buf buf;
};

struct psb_hw_scene{
	struct list_head head;
	uint32_t context_number;

        /*
	 * This pointer does not refcount the last_scene_buffer,
	 * so we must make sure it is set to NULL before destroying
	 * the corresponding task.
	 */

	struct psb_scene *last_scene;
};

struct psb_scene;
struct drm_psb_private;

struct psb_scheduler_seq {
	uint32_t sequence;
	int reported;
};

struct psb_scheduler {
	struct drm_device *dev;
        struct psb_scheduler_seq seq[_PSB_ENGINE_TA_FENCE_TYPES];
	struct psb_hw_scene hs[PSB_NUM_HW_SCENES];
        struct mutex lp_mutex;
        struct mutex hp_mutex;
	spinlock_t lock;
	struct list_head hw_scenes;
	struct list_head ta_queue;
	struct list_head raster_queue;
	struct list_head hp_raster_queue;
        struct list_head task_done_queue;
	struct psb_task *current_task[PSB_SCENE_NUM_ENGINES];
	int ta_state;
	struct psb_hw_scene *pending_hw_scene;
	uint32_t pending_hw_scene_seq;
        struct work_struct wq;
        struct psb_scene_pool *pool;
	uint32_t idle_count;
	int idle;
        wait_queue_head_t idle_queue;
        unsigned long ta_end_jiffies;
        unsigned long raster_end_jiffies;
};

extern struct psb_scene_pool *psb_alloc_scene_pool(struct drm_file *priv, int shareable,
				  uint32_t w, uint32_t h);
extern uint32_t psb_scene_handle(struct psb_scene *scene);
extern int psb_scheduler_init(struct drm_device *dev, struct psb_scheduler *scheduler);
extern void psb_scheduler_takedown(struct psb_scheduler *scheduler);
extern int psb_cmdbuf_ta(struct drm_file *priv,
			     struct drm_psb_cmdbuf_arg *arg,
			     struct drm_buffer_object *cmd_buffer,
			     struct drm_buffer_object *ta_buffer,
			     struct psb_scene *scene,
			     struct drm_fence_arg *fence_arg);
extern int psb_cmdbuf_raster(struct drm_file *priv,
			     struct drm_psb_cmdbuf_arg *arg,
			     struct drm_buffer_object *cmd_buffer,
			     struct drm_fence_arg *fence_arg);
extern void psb_scheduler_handler(struct drm_psb_private *dev_priv, uint32_t status);
extern void psb_scheduler_pause(struct drm_psb_private *dev_priv);
extern void psb_scheduler_restart(struct drm_psb_private *dev_priv);
extern int psb_scheduler_idle(struct drm_psb_private *dev_priv);
void psb_scheduler_lockup(struct drm_psb_private *dev_priv,
			  int *lockup, int *msvdx_lockup,
			  int *idle, int* msvdx_idle);
extern void psb_scheduler_reset(struct drm_psb_private *dev_priv,
				int error_condition);
#endif

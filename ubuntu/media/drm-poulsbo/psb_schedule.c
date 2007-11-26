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
#include "psb_drm.h"
#include "psb_drv.h"
#include "psb_reg.h"
#include "psb_scene.h"

#define PSB_RASTER_TIMEOUT (DRM_HZ / 10)
#define PSB_TA_TIMEOUT (DRM_HZ / 10)

#undef PSB_SOFTWARE_WORKAHEAD

#ifdef PSB_STABLE_SETTING

/*
 * Stable setting that gives around 280 fps.
 * Software blocks completely while the engines are working so there can be no 
 * overlap. Similar to noEDM path.
 */

#define PSB_WAIT_FOR_RASTER_COMPLETION
#define PSB_WAIT_FOR_TA_COMPLETION

#elif defined(PSB_PARANOID_SETTING)
/*
 * Stable setting that gives around 270 fps.
 * Software blocks "almost" while the engines are working so there can be no 
 * overlap. Similar to noEDM path.
 */

#define PSB_WAIT_FOR_RASTER_COMPLETION
#define PSB_WAIT_FOR_TA_COMPLETION
#define PSB_BE_PARANOID

#elif defined(PSB_SOME_OVERLAP_BUT_LOCKUP)
/*
 * Unstable setting that gives around 360 fps.
 * Software leaps ahead while the rasterizer is running and prepares
 * a new ta job that can be scheduled before the rasterizer has
 * finished.
 */

#define PSB_WAIT_FOR_TA_COMPLETION

#elif defined(PSB_SOFTWARE_WORKAHEAD)
/*
 * Don't sync, but allow software to work ahead. and queue a number of jobs.
 * But block overlapping in the scheduler. This should really work, unless
 * we're overwriting buffers or something else is disturbing the 
 * engine while it's running. I think the key to get overlapped rendering working
 * is to make this mode work. One could implement the stable setting in userspace 
 * by waiting on the fences after 3D flush outside of the HW lock, then 
 * Test with 2 or more apps. If it works and can overlap, we're indeed doing
 * something wrong with our buffers. Also have a look at the USE base addr
 * allocation.
 */

#define PSB_BLOCK_OVERLAP
#define ONLY_ONE_JOB_IN_RASTER_QUEUE

#endif
#define ONLY_ONE_JOB_IN_RASTER_QUEUE


void psb_scheduler_lockup(struct drm_psb_private *dev_priv,
			  int *lockup, int *msvdx_lockup,
			  int *idle, int* msvdx_idle)
{
	unsigned long irq_flags;
	struct psb_scheduler *scheduler = &dev_priv->scheduler;
	*lockup = 0;
	*msvdx_lockup = 0;
	*idle = 1;
	*msvdx_idle = 1;

	spin_lock_irqsave(&scheduler->lock, irq_flags);

	if (scheduler->current_task[PSB_SCENE_ENGINE_TA] != NULL &&
	    time_after_eq(jiffies, scheduler->ta_end_jiffies)) {
		*lockup = 1;
	}
	if (!lockup && (scheduler->current_task[PSB_SCENE_ENGINE_RASTER] != NULL ||
	     scheduler->pending_hw_scene != NULL) &&
	    time_after_eq(jiffies, scheduler->raster_end_jiffies)) {
		*lockup = 1;
	}
	if(!lockup)
		*idle = scheduler->idle;

	 if (!dev_priv->has_msvdx) {
                *msvdx_idle = 1;
                *msvdx_lockup = 0;
                goto out;
        }

	/*
	PSB_DEBUG_GENERAL("MSVDXTimer: current_sequence:%d last_sequence:%d and last_submitted_sequence :%d\n",
		dev_priv->msvdx_current_sequence, dev_priv->msvdx_last_sequence, dev_priv->sequence[PSB_ENGINE_VIDEO]);
	*/

	if (dev_priv->sequence[PSB_ENGINE_VIDEO] > dev_priv->msvdx_current_sequence) 
	{
		if (dev_priv->msvdx_current_sequence == dev_priv->msvdx_last_sequence)
		{
			PSB_DEBUG_GENERAL("MSVDXTimer: msvdx locked-up for sequence:%d\n", 
				dev_priv->msvdx_current_sequence);
			*msvdx_lockup = 1;
		}
		else
		{
			PSB_DEBUG_GENERAL("MSVDXTimer: msvdx responded fine so far...\n");
			dev_priv->msvdx_last_sequence = dev_priv->msvdx_current_sequence;
			*msvdx_idle = 0;
		}
	}
out:	
	spin_unlock_irqrestore(&scheduler->lock, irq_flags);
}


static inline void psb_set_idle(struct psb_scheduler *scheduler)
{
	scheduler->idle = 
		(scheduler->current_task[PSB_SCENE_ENGINE_RASTER] == NULL) &&
		(scheduler->current_task[PSB_SCENE_ENGINE_TA] == NULL) &&
		scheduler->pending_hw_scene == NULL;
	
	if (scheduler->idle)
		wake_up(&scheduler->idle_queue);
}


/*
 * Call with the scheduler spinlock held.
 * Assigns a scene context to either the ta or the rasterizer, 
 * flushing out other scenes to memory if necessary.
 */


static int psb_set_scene_fire(struct psb_scheduler *scheduler,
			      struct psb_scene *scene, 
			      int engine,
			      struct psb_task *task)
{
	uint32_t flags = 0;
	struct psb_hw_scene *hw_scene;
	struct drm_device *dev = scene->dev;
	struct drm_psb_private *dev_priv = 
		(struct drm_psb_private *) dev->dev_private;

	hw_scene = scene->hw_scene;
	if (engine == PSB_SCENE_ENGINE_RASTER && 
	    hw_scene && hw_scene->last_scene == scene) {

		/*
		 * Reuse the last hw scene context and delete it from the 
		 * free list.
		 */
		
		PSB_DEBUG_RENDER("Reusing hw scene %d.\n", 
				 hw_scene->context_number);
		if (scene->flags & PSB_SCENE_FLAG_DIRTY) {

			/*
			 * No hw context initialization to be done.
			 */
			
			flags |= PSB_SCENE_FLAG_SETUP_ONLY;
		}

		list_del_init(&hw_scene->head);

	} else {
		struct list_head *list;
		hw_scene = NULL;

		/*
		 * Grab a new hw scene context.
		 */

		list_for_each(list, &scheduler->hw_scenes) {
			hw_scene = list_entry(list, struct psb_hw_scene, head);
			break;
		}
		BUG_ON(!hw_scene);
	        PSB_DEBUG_RENDER("New hw scene %d.\n", 
				 hw_scene->context_number);

		list_del_init(list);
	}

	scene->hw_scene = hw_scene;

	if (engine == PSB_SCENE_ENGINE_TA)
		hw_scene->last_scene = scene;

	flags |= PSB_SCENE_FLAG_SETUP; 

	/*
	 * Switch context and setup the engine.
	 */

	return psb_xhw_scene_bind_fire(dev_priv,
				       &task->buf,
				       task->flags,
				       hw_scene->context_number,
				       scene->hw_cookie, 
				       scene->hw_data->offset,
				       engine,
				       flags | scene->flags);
}

static inline void psb_report_fence(struct psb_scheduler *scheduler,
				    uint32_t class,
				    uint32_t sequence,
				    uint32_t type,
	                            int call_handler)
{
	struct psb_scheduler_seq *seq = &scheduler->seq[type];
	
	seq->sequence = sequence;
	seq->reported = 0;
	if (call_handler)
		psb_fence_handler(scheduler->dev, class);
}

static void psb_schedule_raster(struct drm_psb_private *dev_priv,
				struct psb_scheduler *scheduler);

static void psb_schedule_ta(struct drm_psb_private *dev_priv,
				struct psb_scheduler *scheduler)
{
	struct psb_task *task = NULL;
	struct list_head *list, *next;
	int pushed_raster_task = 0;

	PSB_DEBUG_RENDER("schedule ta\n");

	if (scheduler->idle_count != 0)
		return;

	if (scheduler->current_task[PSB_SCENE_ENGINE_TA] != NULL)
		return;

	/*
	 * Skip the ta stage for rasterization-only 
	 * tasks. They arrive here to make sure we're rasterizing
	 * tasks in the correct order.
	 */

	list_for_each_safe(list, next, &scheduler->ta_queue) {
		task = list_entry(list, struct psb_task, head);
		if (task->task_type != psb_raster_task)
			break;
		
		list_del_init(list);
		list_add_tail(list, &scheduler->raster_queue);
		psb_report_fence(scheduler, task->engine, task->sequence,
				 _PSB_FENCE_TA_DONE_SHIFT, 1);
		task = NULL;
		pushed_raster_task = 1;
	}

	if (pushed_raster_task)
		psb_schedule_raster(dev_priv, scheduler);

	if (!task)
		return;

	if (scheduler->ta_state)
		return;

#ifdef ONLY_ONE_JOB_IN_RASTER_QUEUE

	/*
	 * Block ta from trying to use both hardware contexts
	 * without the rasterizer starting to render from one of them.
	 */

	if (!list_empty(&scheduler->raster_queue)) {
		return;
	}
#endif

#ifdef PSB_BLOCK_OVERLAP
	/*
	 * Make sure rasterizer isn't doing anything.
	 */
	if (scheduler->current_task[PSB_SCENE_ENGINE_RASTER] != NULL ||
	    scheduler->pending_hw_scene != NULL)
		return;
#endif
	if (list_empty(&scheduler->hw_scenes))
	        return;

	list_del_init(&task->head);
	scheduler->current_task[PSB_SCENE_ENGINE_TA] = task;
#if 0
	if (scheduler->current_task[PSB_SCENE_ENGINE_RASTER]) {
	  DRM_INFO("Overlap\n");
	}
#endif
	scheduler->idle = 0;
	scheduler->ta_end_jiffies = jiffies + PSB_TA_TIMEOUT;

	PSB_DEBUG_RENDER("Fire ta. %u\n", task->sequence);
	(void) psb_reg_submit(dev_priv, task->ta_cmds, 
			      task->ta_cmd_size);
	psb_set_scene_fire(scheduler, task->scene, PSB_SCENE_ENGINE_TA,
			   task);
	psb_schedule_watchdog(dev_priv);
}

/*
 * Do a fast check if the hw scene context has released its memory.
 * In that case, put it on the available list and clear the flag.
 */

static inline int psb_check_pending(struct drm_psb_private *dev_priv)
{
	struct psb_scheduler *scheduler = &dev_priv->scheduler;
	uint32_t status = 0;

	if (!scheduler->pending_hw_scene) 
		return 0;
	status = PSB_RSGX32(PSB_CR_EVENT_STATUS) & _PSB_CE_DPM_3D_MEM_FREE;
	if (status) {
		list_add_tail(&scheduler->pending_hw_scene->head,
			      &scheduler->hw_scenes);
		scheduler->pending_hw_scene = NULL;
		psb_report_fence(scheduler, PSB_ENGINE_TA,
				 scheduler->pending_hw_scene_seq,
				 _PSB_FENCE_SCENE_DONE_SHIFT, 1);
		PSB_WSGX32(status, PSB_CR_EVENT_HOST_CLEAR);
		psb_schedule_ta(dev_priv, scheduler);
		psb_set_idle(scheduler);
		return 0;
	}
	return -EBUSY;
}


static int psb_fire_raster(struct psb_scheduler *scheduler, struct psb_task *task)
{
	struct drm_device *dev = scheduler->dev;
	struct drm_psb_private *dev_priv = (struct drm_psb_private *)
		dev->dev_private;

	PSB_DEBUG_RENDER("Fire raster %d\n", task->sequence);

	return psb_xhw_fire_raster(dev_priv, &task->buf, task->flags);
}

/*
 * Take the first rasterization task from the hp raster queue or from the
 * raster queue and fire the rasterizer.
 */

static void psb_schedule_raster(struct drm_psb_private *dev_priv,
				struct psb_scheduler *scheduler)
{
	struct psb_task *task;
	struct list_head *list;
	int ret;

	if (scheduler->idle_count != 0)
		return;

	if (scheduler->current_task[PSB_SCENE_ENGINE_RASTER] != NULL) {
	        PSB_DEBUG_RENDER("Raster busy.\n"); 
		return;
	}

#ifdef PSB_BLOCK_OVERLAP
	if (scheduler->current_task[PSB_SCENE_ENGINE_TA] != NULL ||
	    scheduler->pending_hw_scene != NULL) {
	        PSB_DEBUG_RENDER("Binner busy.\n"); 
		return;
	}
#endif

	if (!list_empty(&scheduler->hp_raster_queue))
		list = scheduler->hp_raster_queue.next;
	else if (!list_empty(&scheduler->raster_queue))
		list = scheduler->raster_queue.next;
	else {
		PSB_DEBUG_RENDER("Nothing in list\n"); 
		return;
	}
	
	task = list_entry(list, struct psb_task, head);

	/*
	 * FIXME: Can we move this test to inside the if statement below?
	 */

	ret = psb_check_pending(dev_priv);
	if (ret) {
	        PSB_DEBUG_RENDER("Check pending failure\n"); 
		return;
	}

#ifdef PSB_BLOCK_OVERLAP
	if (scheduler->current_task[PSB_SCENE_ENGINE_TA] != NULL) {
		PSB_DEBUG_RENDER("Binner busy.\n"); 
		return;
	}
#endif

	list_del_init(list);
	scheduler->current_task[PSB_SCENE_ENGINE_RASTER] = task;
	scheduler->idle = 0;
	scheduler->raster_end_jiffies = jiffies + PSB_RASTER_TIMEOUT;
	
	(void) psb_reg_submit(dev_priv, task->raster_cmds, 
			      task->raster_cmd_size);
	if (task->scene) {
		psb_set_scene_fire(scheduler, 
				   task->scene, PSB_SCENE_ENGINE_RASTER,
				   task);
	} else {
		psb_fire_raster(scheduler, task);
	}
	psb_schedule_watchdog(dev_priv);
}


/*
 * Binner done handler.
 */

static void psb_ta_done(struct drm_psb_private *dev_priv,
			    struct psb_scheduler *scheduler)
{
	struct psb_task *task = scheduler->current_task[PSB_SCENE_ENGINE_TA];
	struct psb_scene *scene = task->scene;

	PSB_DEBUG_RENDER("Binner done %u\n", task->sequence);

	switch(task->ta_complete_action) {
	case PSB_RASTER_BLOCK:
		scheduler->ta_state = 1;
		scene->flags |= (PSB_SCENE_FLAG_DIRTY | PSB_SCENE_FLAG_COMPLETE);
		list_add_tail(&task->head, &scheduler->raster_queue);
		break;
	case PSB_RASTER:
		scheduler->ta_state = 0;
		scene->flags |= (PSB_SCENE_FLAG_DIRTY | PSB_SCENE_FLAG_COMPLETE);
		list_add_tail(&task->head, &scheduler->raster_queue);
		break;
	case PSB_RETURN:
		scheduler->ta_state = 0;
		scene->flags |= PSB_SCENE_FLAG_DIRTY;
		list_add_tail(&scene->hw_scene->head, &scheduler->hw_scenes);

		break;
	}

	scheduler->current_task[PSB_SCENE_ENGINE_TA] = NULL;
	psb_schedule_raster(dev_priv, scheduler);
	psb_report_fence(scheduler, task->engine, task->sequence,
			 _PSB_FENCE_TA_DONE_SHIFT, 1);

	psb_schedule_ta(dev_priv, scheduler);
	psb_set_idle(scheduler);

	if (task->ta_complete_action != PSB_RETURN)
		return;

	list_add_tail(&task->head, &scheduler->task_done_queue);
	schedule_work(&scheduler->wq);
}


/*
 * Rasterizer done handler.
 */

static void psb_raster_done(struct drm_psb_private *dev_priv,
			    struct psb_scheduler *scheduler)
{
	struct psb_task *task = scheduler->current_task[PSB_SCENE_ENGINE_RASTER];
	struct psb_scene *scene = task->scene;

	PSB_DEBUG_RENDER("Raster done %u\n", task->sequence);

	if (scene) {
		switch(task->raster_complete_action) {
		case PSB_TA:
			scheduler->ta_state = 0;
			list_add(&task->head, &scheduler->ta_queue);
			psb_schedule_ta(dev_priv, scheduler);
			break;
		case PSB_RETURN:
			scene->flags &= ~(PSB_SCENE_FLAG_DIRTY | PSB_SCENE_FLAG_COMPLETE);

			/*
			 * Wait until DPM memory is deallocated before releasing
			 * the hw scenes.
			 */

			scheduler->pending_hw_scene = scene->hw_scene;
			scene->hw_scene = NULL;
			scheduler->pending_hw_scene_seq = task->sequence;
			break;
		}
	}
	scheduler->current_task[PSB_SCENE_ENGINE_RASTER] = NULL;
	psb_schedule_ta(dev_priv, scheduler);
	psb_schedule_raster(dev_priv, scheduler);
	psb_set_idle(scheduler);

	if (task->raster_complete_action != PSB_TA) {
	        PSB_DEBUG_RENDER("Signal raster done fence type %u\n", task->sequence);
		psb_report_fence(scheduler, task->engine, task->sequence,
				 _PSB_FENCE_RASTER_DONE_SHIFT, 1);
		list_add_tail(&task->head, &scheduler->task_done_queue);
		schedule_work(&scheduler->wq);
	}
}

void psb_scheduler_pause(struct drm_psb_private *dev_priv)
{
	struct psb_scheduler *scheduler = &dev_priv->scheduler;
	unsigned long irq_flags;

	spin_lock_irqsave(&scheduler->lock, irq_flags);
	scheduler->idle_count++;
	spin_unlock_irqrestore(&scheduler->lock, irq_flags);
}

void psb_scheduler_restart(struct drm_psb_private *dev_priv)
{
	struct psb_scheduler *scheduler = &dev_priv->scheduler;
	unsigned long irq_flags;

	spin_lock_irqsave(&scheduler->lock, irq_flags);
	if (--scheduler->idle_count == 0) {
		psb_schedule_ta(dev_priv, scheduler);
		psb_schedule_raster(dev_priv, scheduler);
	}
	spin_unlock_irqrestore(&scheduler->lock, irq_flags);
}

int psb_scheduler_idle(struct drm_psb_private *dev_priv)
{
	struct psb_scheduler *scheduler = &dev_priv->scheduler;
	unsigned long irq_flags;
	int ret;
	spin_lock_irqsave(&scheduler->lock, irq_flags);
	ret = scheduler->idle_count != 0 && scheduler->idle;
	spin_unlock_irqrestore(&scheduler->lock, irq_flags);
	return ret;
}


static void psb_ta_oom(struct drm_psb_private *dev_priv,
			   struct psb_scheduler *scheduler)
{

	struct psb_task *task = scheduler->current_task[PSB_SCENE_ENGINE_TA];
	if (!task)
		return;

	(void) psb_xhw_ta_oom(dev_priv, &task->buf,
				  task->scene->hw_cookie); 
}

static void psb_ta_oom_reply(struct drm_psb_private *dev_priv,
				 struct psb_scheduler *scheduler)
{

	struct psb_task *task = scheduler->current_task[PSB_SCENE_ENGINE_TA];
	uint32_t flags;
	if (!task)
		return;

	psb_xhw_ta_oom_reply(dev_priv, &task->buf,
				 task->scene->hw_cookie,
				 &task->ta_complete_action,
				 &task->raster_complete_action,
				 &flags);
	task->flags |= flags;

	if (task->ta_complete_action == PSB_RASTER_BLOCK) 
		psb_ta_done(dev_priv, scheduler);
}

static void psb_ta_hw_scene_freed(struct drm_psb_private *dev_priv,
				      struct psb_scheduler *scheduler)
{
	DRM_ERROR("Binner hw scene freed.\n");
}


void psb_scheduler_handler(struct drm_psb_private *dev_priv, uint32_t status)
{
	struct psb_scheduler *scheduler = &dev_priv->scheduler;
	
	spin_lock(&scheduler->lock);

	if (status & _PSB_CE_PIXELBE_END_RENDER) {
		psb_raster_done(dev_priv, scheduler);
	}
	if (status & _PSB_CE_DPM_3D_MEM_FREE) {
		if (scheduler->pending_hw_scene) {
			PSB_DEBUG_RENDER("DPM memory freed %u.\n", 
					 scheduler->pending_hw_scene_seq);
			list_add_tail(&scheduler->pending_hw_scene->head,
				      &scheduler->hw_scenes);
			scheduler->pending_hw_scene = NULL;
			psb_report_fence(scheduler, PSB_ENGINE_TA,
					 scheduler->pending_hw_scene_seq,
					 _PSB_FENCE_SCENE_DONE_SHIFT, 1);
			psb_schedule_raster(dev_priv, scheduler);
			psb_schedule_ta(dev_priv, scheduler);
			psb_set_idle(scheduler);
		} else {
			DRM_ERROR("Huh? No pending hw scene.\n");
		}
	}
	if (status & (_PSB_CE_TA_FINISHED | _PSB_CE_TA_TERMINATE)) {
		psb_ta_done(dev_priv, scheduler);
	}
	if (status & (_PSB_CE_DPM_REACHED_MEM_THRESH |
		      _PSB_CE_DPM_OUT_OF_MEMORY_GBL |
		      _PSB_CE_DPM_OUT_OF_MEMORY_MT)) {
		psb_ta_oom(dev_priv, scheduler);
	}
	if (status & _PSB_CE_DPM_TA_MEM_FREE) {
		psb_ta_hw_scene_freed(dev_priv, scheduler);
	}
	if (status & _PSB_CE_SW_EVENT) {
		psb_ta_oom_reply(dev_priv, scheduler);
	}
	spin_unlock(&scheduler->lock);
}

static void psb_free_task_devlocked(struct psb_scheduler *scheduler, 
			     struct psb_task *task)
{
	if (task->scene)
		psb_scene_unref_devlocked(&task->scene);

	drm_free(task, sizeof(*task), DRM_MEM_DRIVER);
}


static void psb_free_task_wq(struct work_struct *work)
{
	struct psb_scheduler *scheduler = 
		container_of(work, struct psb_scheduler, wq);

	struct drm_device *dev = scheduler->dev;
	struct list_head *list, *next;
	unsigned long irq_flags;
	struct psb_task *task;

	spin_lock_irqsave(&scheduler->lock, irq_flags);
	list_for_each_safe(list, next, &scheduler->task_done_queue) {
		task = list_entry(list, struct psb_task, head);

		if (!atomic_read(&task->buf.done)) 
			continue;

		list_del_init(list);
		spin_unlock_irqrestore(&scheduler->lock, irq_flags);
		mutex_lock(&dev->struct_mutex);
		psb_free_task_devlocked(scheduler, task);
		mutex_unlock(&dev->struct_mutex);
		spin_lock_irqsave(&scheduler->lock, irq_flags);		
	}
	spin_unlock_irqrestore(&scheduler->lock, irq_flags);
}


void psb_scheduler_reset(struct drm_psb_private *dev_priv,
			 int error_condition)
{
	struct psb_scheduler *scheduler = &dev_priv->scheduler;
	unsigned long wait_jiffies;
	unsigned long cur_jiffies;
	struct psb_task *task;
	struct psb_task *next_task;
	unsigned long irq_flags;

	psb_scheduler_pause(dev_priv);

	if (!psb_scheduler_idle(dev_priv)) {
		spin_lock_irqsave(&scheduler->lock, irq_flags);

		cur_jiffies = jiffies;
		wait_jiffies = cur_jiffies;
		if (scheduler->current_task[PSB_SCENE_ENGINE_TA] &&
		    time_after_eq(scheduler->ta_end_jiffies, wait_jiffies))
			wait_jiffies = scheduler->ta_end_jiffies;
		if (scheduler->current_task[PSB_SCENE_ENGINE_RASTER] &&
		    time_after_eq(scheduler->raster_end_jiffies, wait_jiffies))
			wait_jiffies = scheduler->raster_end_jiffies;

		wait_jiffies -= cur_jiffies;
		spin_unlock_irqrestore(&scheduler->lock, irq_flags);

		(void) wait_event_timeout(scheduler->idle_queue,
					  psb_scheduler_idle(dev_priv),
					  wait_jiffies);
	}

	if (!psb_scheduler_idle(dev_priv)) {
		spin_lock_irqsave(&scheduler->lock, irq_flags);
		task = scheduler->current_task[PSB_SCENE_ENGINE_RASTER];
		if (task) {
			DRM_ERROR("Detected Poulsbo rasterizer lockup.\n");

			if (task->engine == PSB_ENGINE_HPRAST) {
				psb_fence_error(scheduler->dev,
						PSB_ENGINE_HPRAST,
						task->sequence,
						_PSB_FENCE_TYPE_RASTER_DONE,
						error_condition);
				
				list_del(&task->head);
				list_add_tail(&task->head, 
					      &scheduler->task_done_queue);
			} else {
				list_add(&task->head, &scheduler->raster_queue);
			}
		}
		scheduler->current_task[PSB_SCENE_ENGINE_RASTER] = NULL;

		task = scheduler->current_task[PSB_SCENE_ENGINE_TA];
		if (task) {
			DRM_ERROR("Detected Poulsbo ta lockup.\n");
			list_add_tail(&task->head, &scheduler->raster_queue);
		}
		scheduler->current_task[PSB_SCENE_ENGINE_TA] = NULL;

		if (scheduler->pending_hw_scene)
			DRM_ERROR("Detected Poulsbo ta memory handler "
				  "lockup.\n");
			
		spin_unlock_irqrestore(&scheduler->lock, irq_flags);
	}

	/*
	 * Empty raster queue.
	 */

	spin_lock_irqsave(&scheduler->lock, irq_flags);

	if (scheduler->pending_hw_scene) {
		psb_fence_error(scheduler->dev,
				PSB_ENGINE_TA,
				scheduler->pending_hw_scene_seq,
				_PSB_FENCE_TYPE_SCENE_DONE,
				error_condition);
		list_add_tail(&scheduler->pending_hw_scene->head, 
			      &scheduler->hw_scenes);
		scheduler->pending_hw_scene = NULL;
	}


	list_for_each_entry_safe(task, next_task, &scheduler->raster_queue,
				 head) {
		struct psb_scene *scene = task->scene;

		psb_fence_error(scheduler->dev,
				task->engine,
				task->sequence,
				_PSB_FENCE_TYPE_TA_DONE |
				_PSB_FENCE_TYPE_RASTER_DONE |
				_PSB_FENCE_TYPE_SCENE_DONE,
				error_condition);

		if (scene) {
			scene->flags = 0;
			if (scene->hw_scene) {
				list_add_tail(&scene->hw_scene->head, 
					      &scheduler->hw_scenes);
			}
		}
		
		list_del(&task->head);
		list_add_tail(&task->head, &scheduler->task_done_queue);
	}

	schedule_work(&scheduler->wq);
	scheduler->idle = 1;
	wake_up(&scheduler->idle_queue);

	spin_unlock_irqrestore(&scheduler->lock, irq_flags);
	psb_scheduler_restart(dev_priv);

}


int psb_scheduler_init(struct drm_device *dev, struct psb_scheduler *scheduler)
{
	struct psb_hw_scene *hw_scene;
	int i;

	memset(scheduler, 0, sizeof(*scheduler));
	scheduler->dev = dev;
	mutex_init(&scheduler->hp_mutex);
	mutex_init(&scheduler->lp_mutex);
	scheduler->lock = SPIN_LOCK_UNLOCKED;
	scheduler->idle = 1;

	INIT_LIST_HEAD(&scheduler->ta_queue);
	INIT_LIST_HEAD(&scheduler->raster_queue);
	INIT_LIST_HEAD(&scheduler->hp_raster_queue);
	INIT_LIST_HEAD(&scheduler->hw_scenes);
	INIT_LIST_HEAD(&scheduler->task_done_queue);
	INIT_WORK(&scheduler->wq, &psb_free_task_wq);
	init_waitqueue_head(&scheduler->idle_queue);

	for (i=0; i<PSB_NUM_HW_SCENES; ++i) {
		hw_scene = &scheduler->hs[i];
		hw_scene->context_number = i;
		list_add_tail(&hw_scene->head, &scheduler->hw_scenes);
	}

	for (i=0; i<_PSB_ENGINE_TA_FENCE_TYPES; ++i) {
		scheduler->seq[i].reported = 0;
	}

	return 0;
}
		
void psb_scheduler_takedown(struct psb_scheduler *scheduler)
{
	/*
	 * FIXME: Flush and terminate the wq.
	 */

	;
}
	

static int psb_setup_task_devlocked(struct drm_device *dev,
				    struct drm_psb_cmdbuf_arg *arg,
				    struct drm_buffer_object *raster_cmd_buffer,
				    struct drm_buffer_object *ta_cmd_buffer,
				    struct psb_scene *scene,
				    enum psb_task_type task_type,
				    uint32_t engine,
				    uint32_t flags,
				    struct psb_task **task_p)
{
	struct psb_task *task;
	int ret;

	if (ta_cmd_buffer && arg->ta_size > PSB_MAX_TA_CMDS) {
		DRM_ERROR("Too many ta cmds %d.\n", arg->ta_size);
		return -EINVAL;
	}
	if (raster_cmd_buffer && arg->cmdbuf_size > PSB_MAX_RASTER_CMDS) {
		DRM_ERROR("Too many raster cmds %d.\n", arg->cmdbuf_size);
		return -EINVAL;
	}
		
	task = drm_calloc(1, sizeof(*task), DRM_MEM_DRIVER);
	if (!task)
		return -ENOMEM;

	atomic_set(&task->buf.done, 1);
	task->engine = engine;
	INIT_LIST_HEAD(&task->head);
	if (ta_cmd_buffer) {
		task->ta_cmd_size = arg->ta_size;
		ret = psb_submit_copy_cmdbuf(dev, ta_cmd_buffer,
					     arg->ta_offset,
					     arg->ta_size,
					     PSB_ENGINE_TA,
					     task->ta_cmds);
		if (ret)
			goto out_err;
	}
	if (raster_cmd_buffer) {
		task->raster_cmd_size = arg->cmdbuf_size;
		ret = psb_submit_copy_cmdbuf(dev, raster_cmd_buffer,
					     arg->cmdbuf_offset,
					     arg->cmdbuf_size,
					     PSB_ENGINE_TA,
					     task->raster_cmds);
		if (ret)
			goto out_err;
	}
	task->task_type = task_type;
	task->flags = flags;
	if (scene)
		task->scene = psb_scene_ref(scene);

	*task_p = task;
	return 0;
out_err:
	drm_free(task, sizeof(*task), DRM_MEM_DRIVER);
	*task_p = NULL;
	return ret;
}

	
int psb_cmdbuf_ta(struct drm_file *priv,
		      struct drm_psb_cmdbuf_arg *arg,
		      struct drm_buffer_object *cmd_buffer,
		      struct drm_buffer_object *ta_buffer,
		      struct psb_scene *scene,
		      struct drm_fence_arg *fence_arg)
			 
{
	struct drm_device *dev = priv->head->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct drm_fence_object *fence = NULL;
	struct psb_task *task = NULL;
	int ret;
	struct psb_scheduler *scheduler = &dev_priv->scheduler;
	unsigned long irq_flags;


	PSB_DEBUG_RENDER("Cmdbuf ta\n");

	ret = mutex_lock_interruptible(&dev_priv->reset_mutex);
	if (ret)
		return -EAGAIN;

	mutex_lock(&dev->struct_mutex);
	ret = psb_setup_task_devlocked(dev, arg, cmd_buffer, ta_buffer, scene,
				       psb_ta_task, PSB_ENGINE_TA,
				       PSB_RASTER_DEALLOC,
				       &task);
	mutex_unlock(&dev->struct_mutex);

	if (ret)
		goto out_err;

	/*
	 * Hand the task over to the scheduler. 
	 */

	spin_lock_irqsave(&scheduler->lock, irq_flags);
	task->sequence = psb_fence_advance_sequence(dev, PSB_ENGINE_TA);

	psb_report_fence(scheduler, PSB_ENGINE_TA,
			 task->sequence, 0, 1);

	task->ta_complete_action = PSB_RASTER;
	task->raster_complete_action = PSB_RETURN;

	list_add_tail(&task->head, &scheduler->ta_queue);
	PSB_DEBUG_RENDER("queued ta %u\n", task->sequence);

	psb_schedule_ta(dev_priv, scheduler);	
	spin_unlock_irqrestore(&scheduler->lock, irq_flags);

	ret = psb_fence_for_errors(priv, arg, fence_arg, &fence);
	if (ret)
		goto out_err;

	drm_regs_fence(&dev_priv->use_manager, fence);
 out_err:	
	if (ret && ret != -EAGAIN)
		DRM_ERROR("Binner task queue job failed.\n");

	if (fence) {
#ifdef PSB_WAIT_FOR_TA_COMPLETION
		drm_fence_object_wait(fence, 1, 1, DRM_FENCE_TYPE_EXE | 
				      _PSB_FENCE_TYPE_TA_DONE);
#ifdef PSB_BE_PARANOID
		drm_fence_object_wait(fence, 1, 1, DRM_FENCE_TYPE_EXE | 
			              _PSB_FENCE_TYPE_SCENE_DONE);
#endif
#endif
		drm_fence_usage_deref_unlocked(&fence);
	}
	mutex_unlock(&dev_priv->reset_mutex);

	return ret;
}



int psb_cmdbuf_raster(struct drm_file *priv,
		      struct drm_psb_cmdbuf_arg *arg,
		      struct drm_buffer_object *cmd_buffer,
		      struct drm_fence_arg *fence_arg)
{
	struct drm_device *dev = priv->head->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct drm_fence_object *fence = NULL;
	struct psb_task *task = NULL;
	int ret;
	struct psb_scheduler *scheduler = &dev_priv->scheduler;
	unsigned long irq_flags;

	/*
	 * We need to fence buffers here, since the fixup relocs
	 * functions need a fence for the USE base address registers.
	 * Re-emit the fence later with a new sequence number when we
	 * are sure that the job will really be scheduled.
	 */

	PSB_DEBUG_RENDER("Cmdbuf Raster\n");

	ret = mutex_lock_interruptible(&dev_priv->reset_mutex);
	if (ret)
		return -EAGAIN;

	mutex_lock(&dev->struct_mutex);
	ret = psb_setup_task_devlocked(dev, arg, cmd_buffer, NULL, NULL,
				       psb_raster_task,
				       PSB_ENGINE_TA,
				       0, &task);
	mutex_unlock(&dev->struct_mutex);

	if (ret)
		goto out_err;

	/*
	 * Hand the task over to the scheduler. 
	 */

	spin_lock_irqsave(&scheduler->lock, irq_flags);
	task->sequence = psb_fence_advance_sequence(dev, PSB_ENGINE_TA);
	psb_report_fence(scheduler, PSB_ENGINE_TA,
			 task->sequence, 0, 1);
	task->ta_complete_action = PSB_RASTER;
	task->raster_complete_action = PSB_RETURN;

	list_add_tail(&task->head, &scheduler->ta_queue);
	PSB_DEBUG_RENDER("queued raster %u\n", task->sequence);
	psb_schedule_ta(dev_priv, scheduler);
	spin_unlock_irqrestore(&scheduler->lock, irq_flags);
	
	ret = psb_fence_for_errors(priv, arg, fence_arg, &fence);
	if (ret)
		goto out_err;

	drm_regs_fence(&dev_priv->use_manager, fence);       
 out_err:	
	if (ret && ret != -EAGAIN)
		DRM_ERROR("Raster task queue job failed.\n");
	
	if (fence) {
#ifdef PSB_WAIT_FOR_RASTER_COMPLETION
		drm_fence_object_wait(fence, 1, 1, fence->type);
#endif
		drm_fence_usage_deref_unlocked(&fence);
	}

	mutex_unlock(&dev_priv->reset_mutex);

	return ret;
}

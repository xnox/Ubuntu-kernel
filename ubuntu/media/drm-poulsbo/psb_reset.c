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
 * Authors:
 * Thomas Hellstrom <thomas-at-tungstengraphics-dot-com>
 */

#include "drmP.h"
#include "psb_drv.h"
#include "psb_reg.h"



void psb_reset(struct drm_psb_private * dev_priv, int reset_2d)
{
	uint32_t val;

	val = _PSB_CS_RESET_BIF_RESET |
		_PSB_CS_RESET_DPM_RESET |
		_PSB_CS_RESET_TA_RESET  |
		_PSB_CS_RESET_USE_RESET |
		_PSB_CS_RESET_ISP_RESET |
		_PSB_CS_RESET_TSP_RESET;

	if (reset_2d)
		val |= _PSB_CS_RESET_TWOD_RESET;

	PSB_WSGX32(val, PSB_CR_SOFT_RESET);
	(void) PSB_RSGX32(PSB_CR_SOFT_RESET);

	msleep(1);
	
	PSB_WSGX32(0, PSB_CR_SOFT_RESET);
	wmb();
	PSB_WSGX32(PSB_RSGX32(PSB_CR_BIF_CTRL) | _PSB_CB_CTRL_CLEAR_FAULT,
		   PSB_CR_BIF_CTRL);
	wmb();
	(void) PSB_RSGX32(PSB_CR_BIF_CTRL);

	msleep(1);
	PSB_WSGX32(PSB_RSGX32(PSB_CR_BIF_CTRL) & ~_PSB_CB_CTRL_CLEAR_FAULT,
		   PSB_CR_BIF_CTRL);
	(void) PSB_RSGX32(PSB_CR_BIF_CTRL);
}

static void psb_print_pagefault(struct drm_psb_private *dev_priv)
{
	uint32_t val;
	uint32_t addr;

	val = PSB_RSGX32(PSB_CR_BIF_INT_STAT);
	addr = PSB_RSGX32(PSB_CR_BIF_FAULT);

	if (val) {
		if (val & _PSB_CBI_STAT_PF_N_RW)
			DRM_ERROR("Poulsbo MMU page fault:\n");
		else
			DRM_ERROR("Poulsbo MMU read / write "
				  "protection fault:\n");

		if (val & _PSB_CBI_STAT_FAULT_CACHE)
			DRM_ERROR("\tCache requestor.\n");
		if (val & _PSB_CBI_STAT_FAULT_TA)
			DRM_ERROR("\tTA requestor.\n");
		if (val & _PSB_CBI_STAT_FAULT_VDM)
			DRM_ERROR("\tVDM requestor.\n");
		if (val & _PSB_CBI_STAT_FAULT_2D)
			DRM_ERROR("\t2D requestor.\n");
		if (val & _PSB_CBI_STAT_FAULT_PBE)
			DRM_ERROR("\tPBE requestor.\n");
		if (val & _PSB_CBI_STAT_FAULT_TSP)
			DRM_ERROR("\tTSP requestor.\n");
		if (val & _PSB_CBI_STAT_FAULT_ISP)
			DRM_ERROR("\tISP requestor.\n");
		if (val & _PSB_CBI_STAT_FAULT_USSEPDS)
			DRM_ERROR("\tUSSEPDS requestor.\n");
		if (val & _PSB_CBI_STAT_FAULT_HOST)
			DRM_ERROR("\tHost requestor.\n");

		DRM_ERROR("\tMMU failing address is 0x%08x.\n", 
			  (unsigned) addr);
	}
}

void psb_schedule_watchdog(struct drm_psb_private *dev_priv)
{
	struct timer_list *wt = &dev_priv->watchdog_timer;

	spin_lock(&dev_priv->watchdog_lock);
	if (dev_priv->timer_available && !timer_pending(wt)) {
		wt->expires = jiffies + PSB_WATCHDOG_DELAY;
		add_timer(wt);
	}
	spin_unlock(&dev_priv->watchdog_lock);
}

static void psb_watchdog_func(unsigned long data)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) data;
	int lockup;
	int msvdx_lockup;
	int idle;
	int msvdx_idle;

	psb_scheduler_lockup(dev_priv, &lockup, &msvdx_lockup, &idle, &msvdx_idle);

	if (lockup || msvdx_lockup) {
		DRM_ERROR("Detected Poulsbo Scheduler Lockup.\n");
		spin_lock(&dev_priv->watchdog_lock);
		dev_priv->timer_available = 0;
		spin_unlock(&dev_priv->watchdog_lock);
		if(lockup)
		{
			psb_print_pagefault(dev_priv);
			schedule_work(&dev_priv->watchdog_wq);
		}
		if(msvdx_lockup)
			schedule_work(&dev_priv->msvdx_watchdog_wq);
	}

	if (!idle || !msvdx_idle)
		psb_schedule_watchdog(dev_priv);
}

static void psb_msvdx_reset_wq(struct work_struct *work)
{
	struct drm_psb_private *dev_priv = 
		container_of(work, struct drm_psb_private, msvdx_watchdog_wq);
	
	struct psb_scheduler *scheduler = &dev_priv->scheduler;

	dev_priv->msvdx_needs_reset = 1;
	dev_priv->msvdx_current_sequence++;
	PSB_DEBUG_GENERAL("MSVDXFENCE: incremented msvdx_current_sequence to :%d\n", 
		dev_priv->msvdx_current_sequence);

	psb_fence_error(scheduler->dev,PSB_ENGINE_VIDEO, 
		dev_priv->msvdx_current_sequence, DRM_FENCE_TYPE_EXE, DRM_CMD_HANG);

	spin_lock(&dev_priv->watchdog_lock);
	dev_priv->timer_available = 1;
	spin_unlock(&dev_priv->watchdog_lock);
}


/*
 * Block command submission and reset hardware and schedulers.
 */

static void psb_reset_wq(struct work_struct *work)
{
	struct drm_psb_private *dev_priv = 
		container_of(work, struct drm_psb_private, watchdog_wq);
	struct psb_xhw_buf buf;
	
	DRM_INFO("Reset\n");

	/*
	 * Block command submission.
	 */

	mutex_lock(&dev_priv->reset_mutex);

	psb_reset(dev_priv, 0);

	(void )psb_xhw_reset_dpm(dev_priv, &buf);

	DRM_INFO("Reset scheduler.\n");

	psb_scheduler_reset(dev_priv, -EBUSY);

	spin_lock(&dev_priv->watchdog_lock);
	dev_priv->timer_available = 1;
	spin_unlock(&dev_priv->watchdog_lock);
	mutex_unlock(&dev_priv->reset_mutex);	
}

void psb_watchdog_init(struct drm_psb_private *dev_priv)
{
	struct timer_list *wt = &dev_priv->watchdog_timer;

	dev_priv->watchdog_lock = SPIN_LOCK_UNLOCKED;
	spin_lock(&dev_priv->watchdog_lock);
	init_timer(wt);

	INIT_WORK(&dev_priv->watchdog_wq, &psb_reset_wq);
	INIT_WORK(&dev_priv->msvdx_watchdog_wq, &psb_msvdx_reset_wq);
	wt->data = (unsigned long) dev_priv;
	wt->function = &psb_watchdog_func;
	dev_priv->timer_available = 1;

	spin_unlock(&dev_priv->watchdog_lock);
}

void psb_watchdog_takedown(struct drm_psb_private *dev_priv)
{
	spin_lock(&dev_priv->watchdog_lock);
	dev_priv->timer_available = 0;
	spin_unlock(&dev_priv->watchdog_lock);
	(void) del_timer_sync(&dev_priv->watchdog_timer);
}	
	

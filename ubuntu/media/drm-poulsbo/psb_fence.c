/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Intel funded Tungsten Graphics (http://www.tungstengraphics.com) to
 * develop this driver.
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
			drm_fence_handler(scheduler->dev, PSB_ENGINE_TA,
					  sequence, flags, 0);
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

static void psb_perform_flush(struct drm_device *dev, uint32_t fence_class)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence_class];
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	uint32_t diff;
	uint32_t sequence;

	if (!dev_priv)
		return;

	/*PSB_DEBUG_GENERAL("MSVDXFENCE: pending_exe_flush=%d last_exe_flush=%d sequence=%d wrap_diff=%d\n",
	   fc->pending_exe_flush, fc->last_exe_flush, sequence, driver->wrap_diff); */

	if (fence_class == PSB_ENGINE_TA) {
		psb_flush_ta_fence(&dev_priv->scheduler, fc);
		return;
	}

	if (fc->pending_exe_flush) {
		if (fence_class == PSB_ENGINE_VIDEO)
			sequence = dev_priv->msvdx_current_sequence;
		else
			sequence = dev_priv->comm[fence_class << 4];

		/*
		 * First update fences with the current breadcrumb.
		 */

		diff = sequence - fc->last_exe_flush;
		if (diff < driver->wrap_diff && diff != 0) {
			drm_fence_handler(dev, fence_class, sequence,
					  DRM_FENCE_TYPE_EXE, 0);
		}

		switch (fence_class) {
		case PSB_ENGINE_2D:
			if (dev_priv->fence0_irq_on && !fc->pending_exe_flush) {
				psb_2D_irq_off(dev_priv);
				dev_priv->fence0_irq_on = 0;
			} else if (!dev_priv->fence0_irq_on
				   && fc->pending_exe_flush) {
				psb_2D_irq_on(dev_priv);
				dev_priv->fence0_irq_on = 1;
			}
			break;
		case PSB_ENGINE_VIDEO:
			/*TBD fix this for video...!!! */
			if (dev_priv->fence2_irq_on && !fc->pending_exe_flush) {
/*			psb_msvdx_irq_off(dev_priv);*/
				dev_priv->fence2_irq_on = 0;
			} else if (!dev_priv->fence2_irq_on
				   && fc->pending_exe_flush) {
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
		     uint32_t fence_class,
		     uint32_t sequence, uint32_t type, int error)
{
	struct drm_fence_manager *fm = &dev->fm;
	unsigned long irq_flags;

	BUG_ON(fence_class >= PSB_NUM_ENGINES);
	write_lock_irqsave(&fm->lock, irq_flags);
	drm_fence_handler(dev, fence_class, sequence, type, error);
	write_unlock_irqrestore(&fm->lock, irq_flags);
}

void psb_poke_flush(struct drm_device *dev, uint32_t fence_class)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	unsigned long flags;

	BUG_ON(fence_class >= PSB_NUM_ENGINES);
	spin_lock_irqsave(&dev_priv->scheduler.lock, flags);
	write_lock(&fm->lock);
	psb_perform_flush(dev, fence_class);
	write_unlock(&fm->lock);
	spin_unlock_irqrestore(&dev_priv->scheduler.lock, flags);
}

int psb_fence_emit_sequence(struct drm_device *dev, uint32_t fence_class,
			    uint32_t flags, uint32_t * sequence,
			    uint32_t * native_type)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	uint32_t seq = 0;
	int ret;

	if (!dev_priv)
		return -EINVAL;

	if (fence_class >= PSB_NUM_ENGINES)
		return -EINVAL;

	switch (fence_class) {
	case PSB_ENGINE_2D:
		spin_lock(&dev_priv->sequence_lock);
		seq = ++dev_priv->sequence[fence_class];
		spin_unlock(&dev_priv->sequence_lock);
		ret = psb_blit_sequence(dev_priv, seq);
		if (ret)
			return ret;
		break;
	case PSB_ENGINE_VIDEO:
		spin_lock(&dev_priv->sequence_lock);
		seq = ++dev_priv->sequence[fence_class];
		spin_unlock(&dev_priv->sequence_lock);
		break;
	default:
		spin_lock(&dev_priv->sequence_lock);
		seq = dev_priv->sequence[fence_class];
		spin_unlock(&dev_priv->sequence_lock);
	}

	*sequence = seq;
	*native_type = DRM_FENCE_TYPE_EXE;

	return 0;
}

uint32_t psb_fence_advance_sequence(struct drm_device * dev,
				    uint32_t fence_class)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	uint32_t sequence;

	spin_lock(&dev_priv->sequence_lock);
	sequence = ++dev_priv->sequence[fence_class];
	spin_unlock(&dev_priv->sequence_lock);

	return sequence;
}

void psb_fence_handler(struct drm_device *dev, uint32_t fence_class)
{
	struct drm_fence_manager *fm = &dev->fm;

#ifdef FIX_TG_16
	if (fence_class == 0) {
		struct drm_psb_private *dev_priv =
			(struct drm_psb_private *)dev->dev_private;

		if ((atomic_read(&dev_priv->ta_wait_2d_irq) == 1) &&
		    (PSB_RSGX32(PSB_CR_2D_SOCIF) == _PSB_C2_SOCIF_EMPTY) &&
		    ((PSB_RSGX32(PSB_CR_2D_BLIT_STATUS) &
		      _PSB_C2B_STATUS_BUSY) == 0))
			psb_resume_ta_2d_idle(dev_priv);
	}
#endif
	write_lock(&fm->lock);
	psb_perform_flush(dev, fence_class);
	write_unlock(&fm->lock);
}

int psb_fence_has_irq(struct drm_device *dev, uint32_t fence_class,
		      uint32_t flags)
{
	/*
	 * We have an irq that tells us when we have a new breadcrumb,
	 */

	if (((fence_class == PSB_ENGINE_2D) ||
	     (fence_class == PSB_ENGINE_RASTERIZER) ||
	     (fence_class == PSB_ENGINE_TA)
	     || (fence_class == PSB_ENGINE_VIDEO)))
		return 1;

	return 0;
}

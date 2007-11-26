/**************************************************************************
 *
 * Copyright (c) 2006 Tungsten Graphics Inc. Cedar Park, TX. USA. 
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
 */

#include "drmP.h"
#include "psb_drv.h"
#include "psb_reg.h"
#include "psb_msvdx.h"

/*
 * Video display controller interrupt.
 */

static void psb_vdc_interrupt(struct drm_device * dev, u32 vdc_stat)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;
	u32 pipe_stats;
	int wake = 0;

	if (vdc_stat & _PSB_VSYNC_PIPEA_FLAG) {
		pipe_stats = PSB_RVDC32(PSB_PIPEASTAT);
		atomic_inc(&dev->vbl_received);
		wake = 1;
		PSB_WVDC32(pipe_stats | _PSB_VBLANK_INTERRUPT_ENABLE |
			   _PSB_VBLANK_CLEAR, PSB_PIPEASTAT);
	}

	if (vdc_stat & _PSB_VSYNC_PIPEB_FLAG) {
		pipe_stats = PSB_RVDC32(PSB_PIPEBSTAT);
		atomic_inc(&dev->vbl_received2);
		wake = 1;
		PSB_WVDC32(pipe_stats | _PSB_VBLANK_INTERRUPT_ENABLE |
			   _PSB_VBLANK_CLEAR, PSB_PIPEBSTAT);
	}

	PSB_WVDC32(vdc_stat, PSB_INT_IDENTITY_R);
	(void)PSB_RVDC32(PSB_INT_IDENTITY_R);
	DRM_READMEMORYBARRIER();

	if (wake) {
		DRM_WAKEUP(&dev->vbl_queue);
		drm_vbl_send_signals(dev);
	}
}

/*
 * SGX interrupt source 1.
 */

static void psb_sgx_interrupt(struct drm_device * dev, u32 sgx_stat)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;

	if (sgx_stat & _PSB_CE_TWOD_COMPLETE) {
	        DRM_WAKEUP(&dev_priv->event_2d_queue);
		psb_fence_handler(dev, 0);
	}
	psb_scheduler_handler(dev_priv, sgx_stat);
}

/*
 * MSVDX interrupt.
 */
static void psb_msvdx_interrupt(struct drm_device * dev, __u32 msvdx_stat)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;

	if(msvdx_stat & MSVDX_INTERRUPT_STATUS_CR_MMU_FAULT_IRQ_MASK)
	{	
		/*Ideally we should we should never get to this*/
		PSB_DEBUG_GENERAL("******MSVDX: msvdx_stat: 0x%x fence2_irq_on=%d ***** (MMU FAULT)\n", msvdx_stat, dev_priv->fence2_irq_on);

		/* Pause MMU */
		PSB_WMSVDX32( MSVDX_MMU_CONTROL0_CR_MMU_PAUSE_MASK, MSVDX_MMU_CONTROL0 );
		DRM_WRITEMEMORYBARRIER();

		/* Clear this interupt bit only */
		PSB_WMSVDX32( MSVDX_INTERRUPT_STATUS_CR_MMU_FAULT_IRQ_MASK, MSVDX_INTERRUPT_CLEAR );
		PSB_RMSVDX32( MSVDX_INTERRUPT_CLEAR );
		DRM_READMEMORYBARRIER();

		dev_priv->msvdx_needs_reset = 1;
	}
	else if(msvdx_stat & MSVDX_INTERRUPT_STATUS_CR_MTX_IRQ_MASK)
	{
		PSB_DEBUG_GENERAL("******MSVDX: msvdx_stat: 0x%x fence2_irq_on=%d ***** (MTX)\n", msvdx_stat, dev_priv->fence2_irq_on);

		/* Clear all interupt bits */
		PSB_WMSVDX32( 0xffff, MSVDX_INTERRUPT_CLEAR );
		PSB_RMSVDX32( MSVDX_INTERRUPT_CLEAR );
		DRM_READMEMORYBARRIER();

		psb_msvdx_mtx_interrupt(dev);
	}
}

irqreturn_t psb_irq_handler(DRM_IRQ_ARGS)
{
	struct drm_device *dev = (struct drm_device *) arg;
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;
	
	u32 vdc_stat;
	u32 sgx_stat;
	u32 msvdx_stat;
	int handled = 0;

	spin_lock(&dev_priv->irqmask_lock);

	vdc_stat = PSB_RVDC32(PSB_INT_IDENTITY_R);
	sgx_stat = PSB_RSGX32(PSB_CR_EVENT_STATUS);
	msvdx_stat = PSB_RMSVDX32(MSVDX_INTERRUPT_STATUS);

	if (!(vdc_stat & dev_priv->vdc_irq_mask)) {
		spin_unlock(&dev_priv->irqmask_lock);
		return IRQ_NONE;
	}

	sgx_stat &= dev_priv->sgx_irq_mask;
	PSB_WSGX32(sgx_stat, PSB_CR_EVENT_HOST_CLEAR);
	(void) PSB_RSGX32(PSB_CR_EVENT_HOST_CLEAR);

	vdc_stat &= dev_priv->vdc_irq_mask;
	spin_unlock(&dev_priv->irqmask_lock);

	if(msvdx_stat) {
		psb_msvdx_interrupt(dev, msvdx_stat);
		handled = 1;
	}
	if (vdc_stat) {
		/* MSVDX IRQ status is part of vdc_irq_mask */
		psb_vdc_interrupt(dev, vdc_stat);
		handled = 1;
	}

	if (sgx_stat) {
		psb_sgx_interrupt(dev, sgx_stat);
		handled = 1;
	}

	if (!handled) 
	        PSB_DEBUG_IRQ("Unhandled irq\n");


	return IRQ_HANDLED;
}

void psb_irq_preinstall(struct drm_device * dev)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;
	spin_lock(&dev_priv->irqmask_lock);
	PSB_WVDC32(0xFFFFFFFF, PSB_HWSTAM);
	PSB_WVDC32(0x00000000, PSB_INT_MASK_R);
	PSB_WVDC32(0x00000000, PSB_INT_ENABLE_R);
	PSB_WSGX32(0x00000000, PSB_CR_EVENT_HOST_ENABLE);
	(void)PSB_RSGX32(PSB_CR_EVENT_HOST_ENABLE);

	dev_priv->sgx_irq_mask = _PSB_CE_PIXELBE_END_RENDER |
		_PSB_CE_DPM_3D_MEM_FREE |
		_PSB_CE_TA_FINISHED |
	        _PSB_CE_DPM_REACHED_MEM_THRESH |
	        _PSB_CE_DPM_OUT_OF_MEMORY_GBL |
	        _PSB_CE_DPM_OUT_OF_MEMORY_MT |
	        _PSB_CE_SW_EVENT;
	dev_priv->vdc_irq_mask = _PSB_VSYNC_PIPEA_FLAG | 
		_PSB_VSYNC_PIPEB_FLAG |
		_PSB_IRQ_SGX_FLAG |
		_PSB_IRQ_MSVDX_FLAG;
	
	/*Clear MTX interrupt*/
	{
		unsigned long MtxInt = 0;
		REGIO_WRITE_FIELD_LITE(MtxInt, MSVDX_INTERRUPT_STATUS, CR_MTX_IRQ, 1 );
		PSB_WMSVDX32( MtxInt, MSVDX_INTERRUPT_CLEAR );
	}	
	spin_unlock(&dev_priv->irqmask_lock);
}

void psb_irq_postinstall(struct drm_device * dev)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);
	PSB_WSGX32(dev_priv->sgx_irq_mask, PSB_CR_EVENT_HOST_ENABLE);
	(void)PSB_RSGX32(PSB_CR_EVENT_HOST_ENABLE);
	/****MSVDX IRQ Setup...*****/
	/* Enable Mtx Interupt to host */
	{
		unsigned long ui32Enables = 0;
		PSB_DEBUG_GENERAL("Setting up MSVDX IRQs.....\n");		
		REGIO_WRITE_FIELD_LITE(ui32Enables, MSVDX_INTERRUPT_STATUS, CR_MTX_IRQ,	1);
		PSB_WMSVDX32( ui32Enables, MSVDX_HOST_INTERRUPT_ENABLE );
	}
	dev_priv->irq_enabled = 1;
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}

void psb_irq_uninstall(struct drm_device * dev)
{
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	dev_priv->sgx_irq_mask = 0x00000000;
	dev_priv->vdc_irq_mask = 0x00000000;

	PSB_WVDC32(0xFFFFFFFF, PSB_HWSTAM);
	PSB_WVDC32(0xFFFFFFFF, PSB_INT_MASK_R);
	PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);
	PSB_WSGX32(dev_priv->sgx_irq_mask, PSB_CR_EVENT_HOST_ENABLE);
	wmb();
	PSB_WVDC32(PSB_RVDC32(PSB_INT_IDENTITY_R), PSB_INT_IDENTITY_R);
	PSB_WSGX32(PSB_RSGX32(PSB_CR_EVENT_STATUS), PSB_CR_EVENT_HOST_CLEAR);

	/****MSVDX IRQ Setup...*****/
	/* Clear interrupt enabled flag */
	PSB_WMSVDX32( 0, MSVDX_HOST_INTERRUPT_ENABLE );
	
	dev_priv->irq_enabled = 0;
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);

}

void psb_2D_irq_off(struct drm_psb_private * dev_priv)
{
	unsigned long irqflags;
	u32 old_mask;
	u32 cleared_mask;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	--dev_priv->irqen_count_2d;
	if (dev_priv->irq_enabled && dev_priv->irqen_count_2d == 0) {
		
		old_mask = dev_priv->sgx_irq_mask;
		dev_priv->sgx_irq_mask &= ~_PSB_CE_TWOD_COMPLETE;
		PSB_WSGX32(dev_priv->sgx_irq_mask, PSB_CR_EVENT_HOST_ENABLE);
		(void)PSB_RSGX32(PSB_CR_EVENT_HOST_ENABLE);
		
		cleared_mask = (old_mask ^ dev_priv->sgx_irq_mask) & old_mask;
		PSB_WSGX32(cleared_mask, PSB_CR_EVENT_HOST_CLEAR);
		(void)PSB_RSGX32(PSB_CR_EVENT_HOST_CLEAR);
	}
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}

void psb_2D_irq_on(struct drm_psb_private * dev_priv)
{
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	if (dev_priv->irq_enabled && dev_priv->irqen_count_2d == 0) {
		dev_priv->sgx_irq_mask |= _PSB_CE_TWOD_COMPLETE;
		PSB_WSGX32(dev_priv->sgx_irq_mask, PSB_CR_EVENT_HOST_ENABLE);
		(void)PSB_RSGX32(PSB_CR_EVENT_HOST_ENABLE);
	}
	++dev_priv->irqen_count_2d;
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}

static int psb_vblank_do_wait(struct drm_device *dev, unsigned int *sequence,
			      atomic_t *counter)
{
	unsigned int cur_vblank;
	int ret = 0;


	DRM_WAIT_ON(ret, dev->vbl_queue, 3 * DRM_HZ,
		    (((cur_vblank = atomic_read(counter))
		      - *sequence) <= (1<<23)));
	
	*sequence = cur_vblank;

	return ret;
}

int psb_vblank_wait(struct drm_device *dev, unsigned int *sequence)
{
        int ret;	

	ret = psb_vblank_do_wait(dev, sequence, &dev->vbl_received);
	return ret;
}

int psb_vblank_wait2(struct drm_device *dev, unsigned int *sequence)
{
	int ret;	

	ret = psb_vblank_do_wait(dev, sequence, &dev->vbl_received2);
	return ret;
}

void psb_msvdx_irq_off(struct drm_psb_private * dev_priv)
{
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	if (dev_priv->irq_enabled) {
		dev_priv->vdc_irq_mask &= ~_PSB_IRQ_MSVDX_FLAG;
		PSB_WSGX32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);
		(void)PSB_RSGX32(PSB_INT_ENABLE_R);
	}
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}

void psb_msvdx_irq_on(struct drm_psb_private * dev_priv)
{
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	if (dev_priv->irq_enabled) {
		dev_priv->vdc_irq_mask |= _PSB_IRQ_MSVDX_FLAG;
		PSB_WSGX32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);
		(void)PSB_RSGX32(PSB_INT_ENABLE_R);
	}
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
}

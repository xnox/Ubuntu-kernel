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
 */

#include "drmP.h"
#include "psb_drv.h"
#include "psb_drm.h"
#include "psb_reg.h"
#include "psb_scene.h"

#include "psb_msvdx.h"

int psb_submit_video_cmdbuf(struct drm_device * dev,
		     struct drm_buffer_object * cmd_buffer,
		     unsigned long cmd_offset, unsigned long cmd_size, struct drm_fence_object *fence);

struct psb_dstbuf_cache {
	unsigned int dst;
	uint32_t *use_page;
	unsigned int use_index;
	uint32_t use_background; 
	struct drm_buffer_object *dst_buf;
	unsigned long dst_offset;
	uint32_t *dst_page;
	unsigned int dst_page_offset;
	struct drm_bo_kmap_obj dst_kmap;
	int dst_is_iomem;
};

static int psb_2d_wait_available(struct drm_psb_private * dev_priv, unsigned size)
{
	u32 avail = PSB_RSGX32(PSB_CR_2D_SOCIF);
	int ret = 0;

retry:
	if (avail < size) {
#if 0
		psb_2D_irq_on(dev_priv);
		DRM_WAIT_ON(ret, dev_priv->event_2d_queue, DRM_HZ,
			    ((avail = PSB_RSGX32(PSB_CR_2D_SOCIF)) >= size));
		psb_2D_irq_off(dev_priv);
		if (ret == 0)
			return 0;
		if (ret == -EINTR) {
			ret = 0;
			goto retry;
		}
#else
		avail = PSB_RSGX32(PSB_CR_2D_SOCIF);
		goto retry;
#endif
	}
	return ret;
}

int psb_2d_submit(struct drm_psb_private * dev_priv, u32 * cmdbuf, unsigned size)
{
	int ret = 0;
	int i;
	unsigned submit_size;

	while (size > 0) {
		submit_size = (size < 0x60) ? size : 0x60;
		size -= submit_size;
		ret = psb_2d_wait_available(dev_priv, submit_size);
		if (ret)
			return ret;

		submit_size <<= 2;

		for (i = 0; i < submit_size; i += 4) {
			PSB_WSGX32(*cmdbuf++, PSB_SGX_2D_SLAVE_PORT + i);
		}
		(void)PSB_RSGX32(PSB_SGX_2D_SLAVE_PORT + i - 4);
	}
	return 0;
}

int psb_blit_sequence(struct drm_psb_private * dev_priv)
{
	u32 buffer[8];
	u32 *bufp = buffer;

	*bufp++ = PSB_2D_FENCE_BH;

	*bufp++ = PSB_2D_DST_SURF_BH |
	    PSB_2D_DST_8888ARGB | (4 << PSB_2D_DST_STRIDE_SHIFT);
	*bufp++ = dev_priv->comm_mmu_offset - dev_priv->mmu_2d_offset;

	*bufp++ = PSB_2D_BLIT_BH |
	    PSB_2D_ROT_NONE |
	    PSB_2D_COPYORDER_TL2BR |
	    PSB_2D_DSTCK_DISABLE |
	    PSB_2D_SRCCK_DISABLE | PSB_2D_USE_FILL | PSB_2D_ROP3_PATCOPY;

	*bufp++ = dev_priv->sequence[PSB_ENGINE_2D] << PSB_2D_FILLCOLOUR_SHIFT;
	*bufp++ = (0 << PSB_2D_DST_XSTART_SHIFT) |
	    (0 << PSB_2D_DST_YSTART_SHIFT);
	*bufp++ = (1 << PSB_2D_DST_XSIZE_SHIFT) | (1 << PSB_2D_DST_YSIZE_SHIFT);

	*bufp++ = PSB_2D_FLUSH_BH;

	return psb_2d_submit(dev_priv, buffer, bufp - buffer);
}

int psb_emit_2d_copy_blit(struct drm_device * dev,
			  u32 src_offset,
			  u32 dst_offset, __u32 pages, int direction)
{
	uint32_t cur_pages;
	struct drm_psb_private *dev_priv = dev->dev_private;
	u32 buf[10];
	u32 *bufp;
	u32 xstart;
	u32 ystart;
	u32 blit_cmd;
	u32 pg_add;
	int ret;

	if (!dev_priv)
		return 0;

	if (direction) {
		pg_add = (pages - 1) << PAGE_SHIFT;
		src_offset += pg_add;
		dst_offset += pg_add;
	}

	blit_cmd = PSB_2D_BLIT_BH |
	    PSB_2D_ROT_NONE |
	    PSB_2D_DSTCK_DISABLE |
	    PSB_2D_SRCCK_DISABLE |
	    PSB_2D_USE_PAT |
	    PSB_2D_ROP3_SRCCOPY |
	    (direction ? PSB_2D_COPYORDER_BR2TL : PSB_2D_COPYORDER_TL2BR);
	xstart = (direction) ? ((PAGE_SIZE - 1) >> 2) : 0;

	while (pages > 0) {
		cur_pages = pages;
		if (cur_pages > 2048)
			cur_pages = 2048;
		pages -= cur_pages;
		ystart = (direction) ? cur_pages - 1 : 0;

		bufp = buf;
		*bufp++ = PSB_2D_FENCE_BH;

		*bufp++ = PSB_2D_DST_SURF_BH | PSB_2D_DST_8888ARGB |
		    (PAGE_SIZE << PSB_2D_DST_STRIDE_SHIFT);
		*bufp++ = dst_offset;
		*bufp++ = PSB_2D_SRC_SURF_BH | PSB_2D_SRC_8888ARGB |
		    (PAGE_SIZE << PSB_2D_SRC_STRIDE_SHIFT);
		*bufp++ = src_offset;
		*bufp++ =
		    PSB_2D_SRC_OFF_BH | (xstart << PSB_2D_SRCOFF_XSTART_SHIFT) |
		    (ystart << PSB_2D_SRCOFF_YSTART_SHIFT);
		*bufp++ = blit_cmd;
		*bufp++ = (xstart << PSB_2D_DST_XSTART_SHIFT) |
		    (ystart << PSB_2D_DST_YSTART_SHIFT);
		*bufp++ = ((PAGE_SIZE >> 2) << PSB_2D_DST_XSIZE_SHIFT) |
		    (cur_pages << PSB_2D_DST_YSIZE_SHIFT);

		ret = psb_2d_submit(dev_priv, buf, bufp - buf);
		if (ret)
			return ret;
		pg_add = (cur_pages << PAGE_SHIFT) * ((direction) ? -1 : 1);
		src_offset += pg_add;
		dst_offset += pg_add;
	}
	return 0;
}

void psb_init_2d(struct drm_psb_private * dev_priv)
{
        psb_reset(dev_priv, 1);
	dev_priv->mmu_2d_offset = dev_priv->pg->gatt_start;
	PSB_WSGX32(dev_priv->mmu_2d_offset, PSB_CR_BIF_TWOD_REQ_BASE);
	(void)PSB_RSGX32(PSB_CR_BIF_TWOD_REQ_BASE);
}

int drm_psb_idle(struct drm_device * dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	unsigned long _end = jiffies + DRM_HZ;
	int busy;
	int ret;

	//	psb_pause_scheduler(dev_priv);
	
	if ((PSB_RSGX32(PSB_CR_2D_SOCIF) == _PSB_C2_SOCIF_EMPTY) &&
	    ((PSB_RSGX32(PSB_CR_2D_BLIT_STATUS) & _PSB_C2B_STATUS_BUSY) == 0))
		return 0;

	if (dev_priv->engine_lockup_2d)
		return -EBUSY;

	if (dev->lock.hw_lock)
	  drm_idlelock_take(&dev->lock);

	do {
		busy = (PSB_RSGX32(PSB_CR_2D_SOCIF) != _PSB_C2_SOCIF_EMPTY);
	} while (busy && !time_after_eq(jiffies, _end));
	if (busy)
		busy = (PSB_RSGX32(PSB_CR_2D_SOCIF) != _PSB_C2_SOCIF_EMPTY);
	if (busy) {
		ret = -EBUSY;
		goto out;
	}

	do {
		busy =
		    ((PSB_RSGX32(PSB_CR_2D_BLIT_STATUS) & _PSB_C2B_STATUS_BUSY)
		     != 0);
	} while (busy && !time_after_eq(jiffies, _end));

	if (busy)
		busy =
		    ((PSB_RSGX32(PSB_CR_2D_BLIT_STATUS) & _PSB_C2B_STATUS_BUSY)
		     != 0);
	ret = (busy) ? -EBUSY : 0;
      out:
	if (dev->lock.hw_lock)
	  
	  drm_idlelock_release(&dev->lock);

	if (ret == -EBUSY) {
		dev_priv->engine_lockup_2d = 1;
		DRM_ERROR("Detected SGX 2D Engine lockup 0x%08x 0x%08x\n",
			  PSB_RSGX32(PSB_CR_BIF_INT_STAT),
			  PSB_RSGX32(PSB_CR_BIF_FAULT));
	}

	return ret;
}

static void psb_dereference_buffers_locked(struct psb_buflist_item * buffers,
					   unsigned num_buffers)
{
	while (num_buffers--)
		drm_bo_usage_deref_locked(&((buffers++)->bo));

}

static int psb_validate_buffer_list(struct drm_file * file_priv,
				    unsigned fence_class,
				    unsigned long data,
				    struct psb_buflist_item * buffers,
				    unsigned *num_buffers)
{
        struct drm_bo_op_arg arg;
	struct drm_bo_op_req *req = &arg.d.req;
	int ret = 0;
	unsigned buf_count = 0;
        struct psb_buflist_item *item = buffers;

	do {
		if (buf_count >= *num_buffers) {
			DRM_ERROR("Buffer count exceeded %d\n.", *num_buffers);
			ret = -EINVAL;
			goto out_err;
		}
		item = buffers + buf_count;
		item->bo = NULL;

		if (copy_from_user(&arg, (void __user *)data, sizeof(arg))) {
			ret = -EFAULT;
			DRM_ERROR("Error copying validate list.\n"
				  "\tbuffer %d, user addr 0x%08lx %d\n",
				  buf_count, (unsigned long) data, sizeof(arg));
			goto out_err;
		}

		if (arg.handled) {
			data = arg.next;
			item->bo =
			    drm_lookup_buffer_object(file_priv, req->bo_req.handle, 1);
			buf_count++;
			continue;
		}

		ret = 0;
		if (req->op != drm_bo_validate) {
			DRM_ERROR
			    ("Buffer object operation wasn't \"validate\".\n");
			ret = -EINVAL;
			goto out_err;
		}

		PSB_DEBUG_GENERAL("validate Fence class is %d\n", fence_class);

		item->ret = 0;
		item->data = (void *) __user data;
		ret = drm_bo_handle_validate(file_priv, 
					     req->bo_req.handle, 
					     fence_class,
					     req->bo_req.flags,
					     req->bo_req.mask,
					     req->bo_req.hint,
					     &item->rep, 
					     &item->bo);
		if (ret)
			goto out_err;

		PSB_DEBUG_GENERAL("Validated buffer at 0x%08lx\n",
				  buffers[buf_count].bo->offset);

		data = arg.next;
		buf_count++;

	} while (data);

	*num_buffers = buf_count;

	return 0;
      out_err:

	*num_buffers = buf_count;
	item->ret = (ret != -EAGAIN) ? ret : 0;
	return ret;
}





int 
psb_reg_submit(struct drm_psb_private *dev_priv, u32 *regs, unsigned int cmds)
{
	int i;
	unsigned int offs;

	/*
	 * cmds is 32-bit words.
	 */
	
	cmds >>= 1;
	for (i=0; i<cmds; ++i) {
		offs = regs[0];

		/*
		 * Don't allow user-space to write to arbitrary registers.
		 */

		PSB_DEBUG_RENDER("Submit 0x%08x 0x%08x\n",
				  regs[0], regs[1]);
#if 0
		if (offs < PSB_LOW_REG_OFFS || offs >= PSB_HIGH_REG_OFFS) {
			DRM_ERROR("Invalid register offset.\n");
			return -EPERM;
		}
#endif
		
		PSB_WSGX32(regs[1], offs);
		regs +=2;
	}
	wmb();
	return 0;
}
		


	
int
psb_submit_copy_cmdbuf(struct drm_device * dev,
		       struct drm_buffer_object * cmd_buffer,
		       unsigned long cmd_offset, 
		       unsigned long cmd_size,
		       int engine,
		       uint32_t *copy_buffer)
{
	unsigned long cmd_end = cmd_offset + (cmd_size << 2);
	struct drm_psb_private *dev_priv = dev->dev_private;
	unsigned long cmd_page_offset = cmd_offset - (cmd_offset & PAGE_MASK);
	unsigned long cmd_next;
	struct drm_bo_kmap_obj cmd_kmap;
	u32 *cmd_page;
	unsigned cmds;
	int is_iomem;
	int ret;

	if (cmd_size == 0)
		return 0;

	do {
		cmd_next = drm_bo_offset_end(cmd_offset, cmd_end);
		ret = drm_bo_kmap(cmd_buffer, cmd_offset >> PAGE_SHIFT,
				  1, &cmd_kmap);

		if (ret)
			return ret;
		cmd_page = drm_bmo_virtual(&cmd_kmap, &is_iomem);
		cmd_page_offset = (cmd_offset & ~PAGE_MASK) >> 2;
		cmds = (cmd_next - cmd_offset) >> 2;

		switch(engine) {
		case PSB_ENGINE_2D:
			ret = psb_2d_submit(dev_priv, cmd_page + cmd_page_offset, 
					    cmds);
			break;
		case PSB_ENGINE_RASTERIZER:
		case PSB_ENGINE_TA:
		case PSB_ENGINE_HPRAST:
			PSB_DEBUG_GENERAL("Reg copy.\n");
			memcpy(copy_buffer, cmd_page + cmd_page_offset, 
			       cmds * sizeof(uint32_t));
			copy_buffer += cmds;
			break;
		default:
			ret = -EINVAL;
		}
		drm_bo_kunmap(&cmd_kmap);
		if (ret)
			return ret;
	} while (cmd_offset = cmd_next, cmd_offset != cmd_end);
	return 0;
}

static void psb_clear_dstbuf_cache(struct psb_dstbuf_cache *dst_cache)
{
	if (dst_cache->dst_page) {
		drm_bo_kunmap(&dst_cache->dst_kmap);
		dst_cache->dst_page = NULL;
	}
	dst_cache->dst_buf = NULL;
	dst_cache->dst = ~0;
	dst_cache->use_page = NULL;
}

static int psb_update_dstbuf_cache(struct psb_dstbuf_cache *dst_cache,
				   struct psb_buflist_item *buffers,
				   unsigned int dst,
				   unsigned long dst_offset)
{
	int ret;

	PSB_DEBUG_GENERAL("Destination buffer is %d\n", dst);

	if (dst != dst_cache->dst || NULL == dst_cache->dst_buf) {
		psb_clear_dstbuf_cache(dst_cache);
		dst_cache->dst = dst;
		dst_cache->dst_buf = buffers[dst].bo;
	}

	if (!drm_bo_same_page(dst_cache->dst_offset, dst_offset) ||
	    NULL == dst_cache->dst_page) {
		if (NULL != dst_cache->dst_page) {
			drm_bo_kunmap(&dst_cache->dst_kmap);
			dst_cache->dst_page = NULL;
		}
	
		ret = drm_bo_kmap(dst_cache->dst_buf, dst_offset >> PAGE_SHIFT,
				  1, &dst_cache->dst_kmap);
		if (ret) {
			DRM_ERROR("Could not map destination buffer for "
				  "relocation.\n");
			return ret;
		}
	
		dst_cache->dst_page = drm_bmo_virtual(&dst_cache->dst_kmap,
						     &dst_cache->dst_is_iomem);
		dst_cache->dst_offset = dst_offset & PAGE_MASK;
		dst_cache->dst_page_offset =
			((dst_cache->dst_offset & PAGE_MASK) >> 2);
	}
	return 0;
}
	
static int psb_apply_reloc(struct drm_psb_private *dev_priv,
			   uint32_t fence_class,
			   const struct drm_psb_reloc * reloc,
			   struct psb_buflist_item * buffers,
			   int num_buffers, 
			   struct psb_dstbuf_cache *dst_cache,
			   int no_wait,
			   int interruptible)
{
        int reg;
	u32 val;
	u32 background;
	unsigned int index;
	int ret;
	unsigned int shift;
	unsigned int align_shift;
	uint32_t fence_type;

	if (reloc->buffer >= num_buffers) {
		DRM_ERROR("Illegal relocation buffer %d\n", reloc->buffer);
		return -EINVAL;
	}

#if 0
	PSB_DEBUG_GENERAL("Reloc type %d\n"
			  "\t where 0x%04x\n"
			  "\t buffer 0x%04x\n"
			  "\t mask 0x%08x\n"
			  "\t shift 0x%08x\n"
			  "\t pre_add 0x%08x\n"
			  "\t background 0x%08x\n"
			  "\t dst_buffer 0x%08x\n"
			  "\t arg0 0x%08x\n"
			  "\t arg1 0x%08x\n",
			  reloc->reloc_op,
			  reloc->where,
			  reloc->buffer,
			  reloc->mask,
			  reloc->shift,
			  reloc->pre_add,
			  reloc->background,
			  reloc->dst_buffer,
			  reloc->arg0,
			  reloc->arg1);
#endif
	/*
	 * Fixme: Check buffer size.
	 */

	ret = psb_update_dstbuf_cache(dst_cache, buffers, reloc->dst_buffer,
				      reloc->where << 2);
	if (ret)
		return ret;

	switch (reloc->reloc_op) {
	case PSB_RELOC_OP_OFFSET:
		val = buffers[reloc->buffer].bo->offset + reloc->pre_add;
		break;
	case PSB_RELOC_OP_2D_OFFSET:
		val = buffers[reloc->buffer].bo->offset + reloc->pre_add - 
			dev_priv->mmu_2d_offset;
		if (val >=  PSB_2D_SIZE) {
			DRM_ERROR("2D relocation out of bounds\n");
			return -EINVAL;
		}
		break;		
	case PSB_RELOC_OP_PDS_OFFSET:
		val = buffers[reloc->buffer].bo->offset + reloc->pre_add - 
			PSB_MEM_PDS_START;
		if (val >=  (PSB_MEM_MMU_START - PSB_MEM_PDS_START)) {
			DRM_ERROR("PDS relocation out of bounds\n");
			return -EINVAL;
		}
		break;
	case PSB_RELOC_OP_USE_OFFSET:
	case PSB_RELOC_OP_USE_REG:
		fence_type = buffers[reloc->buffer].bo->fence_type;
		ret = psb_grab_use_base(dev_priv, 
					buffers[reloc->buffer].bo->offset + 
					reloc->pre_add, reloc->arg0,
					reloc->arg1, fence_class,
					fence_type, no_wait,
					interruptible,
					&reg, &val);
		if (ret)
			return ret;

		val = (reloc->reloc_op == PSB_RELOC_OP_USE_REG) ? reg:val;
		break;
	default:
		DRM_ERROR("Unimplemented relocation.\n");
		return -EINVAL;
	}

	shift = (reloc->shift & PSB_RELOC_SHIFT_MASK) >>
		PSB_RELOC_SHIFT_SHIFT;
	align_shift = (reloc->shift & PSB_RELOC_ALSHIFT_MASK) >>
		PSB_RELOC_ALSHIFT_SHIFT;

	val = ((val >> align_shift) << shift);
	index = reloc->where - dst_cache->dst_page_offset;

	background = reloc->background;

	if (reloc->reloc_op == PSB_RELOC_OP_USE_OFFSET) {
		if (dst_cache->use_page == dst_cache->dst_page &&
		    dst_cache->use_index == index) 
			background = dst_cache->use_background;
		else
			background = dst_cache->dst_page[index];
	}
#if 0	
	if (dst_cache->dst_page[index] != PSB_RELOC_MAGIC &&
	    reloc->reloc_op != PSB_RELOC_OP_USE_OFFSET)
		DRM_ERROR("Inconsistent relocation 0x%08lx.\n", 
			  (unsigned long) dst_cache->dst_page[index]);
#endif

	val = (background & ~reloc->mask) | (val & reloc->mask);
	dst_cache->dst_page[index] = val;

	if (reloc->reloc_op == PSB_RELOC_OP_USE_OFFSET ||
	    reloc->reloc_op == PSB_RELOC_OP_USE_REG) {
		dst_cache->use_page = dst_cache->dst_page;
		dst_cache->use_index = index;
		dst_cache->use_background = val;
	}

        PSB_DEBUG_GENERAL("Reloc buffer %d index 0x%08x, value 0x%08x\n",
			  reloc->dst_buffer, index, dst_cache->dst_page[index]);

	return 0;
}

static int psb_ok_to_map_reloc(struct drm_psb_private *dev_priv,
			       unsigned int num_pages)
{
	int ret = 0;
    
	spin_lock(&dev_priv->reloc_lock);
	if (dev_priv->rel_mapped_pages + num_pages <= 
	    PSB_MAX_RELOC_PAGES) {
		dev_priv->rel_mapped_pages += num_pages;
		ret = 1;
	} 
	spin_unlock(&dev_priv->reloc_lock);
	return ret;
}


static int psb_fixup_relocs(struct drm_file * file_priv,
			    uint32_t fence_class,
			    unsigned int num_relocs,
			    unsigned int reloc_offset,
			    uint32_t reloc_handle,
			    struct psb_buflist_item * buffers,
			    unsigned int num_buffers,
			    int no_wait,
			    int interruptible)
{
	struct drm_psb_private *dev_priv = 
		(struct drm_psb_private *) file_priv->head->dev->dev_private;
	struct drm_buffer_object *reloc_buffer = NULL;
	unsigned int reloc_num_pages;
	unsigned int reloc_first_page;
	unsigned int reloc_last_page;
	struct psb_dstbuf_cache dst_cache;
	struct drm_psb_reloc *reloc;
	struct drm_bo_kmap_obj reloc_kmap;
	int reloc_is_iomem;
	int count;
	int ret = 0;
	int registered = 0;


	memset(&dst_cache, 0, sizeof(dst_cache));
	memset(&reloc_kmap, 0, sizeof(reloc_kmap));

	if (num_relocs == 0)
		goto out;

	reloc_buffer = drm_lookup_buffer_object(file_priv, reloc_handle, 1);
	if (!reloc_buffer)
		goto out;


	reloc_first_page = reloc_offset >> PAGE_SHIFT;
	reloc_last_page = (reloc_offset + num_relocs * sizeof(struct drm_psb_reloc)) >> PAGE_SHIFT;
	reloc_num_pages = reloc_last_page - reloc_first_page + 1;
	reloc_offset &= ~PAGE_MASK;

	if (reloc_num_pages > PSB_MAX_RELOC_PAGES) {
		DRM_ERROR("Relocation buffer is too large\n");
		ret = -EINVAL;
		goto out;
	}

	DRM_WAIT_ON(ret, dev_priv->rel_mapped_queue, 3*DRM_HZ,
		    (registered = psb_ok_to_map_reloc(dev_priv, reloc_num_pages)));

	if (ret == -EINTR) {
		ret = -EAGAIN;
		goto out;
	}
	if (ret) {
		DRM_ERROR("Error waiting for space to map "
			  "relocation buffer.\n");
		goto out;
	}

	ret = drm_bo_kmap(reloc_buffer, reloc_first_page, 
			  reloc_num_pages, &reloc_kmap);

	if (ret) {
		DRM_ERROR("Could not map relocation buffer.\n"
			  "\tReloc buffer id 0x%08x.\n"
			  "\tReloc first page %d.\n"
			  "\tReloc num pages %d.\n",
			  reloc_handle,
			  reloc_first_page,
			  reloc_num_pages);
		goto out;
	}
	reloc = (struct drm_psb_reloc *) 
		((unsigned long) drm_bmo_virtual(&reloc_kmap, &reloc_is_iomem) + reloc_offset);

	for (count = 0; count < num_relocs; ++count) {
		ret = psb_apply_reloc(dev_priv, fence_class,
				      reloc, buffers,
				      num_buffers, &dst_cache,
				      no_wait, interruptible);
		if (ret)
			goto out1;
		reloc++;
	} 

out1:
	drm_bo_kunmap(&reloc_kmap);
out:
	if (registered) {
		spin_lock(&dev_priv->reloc_lock);
		dev_priv->rel_mapped_pages -= reloc_num_pages;
		spin_unlock(&dev_priv->reloc_lock);
		DRM_WAKEUP(&dev_priv->rel_mapped_queue);
	}

	psb_clear_dstbuf_cache(&dst_cache);
	if (reloc_buffer)
		drm_bo_usage_deref_unlocked(&reloc_buffer);
	return ret;
}

static int psb_cmdbuf_2d(struct drm_file *priv,
			 struct drm_psb_cmdbuf_arg *arg,
			 struct drm_buffer_object *cmd_buffer,
			 struct drm_fence_arg *fence_arg)
{
	struct drm_device *dev = priv->head->dev;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	int ret;

	ret = mutex_lock_interruptible(&dev_priv->reset_mutex);
	if (ret)
		return -EAGAIN;

	ret = psb_submit_copy_cmdbuf(dev, cmd_buffer, arg->cmdbuf_offset,
				     arg->cmdbuf_size,
				     PSB_ENGINE_2D, NULL);
	if (ret)
		goto out_unlock;

	ret = psb_fence_for_errors(priv, arg, fence_arg, NULL);

	mutex_lock(&cmd_buffer->mutex);
	if (cmd_buffer->fence != NULL)
		drm_fence_usage_deref_unlocked(&cmd_buffer->fence);
	mutex_unlock(&cmd_buffer->mutex);
out_unlock:
	mutex_unlock(&dev_priv->reset_mutex);
	return ret;
}

#if 0
static int psb_dump_page(struct drm_buffer_object *bo,
			 unsigned int page_offset, unsigned int num)
{
	struct drm_bo_kmap_obj kmobj;
	int is_iomem;
	u32 *p;
	int ret;
	unsigned int i;

	ret = drm_bo_kmap(bo, page_offset, 1, &kmobj);
	if (ret)
		return ret;
	
	p = drm_bmo_virtual(&kmobj, &is_iomem);
	for (i=0; i<num; ++i)
		PSB_DEBUG_GENERAL("0x%04x: 0x%08x\n", i, *p++);
	
	drm_bo_kunmap(&kmobj);
	return 0;
}
#endif

int psb_fence_for_errors(struct drm_file *priv, 
			 struct drm_psb_cmdbuf_arg *arg,
			 struct drm_fence_arg *fence_arg,
			 struct drm_fence_object **fence_p)
{
	struct drm_device *dev = priv->head->dev;
	int ret;
	struct drm_fence_object *fence;

	ret = drm_fence_buffer_objects(dev, NULL, arg->fence_flags,
				       NULL, &fence);
	if (ret) {
		DRM_ERROR("Could not fence buffer objects. "
			  "Idling all engines.\n");
		(void) drm_psb_idle(dev);
		return ret;
	}

	if (!(arg->fence_flags & DRM_FENCE_FLAG_NO_USER)) {
		ret = drm_fence_add_user_object(priv, fence,
						arg->fence_flags &
						DRM_FENCE_FLAG_SHAREABLE);
		if (!ret)
			drm_fence_fill_arg(fence, fence_arg);

		else {
			DRM_ERROR("Could not create a fence user object. "
				  "Idling all engines.\n");
			(void) drm_psb_idle(dev);
		}
	}

	if (fence_p) 
		*fence_p = fence;
	else 
		drm_fence_usage_deref_unlocked(&fence);
	return ret;
}
	
int psb_handle_copyback(struct drm_device *dev, struct psb_buflist_item *buffers,
			unsigned int num_buffers, int ret, void *data)
{
        struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	struct drm_bo_op_arg arg;
	struct psb_buflist_item *item = buffers;
	int err = ret;
	int i;

	/*
	 * Clear the unfenced use base register lists and buffer lists.
	 */

	if (ret) {
		drm_regs_fence(&dev_priv->use_manager, NULL);
		drm_putback_buffer_objects(dev);
	}

	if (ret != -EAGAIN) {
		for (i=0; i<num_buffers; ++i) {
			arg.handled = 1;
			arg.d.rep.ret = item->ret;
			arg.d.rep.bo_info = item->rep;
			if (copy_to_user(item->data, &arg, sizeof(arg)))
				err = -EFAULT;
			++item;
		}
	}

	return err;
}

static int psb_cmdbuf_video(struct drm_file *priv,
			 struct drm_psb_cmdbuf_arg *arg,
			 unsigned int num_buffers,
			 struct drm_buffer_object *cmd_buffer, 
			 struct drm_fence_arg *fence_arg)			 
{
	struct drm_device *dev = priv->head->dev;
	struct drm_fence_object *fence;
	int ret;

	ret = psb_fence_for_errors(priv, arg, fence_arg, &fence);
	ret = psb_submit_video_cmdbuf(dev, cmd_buffer, arg->cmdbuf_offset,
			   arg->cmdbuf_size, fence);

	if (ret)
		return ret;	

	drm_fence_usage_deref_unlocked(&fence);
	mutex_lock(&cmd_buffer->mutex);
	if (cmd_buffer->fence != NULL) 
		drm_fence_usage_deref_unlocked(&cmd_buffer->fence);
	mutex_unlock(&cmd_buffer->mutex);
	return 0;
}
	
int psb_cmdbuf_ioctl(struct drm_device *dev, void *data, 
		     struct drm_file *file_priv)
{
        drm_psb_cmdbuf_arg_t *arg = data;
	int ret = 0;
	unsigned num_buffers;
	struct drm_buffer_object *cmd_buffer = NULL;
	struct drm_buffer_object *ta_buffer = NULL;
	struct drm_fence_arg fence_arg;
	struct drm_psb_scene user_scene;
	struct psb_scene_pool *pool = NULL;
	struct psb_scene *scene = NULL;
	struct drm_psb_private *dev_priv = 
		(struct drm_psb_private *) file_priv->head->dev->dev_private;
	int engine;
	static int againcount = 0;

	if (!dev_priv)
		return -EINVAL;

	LOCK_TEST_WITH_RETURN(dev, file_priv);
	
	num_buffers = PSB_NUM_VALIDATE_BUFFERS;

	ret = mutex_lock_interruptible(&dev_priv->cmdbuf_mutex);
	if (ret)
		return -EAGAIN;

	engine = (arg->engine == PSB_ENGINE_RASTERIZER) ? 
		PSB_ENGINE_TA : arg->engine;

	ret = psb_validate_buffer_list(file_priv, engine, (unsigned long) arg->buffer_list,
				       dev_priv->buffers, &num_buffers);
	if (ret) 
		goto out_err0;

	ret = psb_fixup_relocs(file_priv, engine, arg->num_relocs, 
			       arg->reloc_offset, arg->reloc_handle,
			       dev_priv->buffers, num_buffers, 0, 1);
	if (ret)
		goto out_err0;

#if 0
	if (engine == PSB_ENGINE_TA && (++againcount % 10) != 0) {
	  ret = -EAGAIN;
	  goto out_err0;
	}
#endif

	cmd_buffer = drm_lookup_buffer_object(file_priv, arg->cmdbuf_handle, 1);
	if (!cmd_buffer) {
		ret = -EINVAL;
		goto out_err0;
	}

	switch(arg->engine) {
	case PSB_ENGINE_2D:
		ret = psb_cmdbuf_2d(file_priv, arg, cmd_buffer, &fence_arg);
		if (ret)
			goto out_err0;
		break;
	case PSB_ENGINE_VIDEO:
		ret = psb_cmdbuf_video(file_priv, arg, num_buffers, cmd_buffer, &fence_arg);
		break;
	case PSB_ENGINE_RASTERIZER:
		ret = psb_cmdbuf_raster(file_priv, arg, cmd_buffer, &fence_arg);
		if (ret)
			goto out_err0;
		break;
	case PSB_ENGINE_TA:
		if (arg->ta_handle == arg->cmdbuf_handle) {
			mutex_lock(&dev->struct_mutex);
			atomic_inc(&cmd_buffer->usage);
			ta_buffer = cmd_buffer;
			mutex_unlock(&dev->struct_mutex);
		} else {
			ta_buffer = drm_lookup_buffer_object(file_priv, arg->cmdbuf_handle, 1);
			if (!ta_buffer) {
				ret = -EINVAL;
				goto out_err0;
			}
		}

		ret = copy_from_user(&user_scene, 
				     (void __user *) 
				     ((unsigned long) arg->scene_arg), 
				     sizeof(user_scene));
		if (ret)
			goto out_err0;

		if (!user_scene.handle_valid) {
			pool = psb_scene_pool_alloc(file_priv, 0, 
						    user_scene.num_buffers,
						    user_scene.w,
						    user_scene.h);
			if (!pool) {
				ret = -ENOMEM;
				goto out_err0;
			}

			user_scene.handle = psb_scene_pool_handle(pool);
			user_scene.handle_valid = 1;
			ret = copy_to_user((void __user *) 
					   ((unsigned long) arg->scene_arg), 
					   &user_scene,
					   sizeof(user_scene));
					      
			if (ret)
				goto out_err0;
		} else {
			mutex_lock(&dev->struct_mutex);
			pool = psb_scene_pool_lookup_devlocked(file_priv, 
							       user_scene.handle,
							       1);
			mutex_unlock(&dev->struct_mutex);
			if (!pool) {
				ret = -EINVAL;
				goto out_err0;
			}
		}

		ret = psb_validate_scene_pool(pool, 0, 0, 0,
					      user_scene.w,
					      user_scene.h,
					      arg->ta_flags & 
					      PSB_TA_FLAG_LASTPASS,
					      &scene);
		if (ret)
			goto out_err0;

	        ret = psb_cmdbuf_ta(file_priv, arg, cmd_buffer, ta_buffer, 
					scene, &fence_arg);
		if (ret)
			goto out_err0;
		break;
	default:
		DRM_ERROR("Unimplemented command submission mechanism (%x).\n", arg->engine);
		ret = -EINVAL;
		goto out_err0;
	}
	
	if (!(arg->fence_flags & DRM_FENCE_FLAG_NO_USER))
	{
		ret = copy_to_user((void __user *)
				   ((unsigned long) arg->fence_arg),
				   &fence_arg,
				   sizeof(fence_arg));
	}
		
out_err0:
	ret = psb_handle_copyback(dev, dev_priv->buffers, num_buffers, ret, data);
	mutex_lock(&dev->struct_mutex);
	if (scene) 
		psb_scene_unref_devlocked(&scene);
	if (pool)
		psb_scene_pool_unref_devlocked(&pool);
	if (cmd_buffer)
		drm_bo_usage_deref_locked(&cmd_buffer);
	if (ta_buffer)
	        drm_bo_usage_deref_locked(&ta_buffer);
	
	psb_dereference_buffers_locked(dev_priv->buffers, num_buffers);
	mutex_unlock(&dev->struct_mutex);
	mutex_unlock(&dev_priv->cmdbuf_mutex);

	return ret;
}

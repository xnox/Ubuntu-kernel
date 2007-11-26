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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>

#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "psb_drv.h"

struct psbfb_par {
	struct drm_device *dev;
	struct drm_crtc *crtc;
};

#define CMAP_TOHW(_val, _width) ((((_val) << (_width)) + 0x7FFF - (_val)) >> 16)

static int psbfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
	struct psbfb_par *par = info->par;
	struct drm_crtc *crtc = par->crtc;
	u32 v;

	if (regno > 255)
		return 1;

	if (crtc->funcs->gamma_set)
		crtc->funcs->gamma_set(crtc, red, green, blue, regno);

	red = CMAP_TOHW(red, info->var.red.length);
	blue = CMAP_TOHW(blue, info->var.blue.length);
	green = CMAP_TOHW(green, info->var.green.length);
	transp = CMAP_TOHW(transp, info->var.transp.length);

	v = (red << info->var.red.offset) |
	    (green << info->var.green.offset) |
	    (blue << info->var.blue.offset) |
	    (transp << info->var.transp.offset);

	switch (crtc->fb->bits_per_pixel) {
	case 16:
		((u32 *) info->pseudo_palette)[regno] = v;
		break;
	case 24:
	case 32:
		((u32 *) info->pseudo_palette)[regno] = v;
		break;
	}

	return 0;
}

static int psbfb_check_var(struct fb_var_screeninfo *var,
			     struct fb_info *info)
{
        struct psbfb_par *par = info->par;
        struct drm_device *dev = par->dev;
	struct drm_framebuffer *fb = par->crtc->fb;
        struct drm_display_mode *drm_mode;
        struct drm_output *output;
        int depth;
	int pitch;
	int bpp = var->bits_per_pixel;

        if (!var->pixclock)
                return -EINVAL;
	
	/* don't support virtuals for now */
	if (var->xres_virtual > var->xres)
		return -EINVAL;

	if (var->yres_virtual > var->yres)
		return -EINVAL;

        switch (bpp) {
	case 8:
		depth = 8;
		break;
        case 16:
                depth = (var->green.length == 6) ? 16 : 15;
                break;
	case 24: /* assume this is 32bpp / depth 24 */
		bpp = 32;
		/* fallthrough */
        case 32:
                depth = (var->transp.length > 0) ? 32 : 24;
                break;
        default:
		return -EINVAL;
        }

	pitch = ((var->xres * ((bpp + 1) / 8)) + 0x3f) & ~0x3f;

	/* Check that we can resize */
        if ((pitch * var->yres) > (fb->bo->num_pages << PAGE_SHIFT)) {
#if 1
        	/* Need to resize the fb object.
		 * But the generic fbdev code doesn't really understand
		 * that we can do this. So disable for now.
		 */
		DRM_INFO("Can't support requested size, too big!\n");
		return -EINVAL;
#else
		int ret;
		struct drm_buffer_object *fbo = NULL;
		struct drm_bo_kmap_obj tmp_kmap;

		/* a temporary BO to check if we could resize in setpar.
		 * Therefore no need to set NO_EVICT.
		 */
		ret = drm_buffer_object_create(dev, 
			       pitch * var->yres, 
			       drm_bo_type_kernel,
			       DRM_BO_FLAG_READ |
			       DRM_BO_FLAG_WRITE |
			       DRM_BO_FLAG_MEM_TT |
			       DRM_BO_FLAG_MEM_VRAM, 
			       DRM_BO_HINT_DONT_FENCE,
			       0, 0,
			       &fbo);
		if (ret || !fbo)
			return -ENOMEM;
	
		ret = drm_bo_kmap(fbo, 0, fbo->num_pages, &tmp_kmap);
		if (ret) {
			drm_bo_usage_deref_unlocked(&fbo);
			return -EINVAL;
		}

		drm_bo_kunmap(&tmp_kmap);
		/* destroy our current fbo! */
		drm_bo_usage_deref_unlocked(&fbo);
#endif
	}
                
        switch (depth) {
        case 8:
                var->red.offset = 0;
                var->green.offset = 0;
                var->blue.offset = 0;
                var->red.length = 8;
                var->green.length = 8;
                var->blue.length = 8;
                var->transp.length = 0;
                var->transp.offset = 0;
                break;
        case 15:
                var->red.offset = 10;
                var->green.offset = 5;
                var->blue.offset = 0;
                var->red.length = 5;
                var->green.length = 5;
                var->blue.length = 5;
                var->transp.length = 1;
                var->transp.offset = 15;
                break;
        case 16:
                var->red.offset = 11;
                var->green.offset = 6;
                var->blue.offset = 0;
                var->red.length = 5;
                var->green.length = 6;
                var->blue.length = 5;
                var->transp.length = 0;
                var->transp.offset = 0;
                break;
        case 24:
                var->red.offset = 16;
                var->green.offset = 8;
                var->blue.offset = 0;
                var->red.length = 8;
                var->green.length = 8;
                var->blue.length = 8;
                var->transp.length = 0;
                var->transp.offset = 0;
                break;
        case 32:
                var->red.offset = 16;
                var->green.offset = 8;
                var->blue.offset = 0;
                var->red.length = 8;
                var->green.length = 8;
                var->blue.length = 8;
                var->transp.length = 8;
                var->transp.offset = 24;
                break;
        default:
                return -EINVAL; 
        }

#if 0
        /* Here we walk the output mode list and look for modes. If we haven't
         * got it, then bail. Not very nice, so this is disabled.
         * In the set_par code, we create our mode based on the incoming
         * parameters. Nicer, but may not be desired by some.
         */
        list_for_each_entry(output, &dev->mode_config.output_list, head) {
                if (output->crtc == par->crtc)
                        break;
        }
    
        list_for_each_entry(drm_mode, &output->modes, head) {
                if (drm_mode->hdisplay == var->xres &&
                    drm_mode->vdisplay == var->yres &&
                    drm_mode->clock != 0)
			break;
	}
 
        if (!drm_mode)
                return -EINVAL;
#endif

	return 0;
}

/* this will let fbcon do the mode init */
static int psbfb_set_par(struct fb_info *info)
{
	struct psbfb_par *par = info->par;
	struct drm_framebuffer *fb = par->crtc->fb;
	struct drm_device *dev = par->dev;
        struct drm_display_mode *drm_mode;
        struct fb_var_screeninfo *var = &info->var;
	struct drm_psb_private *dev_priv = dev->dev_private;
        struct drm_output *output;
	int pitch;
	int depth;
	int bpp = var->bits_per_pixel;

        switch (bpp) {
	case 8:
		depth = 8;
		break;
        case 16:
                depth = (var->green.length == 6) ? 16 : 15;
                break;
	case 24: /* assume this is 32bpp / depth 24 */
		bpp = 32;
		/* fallthrough */
        case 32:
                depth = (var->transp.length > 0) ? 32 : 24;
                break;
        default:
		return -EINVAL;
        }

	pitch = ((var->xres * ((bpp + 1) / 8)) + 0x3f) & ~0x3f;

        if ((pitch * var->yres) > (fb->bo->num_pages << PAGE_SHIFT)) {
#if 1
        	/* Need to resize the fb object.
		 * But the generic fbdev code doesn't really understand
		 * that we can do this. So disable for now.
		 */
		DRM_INFO("Can't support requested size, too big!\n");
		return -EINVAL;
#else
		int ret;
		struct drm_buffer_object *fbo = NULL, *tfbo;
		struct drm_bo_kmap_obj tmp_kmap, tkmap;

		ret = drm_buffer_object_create(dev, 
			       pitch * var->yres, 
			       drm_bo_type_kernel,
			       DRM_BO_FLAG_READ |
			       DRM_BO_FLAG_WRITE |
			       DRM_BO_FLAG_MEM_TT |
			       DRM_BO_FLAG_MEM_VRAM |
			       DRM_BO_FLAG_NO_EVICT,
			       DRM_BO_HINT_DONT_FENCE,
			       0, 0,
			       &fbo);
		if (ret || !fbo) {
			DRM_ERROR("failed to allocate new resized framebuffer\n");
			return -ENOMEM;
		}
	
		ret = drm_bo_kmap(fbo, 0, fbo->num_pages, &tmp_kmap);
		if (ret) {
			DRM_ERROR("failed to kmap framebuffer.\n");
			drm_bo_usage_deref_unlocked(&fbo);
			return -EINVAL;
		}

		DRM_DEBUG("allocated %dx%d fb: 0x%08lx, bo %p\n", fb->width,
			       fb->height, fb->offset, fbo);

		/* set new screen base */
		info->screen_base = tmp_kmap.virtual;

		tkmap = fb->kmap;
		fb->kmap = tmp_kmap;
		drm_bo_kunmap(&tkmap);

		tfbo = fb->bo;
		fb->bo = fbo;
		drm_bo_usage_deref_unlocked(&tfbo);
#endif
        } 
	else 
	{
		drm_bo_do_validate(fb->bo, 
			   DRM_BO_FLAG_READ |
			   DRM_BO_FLAG_WRITE |
			   DRM_BO_FLAG_MEM_TT |
			   DRM_BO_FLAG_MEM_VRAM |
			   DRM_BO_FLAG_NO_EVICT,
			   DRM_BO_FLAG_READ |
			   DRM_BO_FLAG_WRITE |
			   DRM_BO_FLAG_MEM_TT |
			   DRM_BO_FLAG_MEM_VRAM |
			   DRM_BO_FLAG_NO_EVICT,
			   DRM_BO_HINT_DONT_FENCE,
			   0, 1, NULL);
	}

	fb->offset = fb->bo->offset - dev_priv->pg->gatt_start;
	fb->width = var->xres;
	fb->height = var->yres;
        fb->bits_per_pixel = bpp;
	fb->pitch = pitch;
	fb->depth = depth;

        info->fix.line_length = fb->pitch;
        info->fix.visual = (fb->depth == 8) ? FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_DIRECTCOLOR;

	/* some fbdev's apps don't want these to change */
	info->fix.smem_start = dev->mode_config.fb_base + fb->offset;

#if 0
	/* relates to resize - disable */
        info->fix.smem_len = info->fix.line_length * var->yres;
        info->screen_size = info->fix.smem_len; /* ??? */
#endif

        /* Should we walk the output's modelist or just create our own ???
         * For now, we create and destroy a mode based on the incoming 
         * parameters. But there's commented out code below which scans 
         * the output list too.
         */
#if 0
        list_for_each_entry(output, &dev->mode_config.output_list, head) {
                if (output->crtc == par->crtc)
                        break;
        }
    
        list_for_each_entry(drm_mode, &output->modes, head) {
                if (drm_mode->hdisplay == var->xres &&
                    drm_mode->vdisplay == var->yres &&
                    drm_mode->clock != 0)
			break;
        }
#else
        drm_mode = drm_mode_create(dev);
        drm_mode->hdisplay = var->xres;
        drm_mode->hsync_start = drm_mode->hdisplay + var->right_margin;
        drm_mode->hsync_end = drm_mode->hsync_start + var->hsync_len;
        drm_mode->htotal = drm_mode->hsync_end + var->left_margin;
        drm_mode->vdisplay = var->yres;
        drm_mode->vsync_start = drm_mode->vdisplay + var->lower_margin;
        drm_mode->vsync_end = drm_mode->vsync_start + var->vsync_len;
        drm_mode->vtotal = drm_mode->vsync_end + var->upper_margin;
        drm_mode->clock = PICOS2KHZ(var->pixclock);
        drm_mode->vrefresh = drm_mode_vrefresh(drm_mode);
        drm_mode_set_name(drm_mode);
	drm_mode_set_crtcinfo(drm_mode, CRTC_INTERLACE_HALVE_V);
#endif

        if (!drm_crtc_set_mode(par->crtc, drm_mode, 0, 0))
                return -EINVAL;

        /* Have to destroy our created mode if we're not searching the mode
         * list for it.
         */
#if 1 
        drm_mode_destroy(dev, drm_mode);
#endif

	return 0;
}

static void psbfb_fillrect (struct fb_info *info, const struct fb_fillrect *rect)
{
	if (info->state != FBINFO_STATE_RUNNING)
                return;
        if (info->flags & FBINFO_HWACCEL_DISABLED) {
                cfb_fillrect(info, rect);
                return;
        }
        cfb_fillrect(info, rect);
}

static void psbfb_copyarea(struct fb_info *info,
			     const struct fb_copyarea *region)
{
	if (info->state != FBINFO_STATE_RUNNING)
                return;
        if (info->flags & FBINFO_HWACCEL_DISABLED) {
                cfb_copyarea(info, region);
                return;
        }
        cfb_copyarea(info, region);
}

void psbfb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	if (info->state != FBINFO_STATE_RUNNING)
                return;
        if (info->flags & FBINFO_HWACCEL_DISABLED) {
                cfb_imageblit(info, image);
                return;
        }
        cfb_imageblit(info, image);
}

static struct fb_ops psbfb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = psbfb_check_var,
	.fb_set_par = psbfb_set_par,
	.fb_setcolreg = psbfb_setcolreg,
	.fb_fillrect = psbfb_fillrect,
	.fb_copyarea = psbfb_copyarea,
	.fb_imageblit = psbfb_imageblit,
};

int psbfb_probe(struct drm_device *dev, struct drm_crtc *crtc)
{
	struct fb_info *info;
	struct psbfb_par *par;
	struct device *device = &dev->pdev->dev; 
	struct drm_framebuffer *fb;
	struct drm_display_mode *mode = crtc->desired_mode;
	struct drm_psb_private *dev_priv = (struct drm_psb_private *) dev->dev_private;
	struct drm_buffer_object *fbo = NULL;
	int ret;

	if (drm_psb_no_fb) {
		/* need to do this as the DRM will disable the output */
		crtc->enabled = 1;
		return 0;
	}

	info = framebuffer_alloc(sizeof(struct psbfb_par), device);
	if (!info){
		return -ENOMEM;
	}

	fb = drm_framebuffer_create(dev);
	if (!fb) {
		framebuffer_release(info);
		DRM_ERROR("failed to allocate fb.\n");
		return -ENOMEM;
	}
	crtc->fb = fb;

	fb->width = mode->hdisplay;
	fb->height = mode->vdisplay;

	fb->bits_per_pixel = 32;
	fb->depth = 24;
	fb->pitch = ((fb->width * ((fb->bits_per_pixel + 1) / 8)) + 0x3f) & ~0x3f;

	ret = drm_buffer_object_create(dev, 
				       fb->pitch * fb->height, 
				       drm_bo_type_kernel,
				       DRM_BO_FLAG_READ |
				       DRM_BO_FLAG_WRITE |
			       	       DRM_BO_FLAG_MEM_TT |
				       DRM_BO_FLAG_MEM_VRAM |
			       	       DRM_BO_FLAG_NO_EVICT,
				       DRM_BO_HINT_DONT_FENCE,
				       0, 0,
				       &fbo);
	if (ret || !fbo) {
		DRM_ERROR("failed to allocate framebuffer\n");
		drm_framebuffer_destroy(fb);
		framebuffer_release(info);
		crtc->fb = NULL;
		return -ENOMEM;
	}
	fb->offset = fbo->offset - dev_priv->pg->gatt_start;
	fb->bo = fbo;
	DRM_DEBUG("allocated %dx%d fb: 0x%08lx, bo %p\n", fb->width,
		       fb->height, fb->offset, fbo);

	fb->fbdev = info;
		
	par = info->par;

	par->dev = dev;
	par->crtc = crtc;

	info->fbops = &psbfb_ops;

	strcpy(info->fix.id, "psbfb");
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_DIRECTCOLOR;
	info->fix.type_aux = 0;
	info->fix.xpanstep = 1;
	info->fix.ypanstep = 1;
	info->fix.ywrapstep = 0;
	info->fix.accel = FB_ACCEL_NONE; /* ??? */
	info->fix.type_aux = 0;
	info->fix.mmio_start = 0;
	info->fix.mmio_len = 0;
	info->fix.line_length = fb->pitch;
	info->fix.smem_start = dev->mode_config.fb_base + fb->offset;
	info->fix.smem_len = info->fix.line_length * fb->height;

	info->flags = FBINFO_DEFAULT |
			FBINFO_PARTIAL_PAN_OK /*| FBINFO_MISC_ALWAYS_SETPAR*/;
	
	ret = drm_bo_kmap(fb->bo, 0, fb->bo->num_pages, &fb->kmap);
	if (ret) {
		DRM_ERROR("error mapping fb: %d\n", ret);
		drm_framebuffer_destroy(fb);
		framebuffer_release(info);
		crtc->fb = NULL;
		return -EINVAL;
	}

	info->screen_base = fb->kmap.virtual;
	info->screen_size = info->fix.smem_len; /* FIXME */
	info->pseudo_palette = fb->pseudo_palette;
	info->var.xres_virtual = fb->width;
	info->var.yres_virtual = fb->height;
	info->var.bits_per_pixel = fb->bits_per_pixel;
	info->var.xoffset = 0;
	info->var.yoffset = 0;
	info->var.activate = FB_ACTIVATE_NOW;
	info->var.height = -1;
	info->var.width = -1;
	info->var.vmode = FB_VMODE_NONINTERLACED;

        info->var.xres = mode->hdisplay;
        info->var.right_margin = mode->hsync_start - mode->hdisplay;
        info->var.hsync_len = mode->hsync_end - mode->hsync_start;
        info->var.left_margin = mode->htotal - mode->hsync_end;
        info->var.yres = mode->vdisplay;
        info->var.lower_margin = mode->vsync_start - mode->vdisplay;
        info->var.vsync_len = mode->vsync_end - mode->vsync_start;
	info->var.upper_margin = mode->vtotal - mode->vsync_end;
        info->var.pixclock = 10000000 / mode->htotal * 1000 /
		mode->vtotal * 100;
	/* avoid overflow */
	info->var.pixclock = info->var.pixclock * 1000 / mode->vrefresh;

	info->pixmap.size = 64*1024;
	info->pixmap.buf_align = 8;
	info->pixmap.access_align = 32;
	info->pixmap.flags = FB_PIXMAP_SYSTEM;
	info->pixmap.scan_align = 1;

	DRM_DEBUG("fb depth is %d\n", fb->depth);
	DRM_DEBUG("   pitch is %d\n", fb->pitch);
	switch(fb->depth) {
	case 8:
                info->var.red.offset = 0;
                info->var.green.offset = 0;
                info->var.blue.offset = 0;
                info->var.red.length = 8; /* 8bit DAC */
                info->var.green.length = 8;
                info->var.blue.length = 8;
                info->var.transp.offset = 0;
                info->var.transp.length = 0;
                break;
 	case 15:
                info->var.red.offset = 10;
                info->var.green.offset = 5;
                info->var.blue.offset = 0;
                info->var.red.length = info->var.green.length =
                        info->var.blue.length = 5;
                info->var.transp.offset = 15;
                info->var.transp.length = 1;
                break;
	case 16:
                info->var.red.offset = 11;
                info->var.green.offset = 5;
                info->var.blue.offset = 0;
                info->var.red.length = 5;
                info->var.green.length = 6;
                info->var.blue.length = 5;
                info->var.transp.offset = 0;
 		break;
	case 24:
		info->var.red.offset = 16;
		info->var.green.offset = 8;
		info->var.blue.offset = 0;
		info->var.red.length = info->var.green.length =
			info->var.blue.length = 8;
		info->var.transp.offset = 0;
		info->var.transp.length = 0;
		break;
	case 32:
		info->var.red.offset = 16;
		info->var.green.offset = 8;
		info->var.blue.offset = 0;
		info->var.red.length = info->var.green.length =
			info->var.blue.length = 8;
		info->var.transp.offset = 24;
		info->var.transp.length = 8;
		break;
	default:
		break;
	}

	if (register_framebuffer(info) < 0) {
		drm_framebuffer_destroy(fb);
		framebuffer_release(info);
		crtc->fb = NULL;
		return -EINVAL;
	}

        if (psbfb_check_var(&info->var, info) < 0) {
		drm_framebuffer_destroy(fb);
		framebuffer_release(info);
		crtc->fb = NULL;
		return -EINVAL;
	}

        psbfb_set_par(info);

	DRM_INFO("fb%d: %s frame buffer device\n", info->node,
	       info->fix.id);
	return 0;
}
EXPORT_SYMBOL(psbfb_probe);

int psbfb_remove(struct drm_device *dev, struct drm_crtc *crtc)
{
	struct drm_framebuffer *fb;
	struct fb_info *info;

	if (drm_psb_no_fb)
		return 0;

	fb = crtc->fb;
	info = fb->fbdev;
	
	if (info) {
		unregister_framebuffer(info);
		framebuffer_release(info);
		drm_bo_kunmap(&fb->kmap);
		drm_bo_usage_deref_unlocked(&fb->bo);
	}
	return 0;
}
EXPORT_SYMBOL(psbfb_remove);

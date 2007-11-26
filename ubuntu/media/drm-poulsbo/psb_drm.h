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

#ifndef _PSB_DRM_H_
#define _PSB_DRM_H_

#if defined(__linux__) && !defined(__KERNEL__)
#include<stdint.h>
#endif

#define DRM_PSB_SAREA_MAJOR 0
#define DRM_PSB_SAREA_MINOR 1
#define PSB_FIXED_SHIFT 16

/*
 * Public memory types.
 */

#define DRM_PSB_MEM_MMU DRM_BO_MEM_PRIV1
#define DRM_PSB_FLAG_MEM_MMU DRM_BO_FLAG_MEM_PRIV1
#define DRM_PSB_MEM_PDS DRM_BO_MEM_PRIV2
#define DRM_PSB_FLAG_MEM_PDS DRM_BO_FLAG_MEM_PRIV2
#define DRM_PSB_MEM_APER DRM_BO_MEM_PRIV3
#define DRM_PSB_FLAG_MEM_APER DRM_BO_FLAG_MEM_PRIV3
#define DRM_PSB_MEM_RASTGEOM DRM_BO_MEM_PRIV4
#define DRM_PSB_FLAG_MEM_RASTGEOM DRM_BO_FLAG_MEM_PRIV4


typedef int32_t psb_fixed;
typedef uint32_t psb_ufixed;

static inline psb_fixed psb_int_to_fixed(int a)
{
	return a * (1 << PSB_FIXED_SHIFT);
}

static inline psb_ufixed psb_unsigned_to_ufixed(unsigned int a)
{
	return a << PSB_FIXED_SHIFT;
}

/*Status of the command sent to the gfx device.*/
typedef enum {
        DRM_CMD_SUCCESS,
        DRM_CMD_FAILED,
        DRM_CMD_HANG
} drm_cmd_status_t;

struct drm_psb_scanout {
	uint32_t buffer_id;	/* DRM buffer object ID */
	uint32_t rotation;	/* Rotation as in RR_rotation definitions */
	uint32_t stride;	/* Buffer stride in bytes */
	uint32_t depth;		/* Buffer depth in bits (NOT) bpp */
	uint32_t width;		/* Buffer width in pixels */
	uint32_t height;	/* Buffer height in lines */
	psb_fixed transform[3][3];	/* Buffer composite transform */
	/* (scaling, rot, reflect) */
};

#define DRM_PSB_SAREA_OWNERS 16
#define DRM_PSB_SAREA_OWNER_2D 0
#define DRM_PSB_SAREA_OWNER_3D 1

#define DRM_PSB_SAREA_SCANOUTS 3

struct drm_psb_sarea {
	/* Track changes of this data structure */

	uint32_t major;
	uint32_t minor;

	/* Last context to touch part of hw */
	uint32_t ctx_owners[DRM_PSB_SAREA_OWNERS];

	/* Definition of front- and rotated buffers */
	uint32_t num_scanouts;
	struct drm_psb_scanout scanouts[DRM_PSB_SAREA_SCANOUTS];

        int pipeA_x;
        int pipeA_y;
        int pipeA_w;
        int pipeA_h;
        int pipeB_x;
        int pipeB_y;
        int pipeB_w;
        int pipeB_h;

	uint32_t msvdx_state;
	uint32_t msvdx_context;
};

#define PSB_RELOC_MAGIC         0x67676767
#define PSB_RELOC_SHIFT_MASK    0x0000FFFF
#define PSB_RELOC_SHIFT_SHIFT   0
#define PSB_RELOC_ALSHIFT_MASK  0xFFFF0000
#define PSB_RELOC_ALSHIFT_SHIFT 16

#define PSB_RELOC_OP_OFFSET     0	/* Offset of the indicated
					 * buffer 
					 */
#define PSB_RELOC_OP_2D_OFFSET  1	/* Offset of the indicated
					 *  buffer, relative to 2D
					 *  base address
					 */
#define PSB_RELOC_OP_PDS_OFFSET 2       /* Offset of the indicated buffer, 
					 *  relative to PDS base address 
					 */
#define PSB_RELOC_OP_STRIDE     3       /* Stride of the indicated
					 * buffer (for tiling)
					 */
#define PSB_RELOC_OP_USE_OFFSET 4       /* Offset of USE buffer
					 * relative to base reg 
					 */
#define PSB_RELOC_OP_USE_REG    5       /* Base reg of USE buffer */



struct drm_psb_reloc {
	uint32_t reloc_op;
	uint32_t where;                     /* offset in destination buffer */
	uint32_t buffer;                    /* Buffer reloc applies to */
	uint32_t mask;                      /* Destination format: */
	uint32_t shift;                     /* Destination format: */
	uint32_t pre_add;                   /* Destination format: */
	uint32_t background;                /* Destination add */
	uint32_t dst_buffer;                /* Destination buffer. Index into buffer_list*/
        uint32_t arg0;                      /* Reloc-op dependant */
	uint32_t arg1;
};

#define PSB_BO_FLAG_TA          0x00100000
#define PSB_BO_FLAG_SCENE           0x00200000

#define PSB_ENGINE_2D 0
#define PSB_ENGINE_VIDEO 1
#define PSB_ENGINE_RASTERIZER 2
#define PSB_ENGINE_TA 3
#define PSB_ENGINE_HPRAST 4

/*
 * For this fence class we have a couple of
 * fence types.
 */

#define _PSB_FENCE_EXE_SHIFT         0
#define _PSB_FENCE_TA_DONE_SHIFT 1
#define _PSB_FENCE_RASTER_DONE_SHIFT 2
#define _PSB_FENCE_SCENE_DONE_SHIFT  3

#define _PSB_ENGINE_TA_FENCE_TYPES   4
#define _PSB_FENCE_TYPE_TA_DONE (1 << _PSB_FENCE_TA_DONE_SHIFT)
#define _PSB_FENCE_TYPE_RASTER_DONE (1 << _PSB_FENCE_RASTER_DONE_SHIFT)
#define _PSB_FENCE_TYPE_SCENE_DONE  (1 << _PSB_FENCE_SCENE_DONE_SHIFT)


#define PSB_ENGINE_HPRAST 4
#define PSB_NUM_ENGINES 5

#define PSB_TA_FLAG_FIRSTPASS   (1 << 0)
#define PSB_TA_FLAG_LASTPASS    (1 << 1)

struct drm_psb_scene {
	int handle_valid;
	uint32_t handle;
	uint32_t w;
	uint32_t h;
	uint32_t num_buffers;
};

typedef struct drm_psb_cmdbuf_arg {
	uint64_t buffer_list;	/* List of buffers to validate */
	uint64_t clip_rects;	/* See i915 counterpart */
	uint64_t scene_arg;
	uint64_t fence_arg;

	uint32_t ta_flags;

	uint32_t ta_handle;     /* Binner reg-value pairs */
        uint32_t ta_offset;     
        uint32_t ta_size;

	uint32_t cmdbuf_handle;	/* 2D Command buffer object or, */
	uint32_t cmdbuf_offset; /* rasterizer reg-value pairs */ 
	uint32_t cmdbuf_size;
	
	uint32_t reloc_handle;	/* Reloc buffer object */
	uint32_t reloc_offset;
	uint32_t num_relocs;

	int32_t damage;		/* Damage front buffer with cliprects */
	/* Not implemented yet */
	uint32_t fence_flags;
	uint32_t engine;
	
	/*
	 * Scene and fence.
	 */

} drm_psb_cmdbuf_arg_t;

struct drm_psb_xhw_init_arg {
        uint32_t operation;
	uint32_t buffer_handle;
};

#define PSB_HW_COOKIE_SIZE 16

struct drm_psb_xhw_arg {
	uint32_t op;
	int ret;
	uint32_t cookie[PSB_HW_COOKIE_SIZE];
	union {
		struct {
			uint32_t w;
			uint32_t h;
			uint32_t size;
			uint32_t clear_p_start;
			uint32_t clear_num_pages;
		} si;
		struct {
			uint32_t fire_flags;
			uint32_t hw_context;
			uint32_t offset;
			uint32_t engine;
			uint32_t flags;
		} sb;
		struct {
			uint32_t pages;
			uint32_t size;
		} bi;
		struct {
			uint32_t bca;
			uint32_t rca;
			uint32_t flags;
		} oom;	             
	} arg;
};

#define DRM_PSB_CMDBUF          0x00 
#define DRM_PSB_XHW_INIT        0x01
#define DRM_PSB_XHW             0x02

#define PSB_XHW_INIT            0x00
#define PSB_XHW_TAKEDOWN        0x01

#define PSB_XHW_FIRE_RASTER     0x00
#define PSB_XHW_SCENE_INFO      0x01
#define PSB_XHW_SCENE_BIND_FIRE 0x02
#define PSB_XHW_BIN_MEM_INFO    0x03
#define PSB_XHW_RESET_DPM       0x04
#define PSB_XHW_OOM             0x05
#define PSB_XHW_TERMINATE       0x06


struct drm_psb_dev_info_arg {
	uint32_t num_use_attribute_registers;
};
#define DRM_PSB_DEVINFO         0x01

#endif

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
#ifndef _PSB_DRV_H_
#define _PSB_DRV_H_

#include "drmP.h"
#include "psb_drm.h"
#include "psb_reg.h"
#include "psb_schedule.h"

enum {
	CHIP_PSB_8108 = 0
};

#define DRIVER_NAME "psb"
#define DRIVER_DESC "drm driver for the Intel Poulsbo chipset"
#define DRIVER_AUTHOR "Tungsten Graphics Inc."

#define PSB_DRM_DRIVER_DATE "20071017"
#define PSB_DRM_DRIVER_MAJOR 0
#define PSB_DRM_DRIVER_MINOR 9 
#define PSB_DRM_DRIVER_PATCHLEVEL 0


#define PSB_VDC_OFFSET           0x00000000
#define PSB_VDC_SIZE             0x000080000
#define PSB_SGX_SAVE_SIZE        0x1000
#define PSB_SGX_SIZE             0x8000
#define PSB_SGX_OFFSET           0x00040000
#define PSB_MMIO_RESOURCE        0
#define PSB_GATT_RESOURCE        2
#define PSB_GTT_RESOURCE         3
#define PSB_GMCH_CTRL            0x52
#define PSB_BSM                  0x5C
#define _PSB_GMCH_ENABLED        0x4
#define PSB_PGETBL_CTL           0x2020
#define _PSB_PGETBL_ENABLED      0x00000001
#define PSB_SGX_2D_SLAVE_PORT    0x4000
#define PSB_TT_PRIV0_LIMIT       (256*1024*1024)
#define PSB_TT_PRIV0_PLIMIT      (PSB_TT_PRIV0_LIMIT >> PAGE_SHIFT)
#define PSB_NUM_VALIDATE_BUFFERS 512
#define PSB_MEM_KERNEL_START     0x10000000
#define PSB_MEM_PDS_START        0x20000000
#define PSB_MEM_RASTGEOM_START   0x30000000
#define PSB_MEM_MMU_START        0x40000000

#define DRM_PSB_MEM_KERNEL       DRM_BO_MEM_PRIV0
#define DRM_PSB_FLAG_MEM_KERNEL  DRM_BO_FLAG_MEM_PRIV0

/*
 * Flags for external memory type field.
 */

#define PSB_MSVDX_OFFSET        0x50000         /*MSVDX Base offset*/
#define PSB_MSVDX_SIZE          0x8000          /*MSVDX MMIO region is 0x50000 - 0x57fff ==> 32KB*/

#define PSB_MMU_CACHED_MEMORY     0x0001	/* Bind to MMU only */
#define PSB_MMU_RO_MEMORY         0x0002	/* MMU RO memory */
#define PSB_MMU_WO_MEMORY         0x0004	/* MMU WO memory */

/*
 * PTE's and PDE's
 */

#define PSB_PDE_MASK              0x003FFFFF
#define PSB_PDE_SHIFT             22
#define PSB_PTE_SHIFT             12

#define PSB_PTE_VALID             0x0001	/* PTE / PDE valid */
#define PSB_PTE_WO                0x0002	/* Write only */
#define PSB_PTE_RO                0x0004	/* Read only */
#define PSB_PTE_CACHED            0x0008	/* CPU cache coherent */

/*
 * VDC registers and bits
 */
#define PSB_HWSTAM                0x2098
#define PSB_INSTPM                0x20C0
#define PSB_INT_IDENTITY_R        0x20A4
#define _PSB_VSYNC_PIPEB_FLAG     (1<<5)
#define _PSB_VSYNC_PIPEA_FLAG     (1<<7)
#define _PSB_IRQ_SGX_FLAG         (1<<18)
#define _PSB_IRQ_MSVDX_FLAG       (1<<19)
#define PSB_INT_MASK_R            0x20A8
#define PSB_INT_ENABLE_R          0x20A0
#define PSB_PIPEASTAT             0x70024
#define _PSB_VBLANK_INTERRUPT_ENABLE (1 << 17)
#define _PSB_VBLANK_CLEAR         (1 << 1)
#define PSB_PIPEBSTAT             0x71024

#define _PSB_MMU_ER_MASK      0x0001FF00
#define _PSB_MMU_ER_HOST      (1 << 16)
#define GPIOA			0x5010
#define GPIOB			0x5014
#define GPIOC			0x5018
#define GPIOD			0x501c
#define GPIOE			0x5020
#define GPIOF			0x5024
#define GPIOG			0x5028
#define GPIOH			0x502c
#define GPIO_CLOCK_DIR_MASK		(1 << 0)
#define GPIO_CLOCK_DIR_IN		(0 << 1)
#define GPIO_CLOCK_DIR_OUT		(1 << 1)
#define GPIO_CLOCK_VAL_MASK		(1 << 2)
#define GPIO_CLOCK_VAL_OUT		(1 << 3)
#define GPIO_CLOCK_VAL_IN		(1 << 4)
#define GPIO_CLOCK_PULLUP_DISABLE	(1 << 5)
#define GPIO_DATA_DIR_MASK		(1 << 8)
#define GPIO_DATA_DIR_IN		(0 << 9)
#define GPIO_DATA_DIR_OUT		(1 << 9)
#define GPIO_DATA_VAL_MASK		(1 << 10)
#define GPIO_DATA_VAL_OUT		(1 << 11)
#define GPIO_DATA_VAL_IN		(1 << 12)
#define GPIO_DATA_PULLUP_DISABLE	(1 << 13)

#define VCLK_DIVISOR_VGA0   0x6000
#define VCLK_DIVISOR_VGA1   0x6004
#define VCLK_POST_DIV       0x6010

#define DRM_DRIVER_PRIVATE_T struct drm_psb_private
#define I915_WRITE(_offs, _val) \
  iowrite32(_val, dev_priv->vdc_reg + (_offs))
#define I915_READ(_offs) \
  ioread32(dev_priv->vdc_reg + (_offs))

#define PSB_COMM_2D (PSB_ENGINE_2D << 4)
#define PSB_COMM_3D (PSB_ENGINE_3D << 4)
#define PSB_COMM_TA (PSB_ENGINE_TA << 4)
#define PSB_COMM_HP (PSB_ENGINE_HP << 4)
#define PSB_COMM_FW (2048 >> 2)


#define PSB_2D_SIZE (256*1024*1024)
#define PSB_MAX_RELOC_PAGES 1024

#define PSB_LOW_REG_OFFS 0x0204
#define PSB_HIGH_REG_OFFS 0x0600

#define PSB_NUM_VBLANKS 2

#define PSB_COMM_2D (PSB_ENGINE_2D << 4)
#define PSB_COMM_3D (PSB_ENGINE_3D << 4)
#define PSB_COMM_TA (PSB_ENGINE_TA << 4)
#define PSB_COMM_HP (PSB_ENGINE_HP << 4)
#define PSB_COMM_FW (2048 >> 2)


#define PSB_2D_SIZE (256*1024*1024)
#define PSB_MAX_RELOC_PAGES 1024

#define PSB_LOW_REG_OFFS 0x0204
#define PSB_HIGH_REG_OFFS 0x0600

#define PSB_NUM_VBLANKS 2
#define PSB_WATCHDOG_DELAY (DRM_HZ / 10)

#define PSB_RASTER_DEALLOC (1 << 0)

/*
 * User options.
 */

struct drm_psb_uopt{
	int disable_clock_gating;
};


struct psb_gtt{
        struct drm_device *dev;
        int initialized;
	u32 gatt_start;
	u32 gtt_start;
	u32 gtt_phys_start;
	unsigned gtt_pages;
        unsigned gatt_pages;
	u32 stolen_base;
	u32 pge_ctl;
	u16 gmch_ctrl;
	unsigned long stolen_size;
	u32 *gtt_map;
	struct rw_semaphore sem;
};

struct psb_use_base {
	struct list_head head;
	struct drm_fence_object *fence;
	unsigned int reg;
	unsigned long offset;
	unsigned int dm;
};

struct psb_buflist_item {
	struct drm_buffer_object *bo;
	void __user *data;
	struct drm_bo_info_rep rep;
	int ret;
};

struct drm_psb_private {
	unsigned long chipset;
  
	struct drm_psb_dev_info_arg dev_info;
	struct drm_psb_uopt uopt;

	struct psb_gtt *pg;

	struct page *scratch_page;
	struct page *comm_page;

	volatile u32 *comm;
	u32 comm_mmu_offset;
	u32 mmu_2d_offset;
	u32 sequence[PSB_NUM_ENGINES];
        u32 last_submitted_seq[PSB_NUM_ENGINES];
	u32 *sgx_save;
	int engine_lockup_2d;

	struct psb_mmu_driver *mmu;
	struct psb_mmu_pd *pf_pd;

	u8 *sgx_reg;
	u8 *vdc_reg;
	u8 *msvdx_reg; /*MSVDX*/
        int msvdx_needs_reset;
	int has_msvdx;
	u32 gatt_free_offset;

	/*
	 * Fencing / irq.
	 */

	u32 sgx_irq_mask;
	u32 vdc_irq_mask;
       
	spinlock_t irqmask_lock;
	int fence0_irq_on;
	int irq_enabled;
    unsigned int irqen_count_2d;
	wait_queue_head_t event_2d_queue;
	
	u32 msvdx_current_sequence;
	u32 msvdx_last_sequence;
    int fence2_irq_on;

    /*
     * MSVDX Rendec Memory
     */
    struct drm_buffer_object *ccb0;
    u32 base_addr0;
    struct drm_buffer_object *ccb1;
    u32 base_addr1;

	/*
	 * Memory managers
	 */

	int have_vram;
	int have_tt;
	int have_mem_mmu;
	int have_mem_aper;
	int have_mem_kernel;
        int have_mem_pds;
        int have_mem_rastgeom;
	struct mutex temp_mem;

	/*
	 * Relocation buffer mapping.
	 */

	spinlock_t reloc_lock;
	unsigned int rel_mapped_pages;
	wait_queue_head_t rel_mapped_queue;


	/*
	 * SAREA
	 */
	struct drm_psb_sarea *sarea_priv;

	/*
	 * LVDS info 
	 */
	int backlight_duty_cycle;  /* restore backlight to this value */
	bool panel_wants_dither;
	struct drm_display_mode *panel_fixed_mode;

	/* 
	 * Register state 
	 */
	u32 saveDSPACNTR;
	u32 saveDSPBCNTR;
	u32 savePIPEACONF;
	u32 savePIPEBCONF;
	u32 savePIPEASRC;
	u32 savePIPEBSRC;
	u32 saveFPA0;
	u32 saveFPA1;
	u32 saveDPLL_A;
	u32 saveDPLL_A_MD;
	u32 saveHTOTAL_A;
	u32 saveHBLANK_A;
	u32 saveHSYNC_A;
	u32 saveVTOTAL_A;
	u32 saveVBLANK_A;
	u32 saveVSYNC_A;
	u32 saveDSPASTRIDE;
	u32 saveDSPASIZE;
	u32 saveDSPAPOS;
	u32 saveDSPABASE;
	u32 saveDSPASURF;
	u32 saveFPB0;
	u32 saveFPB1;
	u32 saveDPLL_B;
	u32 saveDPLL_B_MD;
	u32 saveHTOTAL_B;
	u32 saveHBLANK_B;
	u32 saveHSYNC_B;
	u32 saveVTOTAL_B;
	u32 saveVBLANK_B;
	u32 saveVSYNC_B;
	u32 saveDSPBSTRIDE;
	u32 saveDSPBSIZE;
	u32 saveDSPBPOS;
	u32 saveDSPBBASE;
	u32 saveDSPBSURF;
	u32 saveVCLK_DIVISOR_VGA0;
	u32 saveVCLK_DIVISOR_VGA1;
	u32 saveVCLK_POST_DIV;
	u32 saveVGACNTRL;
	u32 saveADPA;
	u32 saveLVDS;
	u32 saveDVOA;
	u32 saveDVOB;
	u32 saveDVOC;
	u32 savePP_ON;
	u32 savePP_OFF;
	u32 savePP_CONTROL;
	u32 savePP_CYCLE;
	u32 savePFIT_CONTROL;
	u32 savePaletteA[256];
	u32 savePaletteB[256];
	u32 saveBLC_PWM_CTL;

	/*
	 * USE code base register management.
	 */
        
	struct drm_reg_manager use_manager;
  
	/*
	 * Xhw
	 */

	uint32_t *xhw;
	struct drm_buffer_object *xhw_bo;
	struct drm_bo_kmap_obj xhw_kmap;
	struct list_head xhw_in;
	spinlock_t xhw_lock;
        atomic_t xhw_client;
        struct drm_file *xhw_file;
	wait_queue_head_t xhw_queue;
	wait_queue_head_t xhw_caller_queue;
	struct mutex xhw_mutex;
	struct psb_xhw_buf *xhw_cur_buf;
        int xhw_submit_ok;
        int xhw_on;

	/*
	 * Scheduling.
	 */

        struct mutex reset_mutex;
		struct mutex cmdbuf_mutex;
        struct psb_scheduler scheduler;
        struct psb_buflist_item buffers[PSB_NUM_VALIDATE_BUFFERS];

	/*
	 * Watchdog
	 */
	
	spinlock_t watchdog_lock;
	struct timer_list watchdog_timer;
	struct work_struct watchdog_wq;
	struct work_struct msvdx_watchdog_wq;
	int timer_available;
};

struct psb_mmu_driver;

extern struct psb_mmu_driver *psb_mmu_driver_init(u8 __iomem * registers);
extern void psb_mmu_driver_takedown(struct psb_mmu_driver *driver);
extern struct psb_mmu_pd *psb_mmu_get_default_pd(struct psb_mmu_driver *driver);
extern void psb_mmu_mirror_gtt(struct psb_mmu_pd *pd, u32 mmu_offset,
			       u32 gtt_start, __u32 gtt_pages);
extern void psb_mmu_test(struct psb_mmu_driver *driver, u32 offset);
extern struct psb_mmu_pd *psb_mmu_alloc_pd(struct psb_mmu_driver *driver);
extern void psb_mmu_free_pagedir(struct psb_mmu_pd *pd);
extern void psb_mmu_flush(struct psb_mmu_driver *driver);
extern void psb_mmu_remove_pfn_sequence(struct psb_mmu_pd *pd,
					 unsigned long address,
					 uint32_t num_pages);
extern int psb_mmu_insert_pfn_sequence(struct psb_mmu_pd *pd, uint32_t start_pfn,
				       unsigned long address, uint32_t num_pages,
				       int type);

/*
 * Enable / disable MMU for different requestors.
 */

extern void psb_mmu_enable_requestor(struct psb_mmu_driver *driver, u32 mask);
extern void psb_mmu_disable_requestor(struct psb_mmu_driver *driver,
				      u32 mask);
extern void psb_mmu_set_pd_context(struct psb_mmu_pd *pd, int hw_context);
extern int psb_mmu_insert_pages(struct psb_mmu_pd *pd, struct page **pages,
				unsigned long address, u32 num_pages,
				u32 desired_tile_stride, u32 hw_tile_stride,
				int type);
extern void psb_mmu_remove_pages(struct psb_mmu_pd *pd, unsigned long address,
				 u32 num_pages, __u32 desired_tile_stride,
				 u32 hw_tile_stride);
/*
 * psb_sgx.c 
 */

extern int psb_blit_sequence(struct drm_psb_private * dev_priv);
extern void psb_init_2d(struct drm_psb_private * dev_priv);
extern int drm_psb_idle(struct drm_device * dev);
extern int psb_emit_2d_copy_blit(struct drm_device * dev,
				 u32 src_offset,
				 u32 dst_offset, __u32 pages, int direction);
extern int psb_cmdbuf_ioctl(struct drm_device *dev, void *data, 
			    struct drm_file *file_priv);
extern int psb_reg_submit(struct drm_psb_private *dev_priv, 
			  u32 *regs, unsigned int cmds);
extern int psb_submit_copy_cmdbuf(struct drm_device * dev,
			     struct drm_buffer_object * cmd_buffer,
			     unsigned long cmd_offset, 
			     unsigned long cmd_size,
			     int engine,
			     uint32_t *copy_buffer);

extern int psb_fence_for_errors(struct drm_file *priv, 
				struct drm_psb_cmdbuf_arg *arg,
				struct drm_fence_arg *fence_arg,
				struct drm_fence_object **fence_p);

/*
 * psb_irq.c
 */

extern irqreturn_t psb_irq_handler(DRM_IRQ_ARGS);
extern void psb_irq_preinstall(struct drm_device * dev);
extern void psb_irq_postinstall(struct drm_device * dev);
extern void psb_irq_uninstall(struct drm_device * dev);
extern int psb_vblank_wait2(struct drm_device *dev, unsigned int *sequence);
extern int psb_vblank_wait(struct drm_device *dev, unsigned int *sequence);

/*
 * psb_fence.c
 */

extern void psb_poke_flush(struct drm_device * dev, uint32_t class);
extern int psb_fence_emit_sequence(struct drm_device * dev, uint32_t class,
				   uint32_t flags, uint32_t * sequence,
				   uint32_t * native_type);
extern void psb_fence_handler(struct drm_device * dev, uint32_t class);
extern int psb_fence_has_irq(struct drm_device * dev, uint32_t class,
			     uint32_t flags);
extern void psb_2D_irq_off(struct drm_psb_private * dev_priv);
extern void psb_2D_irq_on(struct drm_psb_private * dev_priv);
extern uint32_t psb_fence_advance_sequence(struct drm_device *dev,
				       uint32_t class);
extern void psb_fence_error(struct drm_device *dev,
			    uint32_t class,
			    uint32_t sequence,
			    uint32_t type,
			    int error);

/*MSVDX stuff*/
extern void psb_msvdx_irq_off(struct drm_psb_private * dev_priv);
extern void psb_msvdx_irq_on(struct drm_psb_private * dev_priv);

/*
 * psb_buffer.c
 */
extern drm_ttm_backend_t *drm_psb_tbe_init(struct drm_device * dev);
extern int psb_fence_types(struct drm_buffer_object * bo, uint32_t * class,
			   uint32_t * type);
extern uint32_t psb_evict_mask(struct drm_buffer_object * bo);
extern int psb_invalidate_caches(struct drm_device * dev, uint64_t flags);
extern int psb_init_mem_type(struct drm_device * dev, uint32_t type,
			     struct drm_mem_type_manager * man);
extern int psb_move(struct drm_buffer_object * bo,
		    int evict, int no_wait, struct drm_bo_mem_reg * new_mem);

/*
 * psb_gtt.c 
 */
extern int psb_gtt_init(struct psb_gtt *pg, int resume);
extern int psb_gtt_insert_pages(struct psb_gtt *pg, struct page **pages,
				unsigned offset_pages, unsigned num_pages,
				unsigned desired_tile_stride, 
				unsigned hw_tile_stride, 
				int type);
extern int psb_gtt_remove_pages(struct psb_gtt *pg, unsigned offset_pages, 
				unsigned num_pages, unsigned desired_tile_stride, 
				unsigned hw_tile_stride);

extern struct psb_gtt *psb_gtt_alloc(struct drm_device *dev);
extern void psb_gtt_takedown(struct psb_gtt *pg, int free);

/*
 * psb_fb.c
 */
extern int psbfb_probe(struct drm_device *dev, struct drm_crtc *crtc);
extern int psbfb_remove(struct drm_device *dev, struct drm_crtc *crtc);

/*
 * psb_reset.c
 */

extern void psb_reset(struct drm_psb_private * dev_priv, int reset_2d);
extern void psb_schedule_watchdog(struct drm_psb_private *dev_priv);
extern void psb_watchdog_init(struct drm_psb_private *dev_priv);
extern void psb_watchdog_takedown(struct drm_psb_private *dev_priv);

/*
 * psb_regman.c
 */

extern void psb_takedown_use_base(struct drm_psb_private *dev_priv);
extern int psb_grab_use_base(struct drm_psb_private *dev_priv,
			     unsigned long dev_virtual,
			     unsigned long size,
			     unsigned int data_master,
			     uint32_t fence_class,
			     uint32_t fence_type,
			     int no_wait,
			     int ignore_signals,
			     int *r_reg,
			     u32 *r_offset);
extern int psb_init_use_base(struct drm_psb_private *dev_priv, 
			     unsigned int reg_start,
			     unsigned int reg_num);

/*
 * psb_xhw.c
 */

extern int psb_xhw_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
extern int psb_xhw_init_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
extern int psb_xhw_init(struct drm_device *dev);
extern void psb_xhw_takedown(struct drm_psb_private *dev_priv);
extern void psb_xhw_init_takedown(struct drm_psb_private *dev_priv,
				  struct drm_file *file_priv,
				  int closing);
extern int psb_xhw_scene_bind_fire(struct drm_psb_private *dev_priv,
				   struct psb_xhw_buf *buf,
				   uint32_t fire_flags,
				   uint32_t hw_context,
				   uint32_t *cookie,
				   uint32_t offset,
				   uint32_t engine,
				   uint32_t flags);
extern int psb_xhw_fire_raster(struct drm_psb_private *dev_priv,
			       struct psb_xhw_buf *buf,
			       uint32_t fire_flags);
extern int psb_xhw_scene_info(struct drm_psb_private *dev_priv,
			      struct psb_xhw_buf *buf,
			      uint32_t w, 
			      uint32_t h,
			      uint32_t *hw_cookie,
			      uint32_t *bo_size,
			      uint32_t *clear_p_start,
			      uint32_t *clear_num_pages);

extern int psb_xhw_reset_dpm(struct drm_psb_private *dev_priv,
			     struct psb_xhw_buf *buf);
extern int psb_xhw_bin_mem_info(struct drm_psb_private *dev_priv,
				struct psb_xhw_buf *buf,
				uint32_t pages, 
				uint32_t *hw_cookie,
				uint32_t *size);
extern int psb_xhw_ta_oom(struct drm_psb_private *dev_priv,
			      struct psb_xhw_buf *buf,
			      uint32_t *cookie);
extern void psb_xhw_ta_oom_reply(struct drm_psb_private *dev_priv,
				     struct psb_xhw_buf *buf,
				     uint32_t *cookie,
				     uint32_t *bca,
				     uint32_t *rca,
				     uint32_t *flags);





/*
 * Utilities
 */

#define PSB_ALIGN_TO(_val, _align) \
  (((_val) + ((_align) - 1)) & ~((_align) - 1))
#define PSB_WVDC32(_val, _offs) \
  iowrite32(_val, dev_priv->vdc_reg + (_offs))
#define PSB_RVDC32(_offs) \
  ioread32(dev_priv->vdc_reg + (_offs))
#define PSB_WSGX32(_val, _offs) \
  iowrite32(_val, dev_priv->sgx_reg + (_offs))
#define PSB_RSGX32(_offs) \
  ioread32(dev_priv->sgx_reg + (_offs))
#define PSB_WMSVDX32(_val, _offs) \
  iowrite32(_val, dev_priv->msvdx_reg + (_offs))
#define PSB_RMSVDX32(_offs) \
  ioread32(dev_priv->msvdx_reg + (_offs))

#define PSB_ALPL(_val, _base)			\
  (((_val) >> (_base ## _ALIGNSHIFT)) << (_base ## _SHIFT))
#define PSB_ALPLM(_val, _base)			\
  ((((_val) >> (_base ## _ALIGNSHIFT)) << (_base ## _SHIFT)) & (_base ## _MASK))



static inline psb_fixed psb_mul_fixed(psb_fixed a, psb_fixed b)
{
	s64 tmp;
	s64 a64 = (s64) a;
	s64 b64 = (s64) b;

	tmp = a64*b64;
	return tmp / (1ULL << PSB_FIXED_SHIFT) + 
		((tmp & 0x80000000ULL) ? 1 : 0);
}

static inline psb_fixed psb_mul_ufixed(psb_ufixed a, psb_fixed b)
{
	u64 tmp;
	u64 a64 = (u64) a;
	u64 b64 = (u64) b;

	tmp = a64*b64;
	return (tmp >> PSB_FIXED_SHIFT) + 
		((tmp & 0x80000000ULL) ? 1 : 0);
}

static inline u32 psb_ufixed_to_float32(psb_ufixed a)
{
	u32 exp = 0x7f + 7;
	u32 mantissa = (u32) a;
	
	if (a == 0)
		return 0;
	while((mantissa & 0xff800000) == 0) {
		exp -= 1;
		mantissa <<= 1;
	}
	while((mantissa & 0xff800000) > 0x00800000) {
		exp += 1;
		mantissa >>= 1;
	}
	return (mantissa & ~0xff800000) | (exp << 23);
}

static inline u32 psb_fixed_to_float32(psb_fixed a)
{
	if (a < 0) 
		return psb_ufixed_to_float32(-a) | 0x80000000;
	else
		return psb_ufixed_to_float32(a);
}

#define PSB_D_RENDER  (1 << 16)

#define PSB_D_GENERAL (1 << 0)
#define PSB_D_INIT    (1 << 1)
#define PSB_D_IRQ     (1 << 2)
#define PSB_D_FW      (1 << 3)
#define PSB_D_PERF    (1 << 4)
#define PSB_D_TMP    (1 << 5)  

extern int drm_psb_debug;
extern int drm_psb_no_fb;

#define PSB_DEBUG_FW(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_FW, _fmt, ##_arg)
#define PSB_DEBUG_GENERAL(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_GENERAL, _fmt, ##_arg)
#define PSB_DEBUG_INIT(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_INIT, _fmt, ##_arg)
#define PSB_DEBUG_IRQ(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_IRQ, _fmt, ##_arg)
#define PSB_DEBUG_RENDER(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_RENDER, _fmt, ##_arg)
#define PSB_DEBUG_PERF(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_PERF, _fmt, ##_arg)
#define PSB_DEBUG_TMP(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_TMP, _fmt, ##_arg)

#if DRM_DEBUG_CODE
#define PSB_DEBUG(_flag, _fmt, _arg...)					\
	do {								\
		if ((_flag) & drm_psb_debug)				\
			printk(KERN_DEBUG				\
			       "[psb:0x%02x:%s] " _fmt , _flag,	\
			       __FUNCTION__ , ##_arg);			\
	} while (0)
#else 
#define PSB_DEBUG(_fmt, _arg...)     do { } while (0)
#endif

#endif

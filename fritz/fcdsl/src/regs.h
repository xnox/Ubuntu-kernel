/* 
 * regs.h
 * Copyright (C) 2002, AVM GmbH. All rights reserved.
 * 
 * This Software is  free software. You can redistribute and/or
 * modify such free software under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * The free software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this Software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA, or see
 * http://www.opensource.org/licenses/lgpl-license.html
 * 
 * Contact: AVM GmbH, Alt-Moabit 95, 10559 Berlin, Germany, email: info@avm.de
 */

#ifndef __have_regs_h__
#define __have_regs_h__

/* Pasted from "TriMedia Data Book - MMIO Register Summary, B-1ff" */
/* */
/* Section 1 - General stuff */
/* */
#define DRAM_BASE		0x100000
#define DRAM_LIMIT		0x100004
#define MMIO_BASE		0x100400
#define EXCVEC		0x100800
#define ISETTING0		0x100810
#define ISETTING1		0x100814
#define ISETTING2		0x100818
#define ISETTING3		0x10081c
#define IPENDING		0x100820
#define ICLEAR		0x100824
#define IMASK		0x100828
#define INTVEC0		0x100880
#define INTVEC1		0x100884
#define INTVEC2		0x100888
#define INTVEC3		0x10088c
#define INTVEC4		0x100890
#define INTVEC5		0x100894
#define INTVEC6		0x100898
#define INTVEC7		0x10089c
#define INTVEC8		0x1008a0
#define INTVEC9		0x1008a4
#define INTVEC10		0x1008a8
#define INTVEC11		0x1008ac
#define INTVEC12		0x1008b0
#define INTVEC13		0x1008b4
#define INTVEC14		0x1008b8
#define INTVEC15		0x1008bc
#define INTVEC16		0x1008c0
#define INTVEC17		0x1008c4
#define INTVEC18		0x1008c8
#define INTVEC19		0x1008cc
#define INTVEC20		0x1008d0
#define INTVEC21		0x1008d4
#define INTVEC22		0x1008d8
#define INTVEC23		0x1008dc
#define INTVEC24		0x1008e0
#define INTVEC25		0x1008e4
#define INTVEC26		0x1008e8
#define INTVEC27		0x1008ec
#define INTVEC28		0x1008f0
#define INTVEC29		0x1008f4
#define INTVEC30		0x1008f8
#define INTVEC31		0x1008fc
#define TIMER1_TMODULUS		0x100c00
#define TIMER1_TVALUE		0x100c04
#define TIMER1_TCTL		0x100c08
#define TIMER2_TMODULUS		0x100c20
#define TIMER2_TVALUE		0x100c24
#define TIMER2_TCTL		0x100c28
#define TIMER3_TMODULUS		0x100c40
#define TIMER3_TVALUE		0x100c44
#define TIMER3_TCTL		0x100c48
#define SYSTIMER_TMODULUS		0x100c60
#define SYSTIMER_TVALUE		0x100c64
#define SYSTIMER_TCTL		0x100c68
#define BICTL		0x101000
#define BINSTLOW		0x101004
#define BINSTHIGH		0x101008
#define BDCTL		0x101020
#define BDATAALOW		0x101030
#define BDATAAHIGH		0x101034
#define BDATAVAL		0x101038
#define BDATAMASK		0x10103c
/* */
/* Section 2 - Cache And Memory System */
/* */
#define DRAM_CACHEABLE_LIMIT		0x100008
#define MEM_EVENTS		0x10000c
#define DC_LOCK_CTL		0x100010
#define DC_LOCK_ADDR		0x100014
#define DC_LOCK_SIZE		0x100018
#define DC_PARAMS		0x10001c
#define IC_PARAMS		0x100020
#define MM_CONFIG		0x100100
#define ARB_BW_CTL		0x100104
#define ARB_RAISE		0x10010C
#define POWER_DOWN		0x100108
#define IC_LOCK_CTL		0x100210
#define IC_LOCK_ADDR		0x100214
#define IC_LOCK_SIZE		0x100218
#define PLL_RATIOS		0x100300
#define BLOCK_POWER_DOWN		0x103428
/* */
/* Section 3 - Video In */
/* */
#define VI_STATUS		0x101400
#define VI_CTL		0x101404
#define VI_CLOCK		0x101408
#define VI_CAP_START		0x10140c
#define VI_CAP_SIZE		0x101410
#define VI_BASE1		0x101414
#define VI_Y_BASE_ADR		0x101414
#define VI_BASE2		0x101418
#define VI_U_BASE_ADR		0x101418
#define VI_SIZE		0x10141c
#define VI_V_BASE_ADR		0x10141c
#define VI_UV_DELTA		0x101420
#define VI_Y_DELTA		0x101424
/* */
/* Section 4 - Video Out */
/* */
#define VO_STATUS		0x101800
#define VO_CTL		0x101804
#define VO_CLOCK		0x101808
#define VO_FRAME		0x10180c
#define VO_FIELD		0x101810
#define VO_LINE		0x101814
#define VO_IMAGE		0x101818
#define VO_YTHR		0x10181c
#define VO_OLSTART		0x101820
#define VO_OLHW		0x101824
#define VO_YADD		0x101828
#define VO_UADD		0x10182c
#define VO_VADD		0x101830
#define VO_OLADD		0x101834
#define VO_VUF		0x101838
#define VO_YOLF		0x10183c
#define EVO_CTL		0x101840
#define EVO_MASK		0x101844
#define EVO_CLIP		0x101848
#define EVO_KEY		0x10184c
#define EVO_SLVDLY		0x101850
/* */
/* Section 5 - AUdio In */
/* */
#define AI_CTL		0x101c04
#define AI_SERIAL		0x101c08
#define AI_FRAMING		0x101c0c
#define AI_FREQ		0x101c10
#define AI_BASE1		0x101c14
#define AI_BASE2		0x101c18
#define AI_SIZE		0x101c1c
/* */
/* Section 6 - Audio Out */
/* */
#define AO_STATUS		0x102000
#define AO_CTL		0x102004
#define AO_SERIAL		0x102008
#define AO_FRAMING		0x10200c
#define AO_FREQ		0x102010
#define AO_BASE1		0x102014
#define AO_BASE2		0x102018
#define AO_SIZE		0x10201c
#define AO_CC		0x102020
#define AO_CFC		0x102024
#define AO_TSTAMP		0x102028
/* */
/* Section 7 - SPDIF Out */
/* */
#define SDO_STATUS		0x104C00
#define SDO_CTL		0x104C04
#define SDO_FREQ		0x104C08
#define SDO_BASE1		0x104C0C
#define SDO_BASE2		0x104C10
#define SDO_SIZE		0x104C14
#define SDO_TSTAMP		0x104C18
/* */
/* Section 8 - PCI Interface */
/* */
#define BIU_STATUS		0x103004
#define BIU_CTL		0x103008
#define PCI_ADR		0x10300c
#define PCI_DATA		0x103010
#define CONFIG_ADR		0x103014
#define CONFIG_DATA		0x103018
#define CONFIG_CTL		0x10301c
#define IO_ADR		0x103020
#define IO_DATA		0x103024
#define IO_CTL		0x103028
#define SRC_ADR		0x10302c
#define DEST_ADR		0x103030
#define DMA_CTL		0x103034
#define INT_CTL		0x103038
#define XIO_CTL		0x103060
/* */
/* Section 9 - JTAG */
/* */
#define JTAG_DATA_IN		0x103800
#define JTAG_DATA_OUT		0x103804
#define JTAG_CTL		0x103808
/* */
/* Section 10 - Image CoPro */
/* */
#define ICP_MPC		0x102400
#define ICP_MIR		0x102404
#define ICP_DP		0x102408
#define ICP_DR		0x102410
#define ICP_SR		0x102414
/* */
/* Section 11 - VLD CoPro */
/* */
#define VLD_COMMAND		0x102800
#define VLD_SR		0x102804
#define VLD_QS		0x102808
#define VLD_PI		0x10280C
#define VLD_STATUS		0x102810
#define VLD_IMASK		0x102814
#define VLD_CTL		0x102818
#define VLD_BIT_ADR		0x10281C
#define VLD_BIT_CNT		0x102820
#define VLD_MBH_ADR		0x102824
#define VLD_MBH_CNT		0x102828
#define VLD_RL_ADR		0x10282C
#define VLD_RL_CNT		0x102830
/* */
/* Section 12 - I2C interface */
/* */
#define IIC_AR		0x103400
#define IIC_DR		0x103404
#define IIC_STATUS		0x103408
#define IIC_CTL		0x10340C
/* */
/* Section 13 - Synchronous Serial Interface */
/* */
#define SSI_CTL		0x102C00
#define SSI_CSR		0x102C04
#define SSI_TXDR		0x102C10
#define SSI_RXDR		0x102C20
#define SSI_RXACK		0x102C24
/* */
/* Section 14 - SEM device */
/* */
#define SEM		0x100500

#endif


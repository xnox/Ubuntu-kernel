/*
 * linux/drivers/video/omap2/dss/hdmi.c
 *
 * Copyright (C) 2009 Texas Instruments
 * Author: Yong Zhi
 *
 * HDMI settings from TI's DSS driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 * History:
 * Mythripk <mythripk@ti.com>  Apr 2010 Modified for EDID reading and adding OMAP
 *                                      related timing
 *				May 2010 Added support of Hot Plug Detect
 *				July 2010 Redesigned HDMI EDID for Auto-detect of timing
 *
 */

#define DSS_SUBSYS_NAME "HDMI"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <plat/display.h>
#include <plat/cpu.h>
#include <plat/hdmi_lib.h>
#include <plat/gpio.h>
#include <linux/slab.h>

#include "dss.h"
#include "hdmi.h"

static int hdmi_enable_display(struct omap_dss_device *dssdev);
static void hdmi_disable_display(struct omap_dss_device *dssdev);
static int hdmi_display_suspend(struct omap_dss_device *dssdev);
static int hdmi_display_resume(struct omap_dss_device *dssdev);
static void hdmi_get_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings);
static void hdmi_set_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings);
static void hdmi_set_custom_edid_timing_code(struct omap_dss_device *dssdev, int code , int mode);
static int hdmi_check_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings);
static int hdmi_read_edid(struct omap_video_timings *);
static int get_edid_timing_data(struct HDMI_EDID *edid);
static irqreturn_t hdmi_irq_handler(int irq, void *arg);
static int hdmi_enable_hpd(struct omap_dss_device *dssdev);
static void hdmi_power_off(struct omap_dss_device *dssdev);
static void hdmi_panel_disable(struct omap_dss_device *dssdev);

#define HDMI_PLLCTRL		0x58006200
#define HDMI_PHY		0x58006300

u16 current_descriptor_addrs;
u8		edid[HDMI_EDID_MAX_LENGTH] = {0};
u8		edid_set = false;
u8		header[8] = {0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0};
u8 hpd_mode = 0, custom_set = 0;
/* PLL */
#define PLLCTRL_PLL_CONTROL				0x0ul
#define PLLCTRL_PLL_STATUS				0x4ul
#define PLLCTRL_PLL_GO					0x8ul
#define PLLCTRL_CFG1					0xCul
#define PLLCTRL_CFG2					0x10ul
#define PLLCTRL_CFG3					0x14ul
#define PLLCTRL_CFG4					0x20ul

/* HDMI PHY */
#define HDMI_TXPHY_TX_CTRL				0x0ul
#define HDMI_TXPHY_DIGITAL_CTRL			0x4ul
#define HDMI_TXPHY_POWER_CTRL			0x8ul
#define HDMI_TXPHY_PAD_CFG_CTRL			0xCul

/*This is the structure which has all supported timing values that OMAP4 supports*/
const struct omap_video_timings all_timings_direct[31] = {
						{640, 480, 25200, 96, 16, 48, 2, 10, 33},
						{1280, 720, 74250, 40, 440, 220, 5, 5, 20},
						{1280, 720, 74250, 40, 110, 220, 5, 5, 20},
						{720, 480, 27000, 62, 16, 60, 6, 9, 30},
						{2880, 576, 108000, 256, 48, 272, 5, 5, 39},
						{1440, 240, 27000, 124, 38, 114, 3, 4, 15},
						{1440, 288, 27000, 126, 24, 138, 3, 2, 19},
						{1920, 540, 74250, 44, 528, 148, 5, 2, 15},
						{1920, 540, 74250, 44, 88, 148, 5, 2, 15},
						{1920, 1080, 148500, 44, 88, 148, 5, 4, 36},
						{720, 576, 27000, 64, 12, 68, 5, 5, 39},
						{1440, 576, 54000, 128, 24, 136, 5, 5, 39},
						{1920, 1080, 148500, 44, 528, 148, 5, 4, 36},
						{2880, 480, 108000, 248, 64, 240, 6, 9, 30},
						/*Vesa frome here*/
						{640, 480, 25175, 96, 16, 48, 2 , 11, 31},
						{800, 600, 40000, 128, 40, 88, 4 , 1, 23},
						{848, 480, 33750, 112, 16, 112, 8 , 6, 23},
						{1280, 768, 71000, 128, 64, 192, 7 , 3, 20},
						{1280, 800, 83500, 128, 72, 200, 6 , 3, 22},
						{1360, 768, 85500, 112, 64, 256, 6 , 3, 18},
						{1280, 960, 108000, 112, 96, 312, 3 , 1, 36},
						{1280, 1024, 108000, 112, 48, 248, 3 , 1, 38},
						{1024, 768, 65000, 136, 24, 160, 6, 3, 29},
						{1400, 1050, 121750, 144, 88, 232, 4, 3, 32},
						{1440, 900, 106500, 152, 80, 232, 6, 3, 25},
						{1680, 1050, 146250, 176 , 104, 280, 6, 3, 30},
						{1366, 768, 85500, 143, 70, 213, 3, 3, 24},
						{1920, 1080, 148500, 44, 88, 80, 5, 4, 36},
						{1280, 768, 68250, 32, 48, 80, 7, 3, 12},
						{1400, 1050, 101000, 32, 48, 80, 4, 3, 23},
						{1680, 1050, 119000, 32, 48, 80, 6, 3, 21} };

/*This is a static Mapping array which maps the timing values with corresponding CEA / VESA code*/
int code_index[31] = {1, 19, 4, 2, 37, 6, 21, 20, 5, 16, 17, 29, 31, 35,
			/* <--14 CEA 17--> vesa*/
			4, 9, 0xE, 0x17, 0x1C, 0x27, 0x20, 0x23, 0x10, 0x2A,
			0X2F, 0x3A, 0X51, 0X52, 0x16, 0x29, 0x39};

/*This is revere static mapping which maps the CEA / VESA code to the corresponding timing values*/
/* note: table is 10 entries per line to make it easier to find index.. */
int code_cea[39] = {
		-1,  0,  3,  3,  2,  8,  5,  5, -1, -1,
		-1, -1, -1, -1, -1, -1,  9, 10, 10,  1,
		7,   6,  6, -1, -1, -1, -1, -1, -1, 11,
		11, 12, -1, -1, -1, 13, 13,  4,  4};

/* note: table is 10 entries per line to make it easier to find index.. */
int code_vesa[83] = {
		-1, -1, -1, -1, 14, -1, -1, -1, -1, 15,
		-1, -1, -1, -1, 16, -1, 22, -1, -1, -1,
		-1, -1, 28, 17, -1, -1, -1, -1, 18, -1,
		-1, -1, 20, -1, -1, 21, -1, -1, -1, 19,
		-1, 29, 23, -1, -1, -1, -1, 24, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, 30, 25, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, 26, 27};

static struct {
	void __iomem *base_phy;
	void __iomem *base_pll;
	struct mutex lock;
	int code;
	int mode;
	struct hdmi_config cfg;
	struct omap_display_platform_data *pdata;
	struct platform_device *pdev;
} hdmi;

struct hdmi_cm {
	int code;
	int mode;
};

static void update_cfg (struct hdmi_config *cfg, struct omap_video_timings *timings)
{
	cfg->ppl = timings->x_res;
	cfg->lpp = timings->y_res;
	cfg->hbp = timings->hbp;
	cfg->hfp = timings->hfp;
	cfg->hsw = timings->hsw;
	cfg->vbp = timings->vbp;
	cfg->vfp = timings->vfp;
	cfg->vsw = timings->vsw;
	cfg->pixel_clock = timings->pixel_clock;
	cfg->v_pol = 1;      // XXX get this from EDID
	cfg->h_pol = 1;      // XXX get this from EDID
}

static inline void hdmi_write_reg(u32 base, u16 idx, u32 val)
{
	void __iomem *b;

	switch (base) {
	case HDMI_PHY:
	  b = hdmi.base_phy;
	  break;
	case HDMI_PLLCTRL:
	  b = hdmi.base_pll;
	  break;
	default:
	  BUG();
	}
	__raw_writel(val, b + idx);
	/* DBG("write = 0x%x idx =0x%x\r\n", val, idx); */
}

static inline u32 hdmi_read_reg(u32 base, u16 idx)
{
	void __iomem *b;
	u32 l;

	switch (base) {
	case HDMI_PHY:
	 b = hdmi.base_phy;
	 break;
	case HDMI_PLLCTRL:
	 b = hdmi.base_pll;
	 break;
	default:
	 BUG();
	}
	l = __raw_readl(b + idx);

	/* DBG("addr = 0x%p rd = 0x%x idx = 0x%x\r\n", (b+idx), l, idx); */
	return l;
}

#define FLD_GET(val, start, end) (((val) & FLD_MASK(start, end)) >> (end))
#define FLD_MOD(orig, val, start, end) \
	(((orig) & ~FLD_MASK(start, end)) | FLD_VAL(val, start, end))

#define REG_FLD_MOD(b, i, v, s, e) \
	hdmi_write_reg(b, i, FLD_MOD(hdmi_read_reg(b, i), v, s, e))


/*
 * "edid" sysfs file, allows userspace to read raw EDID, parse with tools
 * such as parse-edid, etc
 */

static ssize_t hdmi_edid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	memcpy(buf, edid, HDMI_EDID_MAX_LENGTH);
	return HDMI_EDID_MAX_LENGTH;
}

static ssize_t hdmi_edid_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	return 0;
}

static DEVICE_ATTR(edid, S_IRUGO, hdmi_edid_show, hdmi_edid_store);

/*
 * "custom_edid_timing" sysfs file, allows userspace to read or override
 * current mode/code.  The value read/written is equal to:
 *
 *    val = (code * 10) + mode
 *
 * (ie. low digit is mode, 1 or 0, and upper digits are code)
 */

static ssize_t hdmi_custom_edid_timing_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", (hdmi.code * 10) + hdmi.mode);
}

static ssize_t hdmi_custom_edid_timing_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	int val, code, mode;
	val = simple_strtoul(buf, NULL, 0);
	if (val) {
		code = val / 10;
		mode = val % 10;
		hdmi_set_custom_edid_timing_code(dssdev, code, mode);
		return size;
	}

	return -EINVAL;
}

static DEVICE_ATTR(custom_edid_timing, S_IRUGO|S_IWUSR,
		hdmi_custom_edid_timing_show, hdmi_custom_edid_timing_store);

/*
 * "hpd_enabled" sysfs file, allows userspace to enable/disable hot plug
 * detect
 */

static ssize_t hdmi_hpd_enabled_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* this is not quite right after the hpd event.. but is the best
	 * that can be done without changing the hot plug detect logic..
	 */
	return sprintf(buf, "%d\n", hpd_mode);
}

static ssize_t hdmi_hpd_enabled_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	bool enabled, r;

	enabled = simple_strtoul(buf, NULL, 10);

	if (enabled != (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)) {
		if (enabled) {
			r = hdmi_enable_hpd(dssdev);
			if (r)
				return r;
		} else {
			hdmi_panel_disable(dssdev);
		}
	}

	return size;
}

static DEVICE_ATTR(hpd_enabled, S_IRUGO|S_IWUSR,
		hdmi_hpd_enabled_show, hdmi_hpd_enabled_store);


/*
 * refclk = (sys_clk/(highfreq+1))/(n+1)
 * so refclk = 38.4/2/(n+1) = 19.2/(n+1)
 * choose n = 15, makes refclk = 1.2
 *
 * m = tclk/cpf*refclk = tclk/2*1.2
 *
 *	for clkin = 38.2/2 = 192
 *	    phy = 2520
 *
 *	m = 2520*16/2* 192 = 105;
 *
 *	for clkin = 38.4
 *	    phy = 2520
 *
 */

#define CPF			2

struct hdmi_pll_info {
	u16 regn;
	u16 regm;
	u32 regmf;
	u16 regm4; /* M4_CLOCK_DIV */
	u16 regm2;
	u16 regsd;
	u16 dcofreq;
};

static inline void print_omap_video_timings(struct omap_video_timings *timings)
{
	extern unsigned int dss_debug;
	if (dss_debug) {
		printk(KERN_DEBUG "Timing Info:\n");
		printk(KERN_DEBUG "  pixel_clk = %d\n", timings->pixel_clock);
		printk(KERN_DEBUG "  x_res     = %d\n", timings->x_res);
		printk(KERN_DEBUG "  y_res     = %d\n", timings->y_res);
		printk(KERN_DEBUG "  hfp       = %d\n", timings->hfp);
		printk(KERN_DEBUG "  hsw       = %d\n", timings->hsw);
		printk(KERN_DEBUG "  hbp       = %d\n", timings->hbp);
		printk(KERN_DEBUG "  vfp       = %d\n", timings->vfp);
		printk(KERN_DEBUG "  vsw       = %d\n", timings->vsw);
		printk(KERN_DEBUG "  vbp       = %d\n", timings->vbp);
	}
}

static void compute_pll(int clkin, int phy,
	int n, struct hdmi_pll_info *pi)
{
	int refclk;
	u32 temp, mf;

	if (clkin > 3200) /* 32 mHz */
		refclk = clkin / (2 * (n + 1));
	else
		refclk = clkin / (n + 1);

	temp = phy * 100/(CPF * refclk);

	pi->regn = n;
	pi->regm = temp/100;
	pi->regm2 = 1;

	mf = (phy - pi->regm * CPF * refclk) * 262144;
	pi->regmf = mf/(CPF * refclk);

	if (phy > 1000 * 100) {
		pi->regm4 = phy / 10000;
		pi->dcofreq = 1;
		pi->regsd = ((pi->regm * 384)/((n + 1) * 250) + 5)/10;
	} else {
		pi->regm4 = 1;
		pi->dcofreq = 0;
		pi->regsd = 0;
	}

	DSSDBG("M = %d Mf = %d, m4= %d\n", pi->regm, pi->regmf, pi->regm4);
	DSSDBG("range = %d sd = %d\n", pi->dcofreq, pi->regsd);
}

static int hdmi_pll_init(int refsel, int dcofreq, struct hdmi_pll_info *fmt, u16 sd)
{
	u32 r;
	unsigned t = 500000;
	u32 pll = HDMI_PLLCTRL;

	/* PLL start always use manual mode */
	REG_FLD_MOD(pll, PLLCTRL_PLL_CONTROL, 0x0, 0, 0);

	r = hdmi_read_reg(pll, PLLCTRL_CFG1);
	r = FLD_MOD(r, fmt->regm, 20, 9); /* CFG1__PLL_REGM */
	r = FLD_MOD(r, fmt->regn, 8, 1);  /* CFG1__PLL_REGN */
	r = FLD_MOD(r, fmt->regm4, 25, 21); /* M4_CLOCK_DIV */

	hdmi_write_reg(pll, PLLCTRL_CFG1, r);

	r = hdmi_read_reg(pll, PLLCTRL_CFG2);

	/* SYS w/o divide by 2 [22:21] = donot care  [11:11] = 0x0 */
	/* SYS divide by 2     [22:21] = 0x3 [11:11] = 0x1 */
	/* PCLK, REF1 or REF2  [22:21] = 0x0, 0x 1 or 0x2 [11:11] = 0x1 */
	r = FLD_MOD(r, 0x0, 11, 11); /* PLL_CLKSEL 1: PLL 0: SYS*/
	r = FLD_MOD(r, 0x0, 12, 12); /* PLL_HIGHFREQ divide by 2 */
	r = FLD_MOD(r, 0x1, 13, 13); /* PLL_REFEN */
	r = FLD_MOD(r, 0x0, 14, 14); /* PHY_CLKINEN de-assert during locking */
	r = FLD_MOD(r, 0x1, 20, 20); /* HSDIVBYPASS assert during locking */
	r = FLD_MOD(r, refsel, 22, 21); /* REFSEL */
	/* DPLL3  used by DISPC or HDMI itself*/
	r = FLD_MOD(r, 0x0, 17, 17); /* M4_CLOCK_PWDN */
	r = FLD_MOD(r, 0x1, 16, 16); /* M4_CLOCK_EN */

	if (dcofreq) {
		/* divider programming for 1080p */
		REG_FLD_MOD(pll, PLLCTRL_CFG3, sd, 17, 10);
		r = FLD_MOD(r, 0x4, 3, 1); /* 1000MHz and 2000MHz */
	} else
		r = FLD_MOD(r, 0x2, 3, 1); /* 500MHz and 1000MHz */

	hdmi_write_reg(pll, PLLCTRL_CFG2, r);

	r = hdmi_read_reg(pll, PLLCTRL_CFG4);
	r = FLD_MOD(r, 0, 24, 18); /* todo: M2 */
	r = FLD_MOD(r, fmt->regmf, 17, 0);

	/* go now */
	REG_FLD_MOD(pll, PLLCTRL_PLL_GO, 0x1ul, 0, 0);

	/* wait for bit change */
	while (FLD_GET(hdmi_read_reg(pll, PLLCTRL_PLL_GO), 0, 0))

	/* Wait till the lock bit is set */
	/* read PLL status */
	while (0 == FLD_GET(hdmi_read_reg(pll, PLLCTRL_PLL_STATUS), 1, 1)) {
		udelay(1);
		if (!--t) {
			printk(KERN_WARNING "HDMI: cannot lock PLL\n");
			DSSDBG("CFG1 0x%x\n", hdmi_read_reg(pll, PLLCTRL_CFG1));
			DSSDBG("CFG2 0x%x\n", hdmi_read_reg(pll, PLLCTRL_CFG2));
			DSSDBG("CFG4 0x%x\n", hdmi_read_reg(pll, PLLCTRL_CFG4));
			return -EIO;
		}
	}

	DSSDBG("PLL locked!\n");

	r = hdmi_read_reg(pll, PLLCTRL_CFG2);
	r = FLD_MOD(r, 0, 0, 0);	/* PLL_IDLE */
	r = FLD_MOD(r, 0, 5, 5);	/* PLL_PLLLPMODE */
	r = FLD_MOD(r, 0, 6, 6);	/* PLL_LOWCURRSTBY */
	r = FLD_MOD(r, 0, 8, 8);	/* PLL_DRIFTGUARDEN */
	r = FLD_MOD(r, 0, 10, 9);	/* PLL_LOCKSEL */
	r = FLD_MOD(r, 1, 13, 13);	/* PLL_REFEN */
	r = FLD_MOD(r, 1, 14, 14);	/* PHY_CLKINEN */
	r = FLD_MOD(r, 0, 15, 15);	/* BYPASSEN */
	r = FLD_MOD(r, 0, 20, 20);	/* HSDIVBYPASS */
	hdmi_write_reg(pll, PLLCTRL_CFG2, r);

	return 0;
}

static int hdmi_pll_reset(void)
{
	int t = 0;

	/* SYSREEST  controled by power FSM*/
	REG_FLD_MOD(HDMI_PLLCTRL, PLLCTRL_PLL_CONTROL, 0x0, 3, 3);

	/* READ 0x0 reset is in progress */
	while (!FLD_GET(hdmi_read_reg(HDMI_PLLCTRL,
			PLLCTRL_PLL_STATUS), 0, 0)) {
		udelay(1);
		if (t++ > 1000) {
			ERR("Failed to sysrest PLL\n");
			return -ENODEV;
		}
	}
	return 0;
}

int hdmi_pll_program(struct hdmi_pll_info *fmt)
{
	u32 r;
	int refsel;

	HDMI_PllPwr_t PllPwrWaitParam;

	/* wait for wrapper rest */
	HDMI_W1_SetWaitSoftReset();

	/* power off PLL */
	PllPwrWaitParam = HDMI_PLLPWRCMD_ALLOFF;
	r = HDMI_W1_SetWaitPllPwrState(HDMI_WP,
				PllPwrWaitParam);
	if (r)
		return r;

	/* power on PLL */
	PllPwrWaitParam = HDMI_PLLPWRCMD_BOTHON_ALLCLKS;
	r = HDMI_W1_SetWaitPllPwrState(HDMI_WP,
				PllPwrWaitParam);
	if (r)
		return r;

	hdmi_pll_reset();

	refsel = 0x3; /* select SYSCLK reference */

	r = hdmi_pll_init(refsel, fmt->dcofreq, fmt, fmt->regsd);

	return r;
}

/* double check the order */
static int hdmi_phy_init(u32 w1,
		u32 phy)
{
	u32 count;
	int r;

	/* wait till PHY_PWR_STATUS=LDOON */
	/* HDMI_PHYPWRCMD_LDOON = 1 */
	r = HDMI_W1_SetWaitPhyPwrState(w1, 1);
	if (r)
		return r;

	/* wait till PHY_PWR_STATUS=TXON */
	r = HDMI_W1_SetWaitPhyPwrState(w1, 2);
	if (r)
		return r;

	/* read address 0 in order to get the SCPreset done completed */
	/* Dummy access performed to solve resetdone issue */
	hdmi_read_reg(phy, HDMI_TXPHY_TX_CTRL);

	/* write to phy address 0 to configure the clock */
	/* use HFBITCLK write HDMI_TXPHY_TX_CONTROL__FREQOUT field */
	REG_FLD_MOD(phy, HDMI_TXPHY_TX_CTRL, 0x1, 31, 30);

	/* write to phy address 1 to start HDMI line (TXVALID and TMDSCLKEN) */
	hdmi_write_reg(phy, HDMI_TXPHY_DIGITAL_CTRL,
				0xF0000000);

	/* setup max LDO voltage */
	REG_FLD_MOD(phy, HDMI_TXPHY_POWER_CTRL, 0xB, 3, 0);
	/*  write to phy address 3 to change the polarity control  */
	REG_FLD_MOD(phy, HDMI_TXPHY_PAD_CFG_CTRL, 0x1, 27, 27);

	count = 0;
	while (count++ < 1000)
		;
	return 0;
}

static int hdmi_phy_off(u32 name)
{
	int r = 0;
	u32 count;

	/* wait till PHY_PWR_STATUS=OFF */
	/* HDMI_PHYPWRCMD_OFF = 0 */
	r = HDMI_W1_SetWaitPhyPwrState(name, 0);
	if (r)
		return r;

	count = 0;
	while (count++ < 200)
		;
	return 0;
}

/* driver */
static int get_timings_index(void)
{
	int code;

	if (hdmi.mode == 0)
		code = code_vesa[hdmi.code];
	else
		code = code_cea[hdmi.code];

	if (code == -1)	{
		code = 9;
		hdmi.code = 16;
		hdmi.mode = 1;
	}
	return code;
}

/* driver */
static int hdmi_panel_probe(struct omap_dss_device *dssdev)
{
	int code;
	DSSDBG("ENTER hdmi_panel_probe()\n");

	dssdev->panel.config = OMAP_DSS_LCD_TFT |
			OMAP_DSS_LCD_IVS | OMAP_DSS_LCD_IHS;

	code = get_timings_index();

	dssdev->panel.timings = all_timings_direct[code];
	DSSDBG("hdmi_panel_probe x_res= %d y_res = %d", \
		dssdev->panel.timings.x_res, dssdev->panel.timings.y_res);

	mdelay(50);

	return 0;
}

static void hdmi_panel_remove(struct omap_dss_device *dssdev)
{

}

static int hdmi_panel_enable(struct omap_dss_device *dssdev)
{
	hdmi_enable_display(dssdev);
	return 0;
}

static void hdmi_panel_disable(struct omap_dss_device *dssdev)
{
	hdmi_disable_display(dssdev);
}

static int hdmi_panel_suspend(struct omap_dss_device *dssdev)
{
	hdmi_display_suspend(dssdev);
	return 0;
}

static int hdmi_panel_resume(struct omap_dss_device *dssdev)
{
	hdmi_display_resume(dssdev);
	return 0;
}

static void hdmi_enable_clocks(int enable)
{
	if (enable)
		dss_clk_enable();
	else
		dss_clk_disable();
}

static struct omap_dss_driver hdmi_driver = {
	.probe		= hdmi_panel_probe,
	.remove		= hdmi_panel_remove,

	.enable		= hdmi_panel_enable,
	.disable	= hdmi_panel_disable,
	.suspend	= hdmi_panel_suspend,
	.resume		= hdmi_panel_resume,
	.get_timings	= hdmi_get_timings,
	.set_timings	= hdmi_set_timings,
	.check_timings	= hdmi_check_timings,
	.driver			= {
		.name   = "hdmi_panel",
		.owner  = THIS_MODULE,
	},
};
/* driver end */

int hdmi_init(struct platform_device *pdev)
{
	int r = 0, hdmi_irq;
	struct resource *hdmi_mem;

	DSSDBG("Enter hdmi_init()\n");

	hdmi.pdata = pdev->dev.platform_data;
	hdmi.pdev = pdev;
	mutex_init(&hdmi.lock);

	hdmi_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	hdmi.base_pll = ioremap((hdmi_mem->start + 0x200), resource_size(hdmi_mem));
	if (!hdmi.base_pll) {
		ERR("can't ioremap pll\n");
		return -ENOMEM;
	}
	hdmi.base_phy = ioremap((hdmi_mem->start + 0x300), 64);

	if (!hdmi.base_phy) {
		ERR("can't ioremap phy\n");
		return -ENOMEM;
	}

	hdmi_enable_clocks(1);

	hdmi_lib_init();

	hdmi_enable_clocks(0);
	hdmi_irq = platform_get_irq(pdev, 0);
	r = request_irq(hdmi_irq,
				hdmi_irq_handler,
			0, "OMAP HDMI", (void *)0);


	return omap_dss_register_driver(&hdmi_driver);

}

void hdmi_exit(void)
{
	hdmi_lib_exit();
	free_irq(OMAP44XX_IRQ_DSS_HDMI, NULL);
	iounmap(hdmi.base_pll);
	iounmap(hdmi.base_phy);
}

static int hdmi_power_on(struct omap_dss_device *dssdev)
{
	int r = 0;
	int code = 0;
	int dirty = false;
	struct omap_video_timings *p;
	struct hdmi_pll_info pll_data;

	int clkin, n, phy;

	hdmi_enable_clocks(1);

	p = &dssdev->panel.timings;

	if (!custom_set) {
		code = get_timings_index();
		DSSDBG("No edid set thus will be calling hdmi_read_edid");
		r = hdmi_read_edid(p);
		if (r) {
			r = -EIO;
			goto err;
		}

		if (get_timings_index() != code) {
			dirty = true;
		}
	} else {
		dirty = true;
	}

	update_cfg(&hdmi.cfg, p);

	code = get_timings_index();
	dssdev->panel.timings = all_timings_direct[code];

	DSSDBG("hdmi_power on x_res= %d y_res = %d", \
		dssdev->panel.timings.x_res, dssdev->panel.timings.y_res);
	DSSDBG("hdmi_power on code= %d mode = %d", hdmi.code,
		 hdmi.mode);
	DSSDBG("hdmi_power on edid read P x_res= %d y_res = %d", p->x_res,
		 p->y_res);

	clkin = 3840; /* 38.4 mHz */
	n = 15; /* this is a constant for our math */
	phy = p->pixel_clock;
	compute_pll(clkin, phy, n, &pll_data);

	HDMI_W1_StopVideoFrame(HDMI_WP);

	dispc_enable_digit_out(0);

	if (dirty) {
		omap_dss_notify(dssdev, OMAP_DSS_SIZE_CHANGE);
	}

	/* config the PLL and PHY first */
	r = hdmi_pll_program(&pll_data);
	if (r) {
		DSSERR("Failed to lock PLL\n");
		r = -EIO;
		goto err;
	}

	r = hdmi_phy_init(HDMI_WP, HDMI_PHY);
	if (r) {
		DSSERR("Failed to start PHY\n");
		r = -EIO;
		goto err;
	}

	hdmi.cfg.hdmi_dvi = hdmi.mode;
	hdmi.cfg.video_format = hdmi.code;

	if ((hdmi.mode)) {
		switch (hdmi.code) {
		case 20:
		case 5:
		case 6:
		case 21:
			hdmi.cfg.interlace = 1;
			break;
		default:
			hdmi.cfg.interlace = 0;
			break;
		}
	}

	hdmi_lib_enable(&hdmi.cfg);

	/* these settings are independent of overlays */
	dss_switch_tv_hdmi(1);

	/* bypass TV gamma table*/
	dispc_enable_gamma_table(0);

	/* do not fall into any sort of idle */
	dispc_set_idle_mode();

	/* tv size */
	dispc_set_digit_size(dssdev->panel.timings.x_res,
			dssdev->panel.timings.y_res);

	HDMI_W1_StartVideoFrame(HDMI_WP);

	dispc_enable_digit_out(1);

	return 0;
err:
	return r;
}

int hdmi_min_enable(void)
{
	int r;
	DSSDBG("hdmi_min_enable");
	r = hdmi_phy_init(HDMI_WP, HDMI_PHY);
	if (r) {
		DSSERR("Failed to start PHY\n");
	}
	hdmi.cfg.hdmi_dvi = hdmi.mode;
	hdmi.cfg.video_format = hdmi.code;
	hdmi_lib_enable(&hdmi.cfg);
	return 0;
}

void hdmi_work_queue(struct hdmi_work_struct *work)
{
	struct omap_dss_device *dssdev = NULL;
	const char *buf = "hdmi";
	int r = ((struct hdmi_work_struct *)work)->r;

	int match(struct omap_dss_device *dssdev2 , void *data)
	{
		const char *str = data;
		return sysfs_streq(dssdev2->name , str);
	}
	dssdev = omap_dss_find_device((void *)buf , match);
	DSSDBG("found hdmi handle %s" , dssdev->name);

	if ((r == 4 || r == 2) && (hpd_mode == 1) &&
		(dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)) {
		hdmi_phy_off(HDMI_WP);
		hdmi_enable_clocks(1);
		hdmi_power_on(dssdev);
		mdelay(1000);
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
		DSSDBG(KERN_INFO "Display enabled");
	}
	if (r == 1 || r == 4)
		hpd_mode = 0;

	if ((r == 3) && (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)) {
		DSSDBG(KERN_INFO "Display disabled");
		hdmi_power_off(dssdev);
		hpd_mode = 1;
		/* PAD0_HDMI_HPD_PAD1_HDMI_CEC */
		omap_writel(0x01180118, 0x4A100098);
		/* PAD0_HDMI_DDC_SCL_PAD1_HDMI_DDC_SDA */
		omap_writel(0x01180118 , 0x4A10009C);
		/* CONTROL_HDMI_TX_PHY */
		omap_writel(0x10000000, 0x4A100610);

		if (dssdev->platform_enable)
			dssdev->platform_enable(dssdev);

		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
		hdmi_min_enable();
	}

	kfree(work);
}

static irqreturn_t hdmi_irq_handler(int irq, void *arg)
{
	struct work_struct *work;
	int r = 0;

	HDMI_W1_HPD_handler(&r);
	printk("r = %d", r);

	if ((r == 4 || r == 2) && (hpd_mode == 1)) {
		hdmi_phy_off(HDMI_WP);
		hdmi_enable_clocks(1);
	}

	work = kmalloc(sizeof(struct hdmi_work_struct), GFP_KERNEL);

	if (work) {
		printk("r = %d", r);
		INIT_WORK(work, hdmi_work_queue);
		((struct hdmi_work_struct *)work)->r = r;
		schedule_work(work);
	} else {
		printk(KERN_ERR "Cannot allocate memory to create work");
	}

	return IRQ_HANDLED;
}


static void hdmi_power_off(struct omap_dss_device *dssdev)
{
	HDMI_W1_StopVideoFrame(HDMI_WP);

	dispc_enable_digit_out(0);

	hdmi_phy_off(HDMI_WP);

	HDMI_W1_SetWaitPllPwrState(HDMI_WP, HDMI_PLLPWRCMD_ALLOFF);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	edid_set = false;
	hdmi_enable_clocks(0);

	/* reset to default */

}

static int hdmi_enable_display(struct omap_dss_device *dssdev)
{
	int r = 0;
	DSSDBG("ENTER hdmi_enable_display()\n");

	mutex_lock(&hdmi.lock);

	/* the tv overlay manager is shared*/
	r = omap_dss_start_device(dssdev);
	if (r) {
		DSSERR("failed to start device\n");
		goto err;
	}

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED) {
		r = -EINVAL;
		goto err;
	}

	free_irq(OMAP44XX_IRQ_DSS_HDMI, NULL);

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	/* PAD0_HDMI_HPD_PAD1_HDMI_CEC */
	omap_writel(0x01180118, 0x4A100098);
	/* PAD0_HDMI_DDC_SCL_PAD1_HDMI_DDC_SDA */
	omap_writel(0x01180118 , 0x4A10009C);
	/* CONTROL_HDMI_TX_PHY */
	omap_writel(0x10000000, 0x4A100610);

	if (dssdev->platform_enable)
		dssdev->platform_enable(dssdev);

	r = hdmi_power_on(dssdev);
	if (r) {
		DSSERR("failed to power on device\n");
		goto err;
	}
	r = request_irq(OMAP44XX_IRQ_DSS_HDMI, hdmi_irq_handler,
			0, "OMAP HDMI", (void *)0);

err:
	mutex_unlock(&hdmi.lock);
	return r;

}

static int hdmi_enable_hpd(struct omap_dss_device *dssdev)
{
	int r = 0;
	DSSDBG("ENTER hdmi_enable_hpd()\n");

	mutex_lock(&hdmi.lock);

	/* the tv overlay manager is shared*/
	r = omap_dss_start_device(dssdev);
	if (r) {
		DSSERR("failed to start device\n");
		goto err;
	}

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED) {
		r = -EINVAL;
		goto err;
	}

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	/* PAD0_HDMI_HPD_PAD1_HDMI_CEC */
	omap_writel(0x01180118, 0x4A100098);
	/* PAD0_HDMI_DDC_SCL_PAD1_HDMI_DDC_SDA */
	omap_writel(0x01180118 , 0x4A10009C);
	/* CONTROL_HDMI_TX_PHY */
	omap_writel(0x10000000, 0x4A100610);

	if (dssdev->platform_enable)
		dssdev->platform_enable(dssdev);

	hpd_mode = 1;
	r = hdmi_min_enable();
	if (r) {
		DSSERR("failed to power on device\n");
		goto err;
	}

err:
	mutex_unlock(&hdmi.lock);
	return r;

}

static void hdmi_disable_display(struct omap_dss_device *dssdev)
{
	DSSDBG("Enter hdmi_disable_display()\n");

	mutex_lock(&hdmi.lock);
	if (dssdev->state == OMAP_DSS_DISPLAY_DISABLED)
		goto end;

	if (dssdev->state == OMAP_DSS_DISPLAY_SUSPENDED) {
		/* suspended is the same as disabled with venc */
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
		goto end;
	}

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	omap_dss_stop_device(dssdev);

	hdmi_power_off(dssdev);

	hdmi.code = 16;
	hdmi.mode = 1 ; /*setting to default only in case of disable and not suspend*/
end:
	mutex_unlock(&hdmi.lock);
}

static int hdmi_display_suspend(struct omap_dss_device *dssdev)
{
	int r = 0;

	DSSDBG("hdmi_display_suspend\n");
		mutex_lock(&hdmi.lock);
	if (dssdev->state == OMAP_DSS_DISPLAY_DISABLED)
		goto end;

	if (dssdev->state == OMAP_DSS_DISPLAY_SUSPENDED)
		goto end;

	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

	omap_dss_stop_device(dssdev);

	hdmi_power_off(dssdev);
end:
	mutex_unlock(&hdmi.lock);
	return r;
}

static int hdmi_display_resume(struct omap_dss_device *dssdev)
{
	int r = 0;

	DSSDBG("hdmi_display_resume\n");
	mutex_lock(&hdmi.lock);

	/* the tv overlay manager is shared*/
	r = omap_dss_start_device(dssdev);
	if (r) {
		DSSERR("failed to start device\n");
		goto err;
	}

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		r = -EINVAL;
		goto err;
	}

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	/* PAD0_HDMI_HPD_PAD1_HDMI_CEC */
	omap_writel(0x01180118, 0x4A100098);
	/* PAD0_HDMI_DDC_SCL_PAD1_HDMI_DDC_SDA */
	omap_writel(0x01180118 , 0x4A10009C);
	/* CONTROL_HDMI_TX_PHY */
	omap_writel(0x10000000, 0x4A100610);

	if (dssdev->platform_enable)
		dssdev->platform_enable(dssdev);

	r = hdmi_power_on(dssdev);
	if (r) {
		DSSERR("failed to power on device\n");
		goto err;
	}

err:
	mutex_unlock(&hdmi.lock);

	return r;
}

static void hdmi_get_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void hdmi_set_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	DSSDBG("hdmi_set_timings\n");

	dssdev->panel.timings = *timings;

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		/* turn the hdmi off and on to get new timings to use */
		hdmi_disable_display(dssdev);
		hdmi_enable_display(dssdev);
	}
}

static void hdmi_set_custom_edid_timing_code(struct omap_dss_device *dssdev, int code , int mode)
{
		if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		/* turn the hdmi off and on to get new timings to use */
		hdmi_disable_display(dssdev);
		hdmi.code = code;
		hdmi.mode = mode;
		custom_set = 1;
		hdmi_enable_display(dssdev);
		custom_set = 0;
	}
}

static struct hdmi_cm hdmi_get_code(struct omap_video_timings *timing)
{
	int i = 0, code = -1, temp_vsync = 0, temp_hsync = 0;
	int timing_vsync = 0, timing_hsync = 0;
	struct omap_video_timings temp;
	struct hdmi_cm cm = {-1};
	DSSDBG("hdmi_get_code");

	for (i = 0; i < 31; i++) {
		temp = all_timings_direct[i];
		if ((temp.pixel_clock == timing->pixel_clock) &&
			(temp.x_res == timing->x_res) &&
			(temp.y_res == timing->y_res)) {

			temp_hsync = temp.hfp + temp.hsw + temp.hbp;
			timing_hsync = timing->hfp + timing->hsw + timing->hbp;
			temp_vsync = temp.vfp + temp.vsw + temp.vbp;
			timing_vsync = timing->vfp + timing->vsw + timing->vbp;

			DSSDBG("Temp_hsync = %d , temp_vsync = %d , \
				timing_hsync = %d, timing_vsync = %d", \
				temp_hsync, temp_hsync, timing_hsync, timing_vsync);

			if ((temp_hsync == timing_hsync)  &&  (temp_vsync == timing_vsync)) {
				code = i;
				cm.code = code_index[i];
				if (code < 14)
					cm.mode = 1;
				else
					cm.mode = 0;
				DSSDBG("Hdmi_code = %d mode = %d\n", cm.code, cm.mode);
				print_omap_video_timings(&temp);
				break;
			}
		}
	}
	return cm;
}

static int hdmi_check_timings(struct omap_dss_device *dssdev,
			struct omap_video_timings *timings)
{
	DSSDBG("hdmi_check_timings\n");

	if (memcmp(&dssdev->panel.timings, timings, sizeof(*timings)) == 0)
		return 0;

	return -EINVAL;
}

int hdmi_init_display(struct omap_dss_device *dssdev)
{
	DSSDBG("init_display\n");

	/* register HDMI specific sysfs files */
	if (device_create_file(&dssdev->dev, &dev_attr_edid) ||
			device_create_file(&dssdev->dev, &dev_attr_custom_edid_timing) ||
			device_create_file(&dssdev->dev, &dev_attr_hpd_enabled))
		DSSERR("failed to create sysfs files\n");

	return 0;
}

static int hdmi_read_edid(struct omap_video_timings *dp)
{
	int ret = 0, code;

	memset(edid, 0, HDMI_EDID_MAX_LENGTH);

	if (!edid_set) {
		ret = HDMI_CORE_DDC_READEDID(HDMI_CORE_SYS, edid);
	}

	if (ret != 0) {
		printk(KERN_WARNING "HDMI failed to read E-EDID\n");
	} else {
		if (!memcmp(edid, header, sizeof(header))) {
			/* search for timings of default resolution */
			if (get_edid_timing_data((struct HDMI_EDID *)edid))
				edid_set = true;
		}
	}

	if (!edid_set) {
		DSSDBG("fallback to VGA\n");
		hdmi.code = 4; /*setting default value of 640 480 VGA*/
		hdmi.mode = 0;
	}

	code = get_timings_index();

	*dp = all_timings_direct[code];
	print_omap_video_timings(dp);

	return 0;
}

static int get_horz_vert_timing_info(union HDMI_EDID_DTD *edid_dtd, struct omap_video_timings *timings)
{
	if (edid_dtd->video.pixel_clock) {
		struct HDMI_EDID_DTD_VIDEO *vid = &edid_dtd->video;
		timings->pixel_clock = 10 * vid->pixel_clock;
		timings->x_res = vid->horiz_active | (((u16)vid->horiz_high & 0xf0) << 4);
		timings->y_res = vid->vert_active | (((u16)vid->vert_high & 0xf0) << 4);
		timings->hfp = vid->horiz_sync_offset | (((u16)vid->sync_pulse_high & 0xc0) << 2);
		timings->hsw = vid->horiz_sync_pulse | (((u16)vid->sync_pulse_high & 0x30) << 4);
		timings->hbp = (vid->horiz_blanking | (((u16)vid->horiz_high & 0x0f) << 8)) -
				(timings->hfp + timings->hsw);
		timings->vfp = ((vid->vert_sync_pulse & 0xf0) >> 4) | ((vid->sync_pulse_high & 0x0f) << 2);
		timings->vsw = (vid->vert_sync_pulse & 0x0f) | ((vid->sync_pulse_high & 0x03) << 4);
		timings->vbp = (vid->vert_blanking | (((u16)vid->vert_high & 0x0f) << 8)) -
				(timings->vfp + timings->vsw);
		return 0;
	} else {
		switch (edid_dtd->monitor_name.block_type) {
			case HDMI_EDID_DTD_TAG_STANDARD_TIMING_DATA: {
				DSSDBG("standard timing data\n");
				return 1;
			}
			case HDMI_EDID_DTD_TAG_COLOR_POINT_DATA: {
				DSSDBG("color point data\n");
				return 1;
			}
			case HDMI_EDID_DTD_TAG_MONITOR_NAME: {
				DSSDBG("monitor name: %s\n", edid_dtd->monitor_name.text);
				return 1;
			}
			case HDMI_EDID_DTD_TAG_MONITOR_LIMITS: {
				int i, max_area = 0, best_idx = -1;
				struct HDMI_EDID_DTD_MONITOR *limits = &edid_dtd->monitor_limits;

				DSSDBG("monitor limits\n");
				DSSDBG("  min_vert_freq=%d\n", limits->min_vert_freq);
				DSSDBG("  max_vert_freq=%d\n", limits->max_vert_freq);
				DSSDBG("  min_horiz_freq=%d\n", limits->min_horiz_freq);
				DSSDBG("  max_horiz_freq=%d\n", limits->max_horiz_freq);
				DSSDBG("  pixel_clock_mhz=%d\n", limits->pixel_clock_mhz * 10);

				/* loop through supported timings, and find the best matching
				 * resolution.. where highest resolution (w*h) is considered
				 * as best
				 */
/* XXX since this is mainly for DVI monitors, should we only support VESA
 * timings?  My monitor at home would pick 1920x1080 otherwise, but that
 * seems to not work well (monitor blanks out and comes back, and picture
 * doesn't fill full screen, but leaves a black bar on left (native res is
 * 2048x1152).  However if I only consider VESA timings, it picks 1680x1050
 * and the picture is stable and fills whole screen..
 */
				for (i = 14; i < 31; i++) {
					const struct omap_video_timings *timings =
							&all_timings_direct[i];
					int hz, hscan, pixclock;
					int vtotal, htotal;

					htotal = timings->hbp + timings->hfp + timings->hsw +
							timings->x_res;
					vtotal = timings->vbp + timings->vfp + timings->vsw +
							timings->y_res;

					/* NOTE: if we supported interlaced, we'd have to
					 * compensate vtotal accordingly for interlaced modes..
					 */

					pixclock = timings->pixel_clock * 1000;
					hscan = (pixclock + htotal / 2) / htotal;
					hscan = (hscan + 500) / 1000 * 1000;
					hz = (hscan + vtotal / 2) / vtotal;

					hscan /= 1000;
					pixclock /= 1000000;
					DSSDBG("pixclock=%d, hscan=%d, hz=%d\n", pixclock, hscan, hz);

					if ((pixclock < (limits->pixel_clock_mhz * 10)) &&
							(limits->min_horiz_freq <= hscan) &&
							(hscan <= limits->max_horiz_freq) &&
							(limits->min_vert_freq <= hz) &&
							(hz <= limits->max_vert_freq)) {
						int area = timings->x_res * timings->y_res;

						DSSDBG(" -> %d: %dx%d\n", i, timings->x_res,
								timings->y_res);

						if (area > max_area) {
							max_area = area;
							best_idx = i;
						}
					}
				}

				if (best_idx > 0) {
					*timings = all_timings_direct[best_idx];
					DSSDBG("found best resolution: %dx%d (%d)\n",
							timings->x_res, timings->y_res, best_idx);
				}
				return 0;
			}
			case HDMI_EDID_DTD_TAG_ASCII_STRING: {
				DSSDBG("ascii string: %s\n", edid_dtd->ascii.text);
				return 1;
			}
			case HDMI_EDID_DTD_TAG_MONITOR_SERIALNUM: {
				DSSDBG("monitor serialnum: %s\n", edid_dtd->monitor_serial_number.text);
				return 1;
			}
			default: {
				DSSDBG("unsupported EDID descriptor block format\n");
				return 1;
			}
		}
	}
}

/*------------------------------------------------------------------------------
 | Function    : get_edid_timing_data
 +------------------------------------------------------------------------------
 | Description : This function gets the resolution information from EDID
 |
 | Parameters  : void
 |
 | Returns     : void
 +----------------------------------------------------------------------------*/
static int get_edid_timing_data(struct HDMI_EDID *edid)
{
	u8 count;
	struct hdmi_cm cm;
	struct omap_video_timings edid_timings = {0};

	/* Search block 0, there are 4 DTDs arranged in priority order */
	for (count = 0; count < EDID_SIZE_BLOCK0_TIMING_DESCRIPTOR; count++) {
		if (!get_horz_vert_timing_info(&edid->DTD[count], &edid_timings)) {
			DSSDBG("Block0 [%d] timings:\n", count);
			print_omap_video_timings(&edid_timings);
			cm = hdmi_get_code(&edid_timings);
			DSSDBG("Block0 [%d] value matches code = %d , mode = %d\n", count, cm.code, cm.mode);
			if (cm.code != -1) {
				hdmi.code = cm.code;
				hdmi.mode = cm.mode;
				return 1;
			}
		} else {
			DSSDBG("Block0 [%d] unsupported!!\n", count);
		}
	}

	/* if we haven't yet found something suitable, check if extension-edid is
	 * present:
	 */
	if (edid->extension_edid) {
		DSSDBG("extension_edid=%d, extension_rev=%d, offset_dtd=%d, num_dtd=%d\n",
				edid->extension_edid, edid->extention_rev,
				edid->offset_dtd, edid->num_dtd);
		/* todo: support extension_edid.. */
		printk(KERN_WARNING "extension_edid not supported yet\n");
	}

	/* and as last resort, check for best standard timing supported:
	 *
	 * note: maybe we want an option to disable this in strict-hdmi
	 * compliance mode?  The below is really for DVI monitors, but
	 * should be ok as long as they don't populate timing_1 and
	 * timing_2 with garbage data..
	 */

	/* no resolutions in edid->timing_3 are supported */

	/* Bitfields for DPMS established timings 2:
	 * Bit(s)	Description
	 * 0	800x600 @ 72 Hz (VESA)
	 * 1	800x600 @ 75 Hz (VESA)
	 * 2	832x624 @ 75 Hz (Mac II)
	 * 3	1024x768 @ 87 Hz interlaced (8514A)
	 * 4	1024x768 @ 60 Hz (VESA)
	 * 5	1024x768 @ 70 Hz (VESA)
	 * 6	1024x768 @ 75 Hz (VESA)
	 * 7	1280x1024 @ 75 Hz (VESA)
	 */

	if (edid->timing_2 & 0x01) {
		DSSDBG("1280x1024@75Hz not supported\n");
	}
	if (edid->timing_2 & 0x02) {
		DSSDBG("1024x768@75Hz not supported\n");
	}
	if (edid->timing_2 & 0x04) {
		DSSDBG("1024x768@70Hz not supported\n");
	}
	if (edid->timing_2 & 0x08) {
		DSSDBG("1024x768@60Hz\n");
		hdmi.mode = 0;
		hdmi.code = 16;
		return 1;
	}
	if (edid->timing_2 & 0x10) {
		DSSDBG("1024x768@87Hz Interlaced not supported\n");
	}
	if (edid->timing_2 & 0x20) {
		DSSDBG("832x624@75Hz not supported\n");
	}
	if (edid->timing_2 & 0x40) {
		DSSDBG("800x600@75Hz not supported\n");
	}
	if (edid->timing_2 & 0x80) {
		DSSDBG("800x600@72Hz not supported\n");
	}

	/* Bitfields for DPMS established timings 1:
	 * Bit(s)	Description
	 * 0	720x400 @ 70 Hz (VGA 640x400, IBM)
	 * 1	720x400 @ 88 Hz (XGA2)
	 * 2	640x480 @ 60 Hz (VGA)
	 * 3	640x480 @ 67 Hz (Mac II, Apple)
	 * 4	640x480 @ 72 Hz (VESA)
	 * 5	640x480 @ 75 Hz (VESA)
	 * 6	800x600 @ 56 Hz (VESA)
	 * 7	800x600 @ 60 Hz (VESA)  -- 9
	 */

	if (edid->timing_1 & 0x01) {
		DSSDBG("800x600@60Hz\n");
		hdmi.mode = 0;
		hdmi.code = 16;
		return 1;
	}
	if (edid->timing_1 & 0x02) {
		DSSDBG("800x600@56Hz not supported\n");
	}
	if (edid->timing_1 & 0x04) {
		DSSDBG("640x480@75Hz not supported\n");
	}
	if (edid->timing_1 & 0x08) {
		DSSDBG("640x480@72Hz not supported\n");
	}
	if (edid->timing_1 & 0x10) {
		DSSDBG("640x480@67Hz not supported\n");
	}
	if (edid->timing_1 & 0x20) {
		DSSDBG("640x480@60Hz\n");
		hdmi.mode = 1;
		hdmi.code = 1;
		return 1;
	}
	if (edid->timing_1 & 0x40) {
		DSSDBG("720x400@88Hz not supported\n");
	}
	if (edid->timing_1 & 0x80) {
		DSSDBG("720x400@70Hz not supported\n");
	}

	return 0;
}

void hdmi_dump_regs(struct seq_file *s)
{
	DSSDBG("0x4a100060 x%x\n", omap_readl(0x4A100060));
	DSSDBG("0x4A100088 x%x\n", omap_readl(0x4A100088));
	DSSDBG("0x48055134 x%x\n", omap_readl(0x48055134));
	DSSDBG("0x48055194 x%x\n", omap_readl(0x48055194));
}

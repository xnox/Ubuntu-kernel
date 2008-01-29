/*
 * Copyright (C) 2005, 2006, 2007 Junjiro Okajima
 *
 * This program, aufs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* $Id: debug.h,v 1.41 2007/12/17 03:30:45 sfjro Exp $ */

#ifndef __AUFS_DEBUG_H__
#define __AUFS_DEBUG_H__

#ifdef __KERNEL__

#include <linux/fs.h>

#define MtxMustLock(mtx)	AuDebugOn(!mutex_is_locked(mtx))

#ifdef CONFIG_AUFS_DEBUG
/* sparse warns about pointer */
#define AuDebugOn(a)		BUG_ON(!!(a))
extern atomic_t aufs_cond;
#define au_debug_on()		atomic_inc_return(&aufs_cond)
#define au_debug_off()		atomic_dec_return(&aufs_cond)
#define au_test_debug()		atomic_read(&aufs_cond)
#else
#define AuDebugOn(a)		do {} while (0)
#define au_debug_on()		do {} while (0)
#define au_debug_off()		do {} while (0)
#define au_test_debug()		0
#endif /* CONFIG_AUFS_DEBUG */

#if defined(CONFIG_AUFS_DEBUG) && defined(CONFIG_MAGIC_SYSRQ)
int __init au_sysrq_init(void);
void au_sysrq_fin(void);
#else
#define au_sysrq_init()		0
#define au_sysrq_fin()		do {} while (0)
#endif /* CONFIG_AUFS_DEBUG && CONFIG_MAGIC_SYSRQ */

/* ---------------------------------------------------------------------- */

/* debug print */
#if defined(CONFIG_LKTR) || defined(CONFIG_LKTR_MODULE)
#include <linux/lktr.h>
#ifdef CONFIG_AUFS_DEBUG
#undef LktrCond
#define LktrCond	unlikely(au_test_debug() || (lktr_cond && lktr_cond()))
#endif
#else
#define LktrCond			au_test_debug()
#define LKTRDumpVma(pre, vma, suf)	do {} while (0)
#define LKTRDumpStack()			do {} while (0)
#define LKTRTrace(fmt, args...) do { \
	if (LktrCond) \
		AuDbg(fmt, ##args); \
} while (0)
#define LKTRLabel(label)		LKTRTrace("%s\n", #label)
#endif /* CONFIG_LKTR */

#define AuTraceErr(e) do { \
	if (unlikely((e) < 0)) \
		LKTRTrace("err %d\n", (int)(e)); \
} while (0)

#define AuTraceErrPtr(p) do { \
	if (IS_ERR(p)) \
		LKTRTrace("err %ld\n", PTR_ERR(p)); \
} while (0)

#define AuTraceEnter()	LKTRLabel(enter)

/* dirty macros for debug print, use with "%.*s" and caution */
#define AuLNPair(qstr)		(qstr)->len, (qstr)->name
#define AuDLNPair(d)		AuLNPair(&(d)->d_name)

/* ---------------------------------------------------------------------- */

#define AuDpri(lvl, fmt, arg...) \
	printk(lvl AUFS_NAME " %s:%d:%s[%d]: " fmt, \
	       __func__, __LINE__, current->comm, current->pid, ##arg)
#define AuDbg(fmt, arg...)	AuDpri(KERN_DEBUG, fmt, ##arg)
#define AuInfo(fmt, arg...)	AuDpri(KERN_INFO, fmt, ##arg)
#define AuWarn(fmt, arg...)	AuDpri(KERN_WARNING, fmt, ##arg)
#define AuErr(fmt, arg...)	AuDpri(KERN_ERR, fmt, ##arg)
#define AuIOErr(fmt, arg...)	AuErr("I/O Error, " fmt, ##arg)
#define AuIOErrWhck(fmt, arg...) AuErr("I/O Error, try whck. " fmt, ##arg)
#define AuWarn1(fmt, arg...) do { \
	static unsigned char _c; \
	if (!_c++) AuWarn(fmt, ##arg); \
} while (0)

#define AuErr1(fmt, arg...) do { \
	static unsigned char _c; \
	if (!_c++) AuErr(fmt, ##arg); \
} while (0)

#define AuIOErr1(fmt, arg...) do { \
	static unsigned char _c; \
	if (!_c++) AuIOErr(fmt, ##arg); \
} while (0)

#define AuUnsupportMsg	"This operation is not supported." \
			" Please report me this application."
#define AuUnsupport(fmt, args...) do { \
	AuErr(AuUnsupportMsg "\n" fmt, ##args); \
	dump_stack(); \
} while (0)

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_DEBUG
struct aufs_nhash;
void au_dpri_whlist(struct aufs_nhash *whlist);
struct aufs_vdir;
void au_dpri_vdir(struct aufs_vdir *vdir);
void au_dpri_inode(struct inode *inode);
void au_dpri_dentry(struct dentry *dentry);
void au_dpri_file(struct file *filp);
void au_dpri_sb(struct super_block *sb);
void au_dbg_sleep(int sec);
#define AuDbgWhlist(w) do { \
	LKTRTrace(#w "\n"); \
	au_dpri_whlist(w); \
} while (0)

#define AuDbgVdir(v) do { \
	LKTRTrace(#v "\n"); \
	au_dpri_vdir(v); \
} while (0)

#define AuDbgInode(i) do { \
	LKTRTrace(#i "\n"); \
	au_dpri_inode(i); \
} while (0)

#define AuDbgDentry(d) do { \
	LKTRTrace(#d "\n"); \
	au_dpri_dentry(d); \
} while (0)

#define AuDbgFile(f) do { \
	LKTRTrace(#f "\n"); \
	au_dpri_file(f); \
} while (0)

#define AuDbgSb(sb) do { \
	LKTRTrace(#sb "\n"); \
	au_dpri_sb(sb); \
} while (0)

#define AuDbgSleep(sec) do { \
	AuDbg("sleep %d sec\n", sec); \
	au_dbg_sleep(sec); \
} while (0)
#else
#define AuDbgWhlist(w)		do {} while (0)
#define AuDbgVdir(v)		do {} while (0)
#define AuDbgInode(i)		do {} while (0)
#define AuDbgDentry(d)		do {} while (0)
#define AuDbgFile(f)		do {} while (0)
#define AuDbgSb(sb)		do {} while (0)
#define AuDbgSleep(sec)		do {} while (0)
#endif /* CONFIG_AUFS_DEBUG */

#endif /* __KERNEL__ */
#endif /* __AUFS_DEBUG_H__ */

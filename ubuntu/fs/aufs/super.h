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

/* $Id: super.h,v 1.67 2007/12/17 03:30:59 sfjro Exp $ */

#ifndef __AUFS_SUPER_H__
#define __AUFS_SUPER_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/version.h>
#include <linux/cramfs_fs.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
#include <linux/magic.h>
#else
#include <linux/nfs_fs.h>
#endif
#include <linux/aufs_type.h>
//#include "hinode.h"
#include "misc.h"
#include "opts.h"
#include "sysaufs.h"
#include "wkq.h"

typedef ssize_t (*readf_t)(struct file *, char __user *, size_t, loff_t *);
typedef ssize_t (*writef_t)(struct file *, const char __user *, size_t,
			    loff_t *);

#ifdef CONFIG_AUFS_SYSAUFS
/* entries under sysfs per mount-point */
enum {
	SysaufsSb_XINO,
#if 0
	SysaufsSb_PLINK,
	SysaufsSb_files,
#endif
	SysaufsSb_Last
};

struct sysaufs_sbinfo {
	au_subsys_t		subsys;
	struct sysaufs_entry	array[SysaufsSb_Last];
};

extern sysaufs_op au_si_ops[];

#else
struct sysaufs_sbinfo {};
#endif /* CONFIG_AUFS_SYSAUFS */

struct au_wbr_copyup_operations {
	int (*copyup)(struct dentry *dentry);
};

struct au_wbr_create_operations {
	int (*create)(struct dentry *dentry, int isdir);
	int (*init)(struct super_block *sb);
	int (*fin)(struct super_block *sb);
};

struct au_wbr_mfs {
	struct mutex	mfs_lock;
	unsigned long	mfs_jiffy;
	unsigned long	mfs_expire;
	aufs_bindex_t	mfs_bindex;

	u64		mfsrr_bytes;
	u64		mfsrr_watermark;
};

struct aufs_branch;
struct aufs_sbinfo {
	/* nowait tasks in the system-wide workqueue */
	struct au_nowait_tasks	si_nowait;

	struct aufs_rwsem	si_rwsem;

	/* branch management */
	au_gen_t		si_generation;

	/*
	 * set true when refresh_dirs() at remount time failed.
	 * then try refreshing dirs at access time again.
	 * if it is false, refreshing dirs at access time is unnecesary
	 */
	unsigned int		si_failed_refresh_dirs:1;

	aufs_bindex_t		si_bend;
	aufs_bindex_t		si_last_br_id;
	struct aufs_branch	**si_branch;

	/* policy to select a writable branch */
	struct au_wbr_copyup_operations *si_wbr_copyup_ops;
	struct au_wbr_create_operations *si_wbr_create_ops;

	/* round robin */
	atomic_t		si_wbr_rr_next;

	/* most free space */
	struct au_wbr_mfs	si_wbr_mfs;

	/* mount flags */
	/* include/asm-ia64/siginfo.h defines a macro named si_flags */
	struct au_opts_flags	au_si_flags;

	/* external inode number (bitmap and translation table) */
	readf_t			si_xread;
	writef_t		si_xwrite;
	struct file		*si_xib;
	struct mutex		si_xib_mtx; /* protect xib members */
	unsigned long		*si_xib_buf;
	unsigned long		si_xib_last_pindex;
	int			si_xib_next_bit;
	//unsigned long long	si_xib_limit;	/* Max xib file size */

	/* readdir cache time, max, in HZ */
	unsigned long		si_rdcache;

	/*
	 * If the number of whiteouts are larger than si_dirwh, leave all of
	 * them after rename_whtmp to reduce the cost of rmdir(2).
	 * future fsck.aufs or kernel thread will remove them later.
	 * Otherwise, remove all whiteouts and the dir in rmdir(2).
	 */
	unsigned int		si_dirwh;

	/*
	 * rename(2) a directory with all children.
	 */
	//int			si_rendir;

	/* pseudo_link list */ // dirty
	spinlock_t		si_plink_lock;
	struct list_head	si_plink;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
	/* super_blocks list is not exported */
	struct list_head	si_list;
	struct vfsmount		*si_mnt;	/* no get/put */
#endif

	/* sysfs */
	struct sysaufs_sbinfo	si_sysaufs;

#ifdef CONFIG_AUFS_ROBR
	/* locked vma list for mmap() */ // very dirty
	spinlock_t		si_lvma_lock;
	struct list_head	si_lvma;
#endif
};

/* ---------------------------------------------------------------------- */
/* mount flags */
#define AuFlag(sbinfo, name)		({ (sbinfo)->au_si_flags.name; })
#define AuFlagSet(sbinfo, name, val)	{ (sbinfo)->au_si_flags.name = (val); }

/* policy to select one among writable branches */
#define AuWbrCopyup(sbinfo, args... ) \
	(sbinfo)->si_wbr_copyup_ops->copyup(args)
#define AuWbrCreate(sbinfo, args... ) \
	(sbinfo)->si_wbr_create_ops->create(args)

/* flags for si_read_lock()/aufs_read_lock()/di_read_lock() */
#define AuLock_DW		1
#define AuLock_IR		(1 << 1)
#define AuLock_IW		(1 << 2)
#define AuLock_FLUSH		(1 << 3)
#define AuLock_DIR		(1 << 4)

/* ---------------------------------------------------------------------- */

/* super.c */
int au_show_brs(struct seq_file *seq, struct super_block *sb);
extern struct file_system_type aufs_fs_type;

/* sbinfo.c */
struct aufs_sbinfo *stosi(struct super_block *sb);
aufs_bindex_t sbend(struct super_block *sb);
struct aufs_branch *stobr(struct super_block *sb, aufs_bindex_t bindex);
au_gen_t au_sigen(struct super_block *sb);
au_gen_t au_sigen_inc(struct super_block *sb);
int find_bindex(struct super_block *sb, struct aufs_branch *br);

void aufs_read_lock(struct dentry *dentry, int flags);
void aufs_read_unlock(struct dentry *dentry, int flags);
void aufs_write_lock(struct dentry *dentry);
void aufs_write_unlock(struct dentry *dentry);
void aufs_read_and_write_lock2(struct dentry *d1, struct dentry *d2, int isdir);
void aufs_read_and_write_unlock2(struct dentry *d1, struct dentry *d2);

aufs_bindex_t new_br_id(struct super_block *sb);

/* wbr_policy.c */
extern struct au_wbr_copyup_operations au_wbr_copyup_ops[];
extern struct au_wbr_create_operations au_wbr_create_ops[];
int au_cpdown_dirs(struct dentry *dentry, aufs_bindex_t bdst,
		   struct dentry *locked);

/* ---------------------------------------------------------------------- */

static inline const char *au_sbtype(struct super_block *sb)
{
	return sb->s_type->name;
}

static inline int au_test_aufs(struct super_block *sb)
{
#ifdef AUFS_SUPER_MAGIC
	return (sb->s_magic == AUFS_SUPER_MAGIC);
#else
	return !strcmp(au_sbtype(sb), AUFS_FSTYPE);
#endif
}

static inline int au_test_fuse(struct super_block *sb)
{
	int ret = 0;
#ifdef CONFIG_AUFS_WORKAROUND_FUSE
#ifdef FUSE_SUPER_MAGIC
	BUILD_BUG_ON(FUSE_SUPER_MAGIC != 0x65735546);
	ret = (sb->s_magic == FUSE_SUPER_MAGIC);
#else
	ret = !strcmp(au_sbtype(sb), "fuse");
#endif
#endif
	return ret;
}

static inline int au_test_xfs(struct super_block *sb)
{
	int ret = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24) \
	&& (defined(CONFIG_XFS_FS) || defined(CONFIG_XFS_FS_MODULE))
#ifdef XFS_SB_MAGIC
	BUILD_BUG_ON(XFS_SB_MAGIC != 0x58465342);
	ret = (sb->s_magic == XFS_SB_MAGIC);
#else
	ret = !strcmp(au_sbtype(sb), "xfs");
#endif
#endif
	return ret;
}

static inline int au_test_nfs(struct super_block *sb)
{
#if defined(CONFIG_NFS_FS) || defined(CONFIG_NFS_FS_MODULE)
	return (sb->s_magic == NFS_SUPER_MAGIC);
#else
	return 0;
#endif
}

static inline int au_test_trunc_xino(struct super_block *sb)
{
	int ret = 0;
#ifdef CONFIG_TMPFS
#ifdef TMPFS_MAGIC
	BUILD_BUG_ON(TMPFS_MAGIC != 0x01021994);
	ret = (sb->s_magic == TMPFS_MAGIC);
#else
	ret = !strcmp(au_sbtype(sb), "tmpfs");
#endif
#endif
	return ret;
}

/* temporary support for i#1 in cramfs */
static inline int au_test_unique_ino(struct dentry *h_dentry, ino_t h_ino)
{
#if defined(CONFIG_CRAMFS) || defined(CONFIG_CRAMFS_MODULE)
	if (unlikely(h_dentry->d_sb->s_magic == CRAMFS_MAGIC))
		return (h_ino != 1);
#endif
	return 1;
}

#ifdef CONFIG_AUFS_EXPORT
extern struct export_operations aufs_export_op;
static inline void au_init_export_op(struct super_block *sb)
{
	sb->s_export_op = &aufs_export_op;
}

static inline int au_test_nfsd(struct task_struct *tsk)
{
	return (!tsk->mm && !strcmp(tsk->comm, "nfsd"));
}

static inline void au_nfsd_lockdep_off(void)
{
	if (au_test_nfsd(current))
		lockdep_off();
}

static inline void au_nfsd_lockdep_on(void)
{
	if (au_test_nfsd(current))
		lockdep_on();
}

#else

static inline int au_test_nfsd(struct task_struct *tsk)
{
	return 0;
}

static inline void au_init_export_op(struct super_block *sb)
{
	/* nothing */
}

#define au_nfsd_lockdep_off()	do {} while (0)
#define au_nfsd_lockdep_on()	do {} while (0)

#endif /* CONFIG_AUFS_EXPORT */

static inline void init_lvma(struct aufs_sbinfo *sbinfo)
{
#ifdef CONFIG_AUFS_ROBR
	spin_lock_init(&sbinfo->si_lvma_lock);
	INIT_LIST_HEAD(&sbinfo->si_lvma);
#else
	/* nothing */
#endif
}

/* ---------------------------------------------------------------------- */

/* limited support before 2.6.18 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
static inline void au_mntget(struct super_block *sb)
{
	mntget(stosi(sb)->si_mnt);
}

static inline void au_mntput(struct super_block *sb)
{
	mntput(stosi(sb)->si_mnt);
}
#else
static inline void au_mntget(struct super_block *sb)
{
	/* empty */
}

static inline void au_mntput(struct super_block *sb)
{
	/* empty */
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18) */

/* ---------------------------------------------------------------------- */

static inline unsigned int au_flag_test_udba_inotify(struct super_block *sb)
{
#ifdef CONFIG_AUFS_HINOTIFY
	return (AuFlag(stosi(sb), f_udba) == AuUdba_INOTIFY);
#else
	return 0;
#endif
}

static inline int au_need_dlgt(struct super_block *sb)
{
#ifdef CONFIG_AUFS_DLGT
	return (AuFlag(stosi(sb), f_dlgt) && !au_test_wkq(current));
#else
	return 0;
#endif
}

/* ---------------------------------------------------------------------- */

/* lock superblock. mainly for entry point functions */
/*
 * si_noflush_read_lock, si_noflush_write_lock,
 * si_read_unlock, si_write_unlock, si_downgrade_lock
 */
SimpleLockRwsemFuncs(si_noflush, struct super_block *sb, stosi(sb)->si_rwsem);
SimpleUnlockRwsemFuncs(si, struct super_block *sb, stosi(sb)->si_rwsem);

static inline void si_read_lock(struct super_block *sb, int flags)
{
	if (unlikely(flags & AuLock_FLUSH))
		au_nwt_flush(&stosi(sb)->si_nowait);
	si_noflush_read_lock(sb);
}

static inline void si_write_lock(struct super_block *sb)
{
	au_nwt_flush(&stosi(sb)->si_nowait);
	si_noflush_write_lock(sb);
}

static inline int si_read_trylock(struct super_block *sb, int flags)
{
	if (unlikely(flags & AuLock_FLUSH))
		au_nwt_flush(&stosi(sb)->si_nowait);
	return si_noflush_read_trylock(sb);
}

static inline int si_write_trylock(struct super_block *sb)
{
	au_nwt_flush(&stosi(sb)->si_nowait);
	return si_noflush_write_trylock(sb);
}

/* to debug easier, do not make them inlined functions */
#define SiMustReadLock(sb)	RwMustReadLock(&stosi(sb)->si_rwsem)
#define SiMustWriteLock(sb)	RwMustWriteLock(&stosi(sb)->si_rwsem)
#define SiMustAnyLock(sb)	RwMustAnyLock(&stosi(sb)->si_rwsem)

#endif /* __KERNEL__ */
#endif /* __AUFS_SUPER_H__ */

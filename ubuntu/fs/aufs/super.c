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

/* $Id: super.c,v 1.73 2008/01/28 05:02:15 sfjro Exp $ */

#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/smp_lock.h>
#include <linux/statfs.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
#include <linux/mnt_namespace.h>
typedef struct mnt_namespace au_mnt_ns_t;
#define au_nsproxy(tsk)	(tsk)->nsproxy
#define au_mnt_ns(tsk)	(tsk)->nsproxy->mnt_ns
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
#include <linux/namespace.h>
typedef struct namespace au_mnt_ns_t;
#define au_nsproxy(tsk)	(tsk)->nsproxy
#define au_mnt_ns(tsk)	(tsk)->nsproxy->namespace
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
#include <linux/namespace.h>
typedef struct namespace au_mnt_ns_t;
#define au_nsproxy(tsk)	(tsk)->namespace
#define au_mnt_ns(tsk)	(tsk)->namespace
#endif

#include "aufs.h"

/*
 * super_operations
 */
static struct inode *aufs_alloc_inode(struct super_block *sb)
{
	struct aufs_icntnr *c;

	AuTraceEnter();

	c = au_cache_alloc_icntnr();
	//if (LktrCond) {au_cache_free_icntnr(c); c = NULL;}
	if (c) {
		inode_init_once(&c->vfs_inode);
		c->vfs_inode.i_version = 1; //sigen(sb);
		c->iinfo.ii_hinode = NULL;
		return &c->vfs_inode;
	}
	return NULL;
}

static void aufs_destroy_inode(struct inode *inode)
{
	LKTRTrace("i%lu\n", inode->i_ino);
	au_iinfo_fin(inode);
	au_cache_free_icntnr(container_of(inode, struct aufs_icntnr,
					  vfs_inode));
}

//todo: how about merge with alloc_inode()?
static void aufs_read_inode(struct inode *inode)
{
	int err;
#if 0
	static struct backing_dev_info bdi = {
		.ra_pages	= 0,	/* No readahead */
		.capabilities	= BDI_CAP_NO_ACCT_DIRTY | BDI_CAP_NO_WRITEBACK
	};
#endif

	LKTRTrace("i%lu\n", inode->i_ino);

	err = au_iinfo_init(inode);
	//if (LktrCond) err = -1;
	if (!err) {
		inode->i_version++;
		inode->i_op = &aufs_iop;
		inode->i_fop = &aufs_file_fop;
		inode->i_mapping->a_ops = &aufs_aop;
		//inode->i_mapping->backing_dev_info = &bdi;
		return; /* success */
	}

	LKTRTrace("intializing inode info failed(%d)\n", err);
	make_bad_inode(inode);
}

int au_show_brs(struct seq_file *seq, struct super_block *sb)
{
	int err;
	aufs_bindex_t bindex, bend;
	struct dentry *root;

	AuTraceEnter();
	SiMustAnyLock(sb);
	root = sb->s_root;
	DiMustAnyLock(root);

	err = 0;
	bend = sbend(sb);
	for (bindex = 0; !err && bindex <= bend; bindex++) {
		err = seq_path(seq, sbr_mnt(sb, bindex),
			       au_h_dptr_i(root, bindex), au_esc_chars);
		if (err > 0)
			err = seq_printf
				(seq, "=%s",
				 au_optstr_br_perm(sbr_perm(sb, bindex)));
		if (!err && bindex != bend)
			err = seq_putc(seq, ':');
	}

	AuTraceErr(err);
	return err;
}

static int aufs_show_options(struct seq_file *m, struct vfsmount *mnt)
{
	int err, n;
	struct super_block *sb;
	struct aufs_sbinfo *sbinfo;
	struct dentry *root;
	struct file *xino;
	struct au_opts_flags def;

	AuTraceEnter();

	sb = mnt->mnt_sb;
	root = sb->s_root;
	aufs_read_lock(root, !AuLock_IR);
	sbinfo = stosi(sb);
	if (AuFlag(sbinfo, f_xino) == AuXino_XINO) {
		err = seq_puts(m, ",xino=");
		if (unlikely(err))
			goto out;
		xino = sbinfo->si_xib;
		err = seq_path(m, xino->f_vfsmnt, xino->f_dentry, au_esc_chars);
		if (unlikely(err <= 0))
			goto out;
		err = 0;

#define Deleted "\\040(deleted)"
		m->count -= sizeof(Deleted) - 1;
		AuDebugOn(memcmp(m->buf + m->count, Deleted,
				 sizeof(Deleted) - 1));
#undef Deleted
	} else
		err = seq_puts(m, ",noxino");

	au_opts_flags_def(&def);

#define Print(name) do { \
	n = AuFlag(sbinfo, f_##name); \
	if (unlikely(!err && n != def.f_##name)) \
		err = seq_printf(m, ",%s"#name, n ? "" : "no"); \
} while (0)

#define PrintStr(name, str) do { \
	n = AuFlag(sbinfo, f_##name); \
	if (unlikely(!err && n != def.f_##name)) \
		err = seq_printf(m, "," str "=%s", au_optstr_##name(n)); \
} while (0)

	Print(trunc_xino);
	Print(plink);
	PrintStr(udba, "udba");
	switch (AuFlag(sbinfo, f_wbr_create)) {
	case AuWbrCreate_TDP:
	case AuWbrCreate_RR:
	case AuWbrCreate_MFS:
	case AuWbrCreate_PMFS:
		PrintStr(wbr_create, "create");
		break;
	case AuWbrCreate_MFSV:
		err = seq_printf(m, ",create=mfs:%lu",
				 sbinfo->si_wbr_mfs.mfs_expire / HZ);
		break;
	case AuWbrCreate_PMFSV:
		err = seq_printf(m, ",create=pmfs:%lu",
				 sbinfo->si_wbr_mfs.mfs_expire / HZ);
		break;
	case AuWbrCreate_MFSRR:
		err = seq_printf(m, ",create=mfsrr:%Lu",
				 sbinfo->si_wbr_mfs.mfsrr_watermark);
		break;
	case AuWbrCreate_MFSRRV:
		err = seq_printf(m, ",create=mfsrr:%Lu:%lu",
				 sbinfo->si_wbr_mfs.mfsrr_watermark,
				 sbinfo->si_wbr_mfs.mfs_expire / HZ);
		break;
	default:
		AuDebugOn(1);
	}

	PrintStr(wbr_copyup, "cpup");
	n = AuFlag(sbinfo, f_always_diropq);
	if (unlikely(!err && n != def.f_always_diropq))
		err = seq_printf(m, ",diropq=%c", n ? 'a' : 'w');
	Print(refrof);
	Print(dlgt);
	Print(warn_perm);
	Print(verbose);

	n = sbinfo->si_dirwh;
	if (unlikely(!err && n != AUFS_DIRWH_DEF))
		err = seq_printf(m, ",dirwh=%d", n);
	n = sbinfo->si_rdcache / HZ;
	if (unlikely(!err && n != AUFS_RDCACHE_DEF))
		err = seq_printf(m, ",rdcache=%d", n);

	PrintStr(coo, "coo");

#undef Print
#undef PrintStr

	if (!err && !sysaufs_brs) {
#ifdef CONFIG_AUFS_COMPAT
		err = seq_puts(m, ",dirs=");
#else
		err = seq_puts(m, ",br:");
#endif
		if (!err)
			err = au_show_brs(m, sb);
	}

 out:
	aufs_read_unlock(root, !AuLock_IR);
	AuTraceErr(err);
	if (err)
		err = -E2BIG;
	AuTraceErr(err);
	return err;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
#define StatfsLock(d)	aufs_read_lock((d)->d_sb->s_root, 0)
#define StatfsUnlock(d)	aufs_read_unlock((d)->d_sb->s_root, 0)
#define StatfsArg(d)	au_h_dptr((d)->d_sb->s_root)
#define StatfsHInode(d)	(StatfsArg(d)->d_inode)
#define StatfsSb(d)	((d)->d_sb)
static int aufs_statfs(struct dentry *arg, struct kstatfs *buf)
#else
#define StatfsLock(s)	si_read_lock(s, !AuLock_FLUSH)
#define StatfsUnlock(s)	si_read_unlock(s)
#define StatfsArg(s)	sbr_sb(s, 0)
#define StatfsHInode(s)	(StatfsArg(s)->s_root->d_inode)
#define StatfsSb(s)	(s)
static int aufs_statfs(struct super_block *arg, struct kstatfs *buf)
#endif
{
	int err;

	AuTraceEnter();

	StatfsLock(arg);
	err = vfsub_statfs(StatfsArg(arg), buf, au_need_dlgt(StatfsSb(arg)));
	//if (LktrCond) err = -1;
	StatfsUnlock(arg);
	if (!err) {
		buf->f_type = AUFS_SUPER_MAGIC;
		buf->f_type = 0;
		buf->f_namelen -= AUFS_WH_PFX_LEN;
		//todo: support uuid?
		memset(&buf->f_fsid, 0, sizeof(buf->f_fsid));
	}
	/* buf->f_bsize = buf->f_blocks = buf->f_bfree = buf->f_bavail = -1; */

	AuTraceErr(err);
	return err;
}

static void au_update_mnt(struct vfsmount *mnt, int flags)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
	struct vfsmount *pos;
	struct super_block *sb = mnt->mnt_sb;
	struct dentry *root = sb->s_root;
	struct aufs_sbinfo *sbinfo = stosi(sb);
	au_mnt_ns_t *ns;

	AuTraceEnter();
	AuDebugOn(!kernel_locked());

	if (sbinfo->si_mnt != mnt
	    || atomic_read(&sb->s_active) == 1
	    || !au_nsproxy(current))
		return;

	/* no get/put */
	ns = au_mnt_ns(current);
	AuDebugOn(!ns);
	sbinfo->si_mnt = NULL;
	list_for_each_entry(pos, &ns->list, mnt_list)
		if (pos != mnt && pos->mnt_sb->s_root == root) {
			sbinfo->si_mnt = pos;
			break;
		}
	AuDebugOn(!(flags & MNT_DETACH) && !sbinfo->si_mnt);
#endif
}

#define UmountBeginHasMnt	(LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18))

#if UmountBeginHasMnt
#define UmountBeginSb(mnt)	(mnt)->mnt_sb
#define UmountBeginMnt(mnt)	(mnt)
#define UmountBeginFlags	_flags
static void aufs_umount_begin(struct vfsmount *arg, int _flags)
#else
#define UmountBeginSb(sb)	sb
#define UmountBeginMnt(sb)	NULL
#define UmountBeginFlags	0
static void aufs_umount_begin(struct super_block *arg)
#endif
{
	struct super_block *sb = UmountBeginSb(arg);
	struct vfsmount *mnt = UmountBeginMnt(arg);
	struct aufs_sbinfo *sbinfo;

	AuTraceEnter();

	sbinfo = stosi(sb);
	if (unlikely(!sbinfo))
		return;

	si_write_lock(sb);
	if (AuFlag(sbinfo, f_plink))
		au_put_plink(sb);
#if 0 // remove
	if (unlikely(au_flag_test(sb, AuFlag_UDBA_INOTIFY)))
		shrink_dcache_sb(sb);
#endif
	au_update_mnt(mnt, UmountBeginFlags);
#if 0
	if (sbinfo->si_wbr_create_ops->fin)
		sbinfo->si_wbr_create_ops->fin(sb);
#endif
	si_write_unlock(sb);
}

static void free_sbinfo(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;

	AuTraceEnter();
	sbinfo = stosi(sb);
	AuDebugOn(!sbinfo || !list_empty(&sbinfo->si_plink));

	si_write_lock(sb);
	xino_clr(sb);
	free_branches(sbinfo);
	kfree(sbinfo->si_branch);
	si_write_unlock(sb);
	kfree(sbinfo);
}

/* final actions when unmounting a file system */
static void aufs_put_super(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;

	AuTraceEnter();

	sbinfo = stosi(sb);
	if (unlikely(!sbinfo))
		return;

	sysaufs_del(sbinfo);

#if !UmountBeginHasMnt
	/* umount_begin() may not be called. */
	aufs_umount_begin(sb);
#endif
	free_sbinfo(sb);
}

/* ---------------------------------------------------------------------- */

/*
 * refresh dentry and inode at remount time.
 */
static int do_refresh(struct dentry *dentry, mode_t type,
		      unsigned int dir_flags)
{
	int err;
	struct dentry *parent;
	struct inode *inode;

	LKTRTrace("%.*s, 0%o\n", AuDLNPair(dentry), type);
	inode = dentry->d_inode;
	AuDebugOn(!inode);

	di_write_lock_child(dentry);
	parent = dget_parent(dentry);
	di_read_lock_parent(parent, AuLock_IR);
	/* returns a number of positive dentries */
	err = au_refresh_hdentry(dentry, type);
	//err = -1;
	if (err >= 0) {
		err = au_refresh_hinode(inode, dentry);
		//err = -1;
		if (unlikely(!err && type == S_IFDIR))
			au_reset_hinotify(inode, dir_flags);
	}
	if (unlikely(err))
		AuErr("unrecoverable error %d, %.*s\n", err, AuDLNPair(dentry));
	di_read_unlock(parent, AuLock_IR);
	dput(parent);
	di_write_unlock(dentry);

	AuTraceErr(err);
	return err;
}

static int test_dir(struct dentry *dentry, void *arg)
{
	return S_ISDIR(dentry->d_inode->i_mode);
}

static int refresh_dir(struct dentry *root, au_gen_t sgen)
{
	int err, i, j, ndentry, e;
	const unsigned int flags = au_hi_flags(root->d_inode, /*isdir*/1);
	struct au_dcsub_pages dpages;
	struct au_dpage *dpage;
	struct dentry **dentries;
	struct inode *inode;

	LKTRTrace("sgen %d\n", sgen);
	SiMustWriteLock(root->d_sb);
	AuDebugOn(au_digen(root) != sgen
		  || !kernel_locked());

	err = 0;
	list_for_each_entry(inode, &root->d_sb->s_inodes, i_sb_list)
		if (unlikely(S_ISDIR(inode->i_mode)
			     && au_iigen(inode) != sgen)) {
			ii_write_lock_child(inode);
			e = au_refresh_hinode_self(inode);
			//e = -1;
			ii_write_unlock(inode);
			if (unlikely(e)) {
				LKTRTrace("e %d, i%lu\n", e, inode->i_ino);
				if (!err)
					err = e;
				/* go on even if err */
			}
		}

	e = au_dpages_init(&dpages, GFP_TEMPORARY);
	if (unlikely(e)) {
		if (!err)
			err = e;
		goto out;
	}
	e = au_dcsub_pages(&dpages, root, test_dir, NULL);
	if (unlikely(e)) {
		if (!err)
			err = e;
		goto out_dpages;
	}

	e = 0;
	for (i = 0; !e && i < dpages.ndpage; i++) {
		dpage = dpages.dpages + i;
		dentries = dpage->dentries;
		ndentry = dpage->ndentry;
		for (j = 0; !e && j < ndentry; j++) {
			struct dentry *d;
			d = dentries[j];
#ifdef CONFIG_AUFS_DEBUG
			{
				struct dentry *parent;
				parent = dget_parent(d);
				AuDebugOn(!S_ISDIR(d->d_inode->i_mode)
					  || IS_ROOT(d)
					  || au_digen(parent) != sgen);
				dput(parent);
			}
#endif
			if (au_digen(d) != sgen) {
				e = do_refresh(d, S_IFDIR, flags);
				//e = -1;
				if (unlikely(e && !err))
					err = e;
				/* break on err */
			}
		}
	}

 out_dpages:
	au_dpages_free(&dpages);
 out:
	AuTraceErr(err);
	return err;
}

static int test_nondir(struct dentry *dentry, void *arg)
{
	return !S_ISDIR(dentry->d_inode->i_mode);
}

static int refresh_nondir(struct dentry *root, au_gen_t sgen, int do_dentry)
{
	int err, i, j, ndentry, e;
	struct au_dcsub_pages dpages;
	struct au_dpage *dpage;
	struct dentry **dentries;
	struct inode *inode;

	LKTRTrace("sgen %d\n", sgen);
	SiMustWriteLock(root->d_sb);
	AuDebugOn(au_digen(root) != sgen
		  || !kernel_locked());

	err = 0;
	list_for_each_entry(inode, &root->d_sb->s_inodes, i_sb_list)
		if (unlikely(!S_ISDIR(inode->i_mode)
			     && au_iigen(inode) != sgen)) {
			ii_write_lock_child(inode);
			e = au_refresh_hinode_self(inode);
			//e = -1;
			ii_write_unlock(inode);
			if (unlikely(e)) {
				LKTRTrace("e %d, i%lu\n", e, inode->i_ino);
				if (!err)
					err = e;
				/* go on even if err */
			}
		}

	if (unlikely(!do_dentry))
		goto out;

	e = au_dpages_init(&dpages, GFP_TEMPORARY);
	if (unlikely(e)) {
		if (!err)
			err = e;
		goto out;
	}
	e = au_dcsub_pages(&dpages, root, test_nondir, NULL);
	if (unlikely(e)) {
		if (!err)
			err = e;
		goto out_dpages;
	}

	for (i = 0; i < dpages.ndpage; i++) {
		dpage = dpages.dpages + i;
		dentries = dpage->dentries;
		ndentry = dpage->ndentry;
		for (j = 0; j < ndentry; j++) {
			struct dentry *d;
			d = dentries[j];
#ifdef CONFIG_AUFS_DEBUG
			{
				struct dentry *parent;
				parent = dget_parent(d);
				AuDebugOn(S_ISDIR(d->d_inode->i_mode)
					  || au_digen(parent) != sgen);
				dput(parent);
			}
#endif
			inode = d->d_inode;
			if (inode && au_digen(d) != sgen) {
				e = do_refresh(d, inode->i_mode & S_IFMT, 0);
				//e = -1;
				if (unlikely(e && !err))
					err = e;
				/* go on even err */
			}
		}
	}

 out_dpages:
	au_dpages_free(&dpages);
 out:
	AuTraceErr(err);
	return err;
}

/* stop extra interpretation of errno in mount(8), and strange error messages */
static int cvt_err(int err)
{
	AuTraceErr(err);

	switch (err) {
	case -ENOENT:
	case -ENOTDIR:
	case -EEXIST:
	case -EIO:
		err = -EINVAL;
	}
	return err;
}

/* protected by s_umount */
static int aufs_remount_fs(struct super_block *sb, int *flags, char *data)
{
	int err;
	struct dentry *root;
	struct inode *inode;
	struct au_opts opts;
	unsigned int dlgt;
	struct aufs_sbinfo *sbinfo;

	//au_debug_on();
	LKTRTrace("flags 0x%x, data %s, len %lu\n",
		  *flags, data ? data : "NULL",
		  (unsigned long)(data ? strlen(data) : 0));

	err = 0;
	if (unlikely(!data || !*data))
		goto out; /* success */

	err = -ENOMEM;
	memset(&opts, 0, sizeof(opts));
	opts.opt = (void *)__get_free_page(GFP_TEMPORARY);
	//if (LktrCond) {free_page((unsigned long)opts.opt); opts.opt = NULL;}
	if (unlikely(!opts.opt))
		goto out;
	opts.max_opt = PAGE_SIZE / sizeof(*opts.opt);
	opts.remount = 1;

	//todo: writeback everything on branches.

	/* parse it before aufs lock */
	err = au_opts_parse(sb, data, &opts);
	//if (LktrCond) {au_free_opts(&opts); err = -1;}
	if (unlikely(err))
		goto out_opts;

	sbinfo = stosi(sb);
	root = sb->s_root;
	inode = root->d_inode;
	vfsub_i_lock(inode);
	aufs_write_lock(root);

	//DbgSleep(3);

	/* au_do_opts() may return an error */
	err = au_opts_remount(sb, &opts);
	//if (LktrCond) err = -1;
	au_opts_free(&opts);

	if (opts.refresh_dir || opts.refresh_nondir) {
		int rerr;
		au_gen_t sigen;

		dlgt = au_need_dlgt(sb);
		AuFlagSet(sbinfo, f_dlgt, 0);
		au_sigen_inc(sb);
		au_reset_hinotify(inode, au_hi_flags(inode, /*isdir*/1));
		sigen = au_sigen(sb);
		sbinfo->si_failed_refresh_dirs = 0;

		DiMustNoWaiters(root);
		IiMustNoWaiters(root->d_inode);
		di_write_unlock(root);

		rerr = refresh_dir(root, sigen);
		if (unlikely(rerr)) {
			sbinfo->si_failed_refresh_dirs = 1;
			AuWarn("Refreshing directories failed, ignores (%d)\n",
			       rerr);
		}

		if (unlikely(opts.refresh_nondir)) {
			//au_debug_on();
			rerr = refresh_nondir(root, sigen, !rerr);
			if (unlikely(rerr))
				AuWarn("Refreshing non-directories failed,"
				       " ignores (%d)\n", rerr);
			//au_debug_off();
		}

		/* aufs_write_lock() calls ..._child() */
		di_write_lock_child(root);

		au_cpup_attr_all(inode);
		AuFlagSet(sbinfo, f_dlgt, dlgt);
	}

	aufs_write_unlock(root);
	vfsub_i_unlock(inode);
	if (opts.refresh_dir)
		sysaufs_notify_remount();

 out_opts:
	free_page((unsigned long)opts.opt);
 out:
	err = cvt_err(err);
	AuTraceErr(err);
	//au_debug_off();
	return err;
}

static struct super_operations aufs_sop = {
	.alloc_inode	= aufs_alloc_inode,
	.destroy_inode	= aufs_destroy_inode,
	.read_inode	= aufs_read_inode,
	//.dirty_inode	= aufs_dirty_inode,
	//.write_inode	= aufs_write_inode,
	//void (*put_inode) (struct inode *);
	.drop_inode	= generic_delete_inode,
	//.delete_inode	= aufs_delete_inode,
	//.clear_inode	= aufs_clear_inode,

	.show_options	= aufs_show_options,
	.statfs		= aufs_statfs,

	.put_super	= aufs_put_super,
	//void (*write_super) (struct super_block *);
	//int (*sync_fs)(struct super_block *sb, int wait);
	//void (*write_super_lockfs) (struct super_block *);
	//void (*unlockfs) (struct super_block *);
	.remount_fs	= aufs_remount_fs,
	/* depends upon umount flags. also use put_super() (< 2.6.18) */
	.umount_begin	= aufs_umount_begin
};

/* ---------------------------------------------------------------------- */

/*
 * at first mount time.
 */

static int alloc_sbinfo(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;

	AuTraceEnter();

	sbinfo = kmalloc(sizeof(*sbinfo), GFP_KERNEL);
	//if (LktrCond) {kfree(sbinfo); sbinfo = NULL;}
	if (unlikely(!sbinfo))
		goto out;
	sbinfo->si_branch = kzalloc(sizeof(*sbinfo->si_branch), GFP_KERNEL);
	//if (LktrCond) {kfree(sbinfo->si_branch); sbinfo->si_branch = NULL;}
	if (unlikely(!sbinfo->si_branch))
		goto out_sbinfo;

	rw_init_wlock(&sbinfo->si_rwsem);
	sbinfo->si_generation = 0;
	//sbinfo->si_generation = INT_MAX - 2;
	sbinfo->si_failed_refresh_dirs = 0;
	sbinfo->si_bend = -1;
	sbinfo->si_last_br_id = 0;

	sbinfo->si_wbr_copyup_ops = au_wbr_copyup_ops + AuWbrCopyup_TDP;
	sbinfo->si_wbr_create_ops = au_wbr_create_ops + AuWbrCreate_TDP;

	au_opts_flags_def(&sbinfo->au_si_flags);

	sbinfo->si_xread = NULL;
	sbinfo->si_xwrite = NULL;
	sbinfo->si_xib = NULL;
	mutex_init(&sbinfo->si_xib_mtx);
	sbinfo->si_xib_buf = NULL;
	/* leave si_xib_last_pindex and si_xib_next_bit */

	au_nwt_init(&sbinfo->si_nowait);

	sbinfo->si_rdcache = AUFS_RDCACHE_DEF * HZ;
	sbinfo->si_dirwh = AUFS_DIRWH_DEF;

	spin_lock_init(&sbinfo->si_plink_lock);
	INIT_LIST_HEAD(&sbinfo->si_plink);

	/* leave syaufs members, si_list, si_mnt and si_sysaufs. */

	init_lvma(sbinfo);
	sb->s_fs_info = sbinfo;

#ifdef ForceInotify
	AuFlagSet(sbinfo, f_udba, AuUdba_INOTIFY);
#endif
#ifdef ForceDlgt
	AuFlagSet(sbinfo, f_dlgt, 1);
#endif
#ifdef ForceNoPlink
	AuFlagSet(sbinfo, f_plink, 0);
#endif
#ifdef ForceNoXino
	AuFlagSet(sbinfo, f_xino, AuXino_NONE);
#endif
#ifdef ForceNoRefrof
	AuFlagSet(sbinfo, f_refrof, 0);
#endif
	return 0; /* success */

// out_branch:
	kfree(sbinfo->si_branch);
 out_sbinfo:
	kfree(sbinfo);
 out:
	AuTraceErr(-ENOMEM);
	return -ENOMEM;
}

static int alloc_root(struct super_block *sb)
{
	int err;
	struct inode *inode;
	struct dentry *root;

	AuTraceEnter();

	err = -ENOMEM;
	inode = iget(sb, AUFS_ROOT_INO);
	//if (LktrCond) {iput(inode); inode = NULL;}
	if (unlikely(!inode))
		goto out;
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out;
	err = -ENOMEM;
	if (unlikely(is_bad_inode(inode)))
		goto out_iput;

	inode->i_mode = S_IFDIR;
	root = d_alloc_root(inode);
	//if (LktrCond) {igrab(inode); dput(root); root = NULL;}
	if (unlikely(!root))
		goto out_iput;
	err = PTR_ERR(root);
	if (IS_ERR(root))
		goto out_iput;

	err = au_alloc_dinfo(root);
	//if (LktrCond){rw_write_unlock(&dtodi(root)->di_rwsem);err=-1;}
	if (!err) {
		sb->s_root = root;
		return 0; /* success */
	}
	dput(root);
	goto out; /* do not iput */

 out_iput:
	iput(inode);
 out:
	AuTraceErr(err);
	return err;

}

static int aufs_fill_super(struct super_block *sb, void *raw_data, int silent)
{
	int err;
	struct dentry *root;
	struct inode *inode;
	struct au_opts opts;
	char *arg = raw_data;

	//au_debug_on();
	if (unlikely(!arg || !*arg)) {
		err = -EINVAL;
		AuErr("no arg\n");
		goto out;
	}
	LKTRTrace("%s, silent %d\n", arg, silent);

	err = -ENOMEM;
	memset(&opts, 0, sizeof(opts));
	opts.opt = (void *)__get_free_page(GFP_TEMPORARY);
	//if (LktrCond) {free_page((unsigned long)opts.opt); opts.opt = NULL;}
	if (unlikely(!opts.opt))
		goto out;
	opts.max_opt = PAGE_SIZE / sizeof(*opts.opt);

	err = alloc_sbinfo(sb);
	//if (LktrCond) {si_write_unlock(sb);free_sbinfo(sb);err=-1;}
	if (unlikely(err))
		goto out_opts;
	SiMustWriteLock(sb);
	/* all timestamps always follow the ones on the branch */
	sb->s_flags |= MS_NOATIME | MS_NODIRATIME;
	sb->s_op = &aufs_sop;
	sb->s_magic = AUFS_SUPER_MAGIC;
	au_init_export_op(sb);

	err = alloc_root(sb);
	//if (LktrCond) {rw_write_unlock(&dtodi(sb->s_root)->di_rwsem);
	//dput(sb->s_root);sb->s_root=NULL;err=-1;}
	if (unlikely(err)) {
		AuDebugOn(sb->s_root);
		si_write_unlock(sb);
		goto out_info;
	}
	root = sb->s_root;
	DiMustWriteLock(root);
	inode = root->d_inode;
	inode->i_nlink = 2;

	/*
	 * actually we can parse options regardless aufs lock here.
	 * but at remount time, parsing must be done before aufs lock.
	 * so we follow the same rule.
	 */
	ii_write_lock_parent(inode);
	aufs_write_unlock(root);
	err = au_opts_parse(sb, arg, &opts);
	//if (LktrCond) {au_opts_free(&opts); err = -1;}
	if (unlikely(err))
		goto out_root;

	/* lock vfs_inode first, then aufs. */
	vfsub_i_lock(inode);
	inode->i_op = &aufs_dir_iop;
	inode->i_fop = &aufs_dir_fop;
	aufs_write_lock(root);

	sb->s_maxbytes = 0;
	err = au_opts_mount(sb, &opts);
	//if (LktrCond) err = -1;
	au_opts_free(&opts);
	if (unlikely(err))
		goto out_unlock;
	AuDebugOn(!sb->s_maxbytes);

	//AuDbgDentry(root);
	aufs_write_unlock(root);
	vfsub_i_unlock(inode);
	//AuDbgSb(sb);
	goto out_opts; /* success */

 out_unlock:
	aufs_write_unlock(root);
	vfsub_i_unlock(inode);
 out_root:
	dput(root);
	sb->s_root = NULL;
 out_info:
	free_sbinfo(sb);
	sb->s_fs_info = NULL;
 out_opts:
	free_page((unsigned long)opts.opt);
 out:
	AuTraceErr(err);
	err = cvt_err(err);
	AuTraceErr(err);
	//au_debug_off();
	return err;
}

/* ---------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
static int aufs_get_sb(struct file_system_type *fs_type, int flags,
		       const char *dev_name, void *raw_data,
		       struct vfsmount *mnt)
{
	int err;

	/* all timestamps always follow the ones on the branch */
	/* mnt->mnt_flags |= MNT_NOATIME | MNT_NODIRATIME; */
	err = get_sb_nodev(fs_type, flags, raw_data, aufs_fill_super, mnt);
	if (!err) {
		struct aufs_sbinfo *sbinfo = stosi(mnt->mnt_sb);
		sbinfo->si_mnt = mnt;
		sysaufs_add(sbinfo);
	}
	return err;
}
#else
static struct super_block *aufs_get_sb(struct file_system_type *fs_type,
				       int flags, const char *dev_name,
				       void *raw_data)
{
	return get_sb_nodev(fs_type, flags, raw_data, aufs_fill_super);
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18) */

struct file_system_type aufs_fs_type = {
	.name		= AUFS_FSTYPE,
	.fs_flags	= FS_REVAL_DOT, /* for UDBA and NFS branch */
	.get_sb		= aufs_get_sb,
	.kill_sb	= generic_shutdown_super,
	/* no need to __module_get() and module_put(). */
	.owner		= THIS_MODULE,
};

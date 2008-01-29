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

/* $Id: branch.c,v 1.70 2007/12/03 01:37:41 sfjro Exp $ */

#include <linux/loop.h>
#include <linux/smp_lock.h>
#if defined(CONFIG_ISO9660_FS) || defined(CONFIG_ISO9660_FS_MODULE)
#include <linux/iso_fs.h>
#endif
#if defined(CONFIG_ROMFS_FS) || defined(CONFIG_ROMFS_FS_MODULE)
#include <linux/romfs_fs.h>
#endif
#include "aufs.h"

static void free_branch(struct aufs_branch *br)
{
	AuTraceEnter();

	if (br->br_xino)
		fput(br->br_xino);
	dput(br->br_wh);
	dput(br->br_plink);
	if (!au_test_nfs(br->br_mnt->mnt_sb))
		mntput(br->br_mnt);
	else {
		lockdep_off();
		mntput(br->br_mnt);
		lockdep_on();
	}
	AuDebugOn(br_count(br) || atomic_read(&br->br_wh_running));
	kfree(br);
}

/*
 * frees all branches
 */
void free_branches(struct aufs_sbinfo *sbinfo)
{
	aufs_bindex_t bmax;
	struct aufs_branch **br;

	AuTraceEnter();
	bmax = sbinfo->si_bend + 1;
	br = sbinfo->si_branch;
	while (bmax--)
		free_branch(*br++);
}

/*
 * find the index of a branch which is specified by @br_id.
 */
int find_brindex(struct super_block *sb, aufs_bindex_t br_id)
{
	aufs_bindex_t bindex, bend;

	AuTraceEnter();

	bend = sbend(sb);
	for (bindex = 0; bindex <= bend; bindex++)
		if (sbr_id(sb, bindex) == br_id)
			return bindex;
	return -1;
}

/*
 * test if the @br is readonly or not.
 */
int br_rdonly(struct aufs_branch *br)
{
	return ((br->br_mnt->mnt_sb->s_flags & MS_RDONLY)
		|| !br_writable(br->br_perm))
		? -EROFS : 0;
}

int au_test_def_rr(struct super_block *h_sb)
{
	switch (h_sb->s_magic) {
#ifdef CONFIG_AUFS_RR_SQUASHFS
	case SQUASHFS_MAGIC_LZMA:
	case SQUASHFS_MAGIC:
	case SQUASHFS_MAGIC_LZMA_SWAP:
	case SQUASHFS_MAGIC_SWAP:
		return 1; /* real readonly */
#endif

#if defined(CONFIG_ISO9660_FS) || defined(CONFIG_ISO9660_FS_MODULE)
	case ISOFS_SUPER_MAGIC:
		return 1;
#endif

#if defined(CONFIG_CRAMFS) || defined(CONFIG_CRAMFS_MODULE)
	case CRAMFS_MAGIC:
		return 1;
#endif

#if defined(CONFIG_ROMFS_FS) || defined(CONFIG_ROMFS_FS_MODULE)
	case ROMFS_MAGIC:
		return 1;
#endif

	default:
		return 0;
	}
}

/* ---------------------------------------------------------------------- */

/*
 * test if two hidden_dentries have overlapping branches.
 */
static int do_test_overlap(struct super_block *sb, struct dentry *h_d1,
			   struct dentry *h_d2)
{
	int err;

	LKTRTrace("%.*s, %.*s\n", AuDLNPair(h_d1), AuDLNPair(h_d2));

	err = au_test_subdir(h_d1, h_d2);
	AuTraceErr(err);
	return err;
}

static int test_overlap_loopback(struct super_block *sb, struct dentry *h_d1,
				 struct dentry *h_d2)
{
#if defined(CONFIG_BLK_DEV_LOOP) || defined(CONFIG_BLK_DEV_LOOP_MODULE)
	struct inode *h_inode;
	struct loop_device *l;

	h_inode = h_d1->d_inode;
	if (MAJOR(h_inode->i_sb->s_dev) != LOOP_MAJOR)
		return 0;

	l = h_inode->i_sb->s_bdev->bd_disk->private_data;
	h_d1 = l->lo_backing_file->f_dentry;
	if (unlikely(h_d1->d_sb == sb))
		return 1;
	return do_test_overlap(sb, h_d1, h_d2);
#else
	return 0;
#endif
}

static int test_overlap(struct super_block *sb, struct dentry *h_d1,
			struct dentry *h_d2)
{
	LKTRTrace("d1 %.*s, d2 %.*s\n",
		  AuDLNPair(h_d1), AuDLNPair(h_d2));

	if (unlikely(h_d1 == h_d2))
		return 1;
	return do_test_overlap(sb, h_d1, h_d2)
		|| do_test_overlap(sb, h_d2, h_d1)
		|| test_overlap_loopback(sb, h_d1, h_d2)
		|| test_overlap_loopback(sb, h_d2, h_d1);
}

/* ---------------------------------------------------------------------- */

static int init_br_wh(struct super_block *sb, aufs_bindex_t bindex,
		      struct aufs_branch *br, int new_perm,
		      struct dentry *h_root, struct vfsmount *h_mnt)
{
	int err, old_perm;
	struct inode *dir = sb->s_root->d_inode, *h_dir = h_root->d_inode;
	const int new = (bindex < 0);

	LKTRTrace("b%d, new_perm %d\n", bindex, new_perm);

	if (new)
		vfsub_i_lock_nested(h_dir, AuLsc_I_PARENT);
	else
		hdir_lock(h_dir, dir, bindex);

	br_wh_write_lock(br);
	old_perm = br->br_perm;
	br->br_perm = new_perm;
	err = init_wh(h_root, br, au_do_nfsmnt(h_mnt), sb);
	br->br_perm = old_perm;
	br_wh_write_unlock(br);

	if (new)
		vfsub_i_unlock(h_dir);
	else
		hdir_unlock(h_dir, dir, bindex);

	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

/*
 * returns a newly allocated branch. @new_nbranch is a number of branches
 * after adding a branch.
 */
static struct aufs_branch *alloc_addbr(struct super_block *sb, int new_nbranch)
{
	struct aufs_branch **branchp, *add_branch;
	int sz;
	void *p;
	struct dentry *root;
	struct inode *inode;
	struct aufs_hinode *hinodep;
	struct aufs_hdentry *hdentryp;

	LKTRTrace("new_nbranch %d\n", new_nbranch);
	SiMustWriteLock(sb);
	root = sb->s_root;
	DiMustWriteLock(root);
	inode = root->d_inode;
	IiMustWriteLock(inode);

	add_branch = kmalloc(sizeof(*add_branch), GFP_KERNEL);
	//if (LktrCond) {kfree(add_branch); add_branch = NULL;}
	if (unlikely(!add_branch))
		goto out;

	sz = sizeof(*branchp) * (new_nbranch - 1);
	if (unlikely(!sz))
		sz = sizeof(*branchp);
	p = stosi(sb)->si_branch;
	branchp = au_kzrealloc(p, sz, sizeof(*branchp) * new_nbranch,
			       GFP_KERNEL);
	//if (LktrCond) branchp = NULL;
	if (unlikely(!branchp))
		goto out;
	stosi(sb)->si_branch = branchp;

	sz = sizeof(*hdentryp) * (new_nbranch - 1);
	if (unlikely(!sz))
		sz = sizeof(*hdentryp);
	p = dtodi(root)->di_hdentry;
	hdentryp = au_kzrealloc(p, sz, sizeof(*hdentryp) * new_nbranch,
				GFP_KERNEL);
	//if (LktrCond) hdentryp = NULL;
	if (unlikely(!hdentryp))
		goto out;
	dtodi(root)->di_hdentry = hdentryp;

	sz = sizeof(*hinodep) * (new_nbranch - 1);
	if (unlikely(!sz))
		sz = sizeof(*hinodep);
	p = itoii(inode)->ii_hinode;
	hinodep = au_kzrealloc(p, sz, sizeof(*hinodep) * new_nbranch,
			       GFP_KERNEL);
	//if (LktrCond) hinodep = NULL; // unavailable test
	if (unlikely(!hinodep))
		goto out;
	itoii(inode)->ii_hinode = hinodep;
	return add_branch; /* success */

 out:
	kfree(add_branch);
	AuTraceErr(-ENOMEM);
	return ERR_PTR(-ENOMEM);
}

/*
 * test if the branch permission is legal or not.
 */
static int test_br(struct super_block *sb, struct inode *inode, int brperm,
		   char *path)
{
	int err;

	err = 0;
	if (unlikely(br_writable(brperm) && IS_RDONLY(inode))) {
		AuErr("write permission for readonly fs or inode, %s\n", path);
		err = -EINVAL;
	}

	AuTraceErr(err);
	return err;
}

/*
 * retunrs:
 * 0: success, the caller will add it
 * plus: success, it is already unified, the caller should ignore it
 * minus: error
 */
static int test_add(struct super_block *sb, struct au_opt_add *add, int remount)
{
	int err;
	struct dentry *root;
	struct inode *inode, *hidden_inode;
	aufs_bindex_t bend, bindex;

	LKTRTrace("%s, remo%d\n", add->path, remount);

	root = sb->s_root;
	bend = sbend(sb);
	if (unlikely(bend >= 0 && au_find_dbindex(root, add->nd.dentry) >= 0)) {
		err = 1;
		if (!remount) {
			err = -EINVAL;
			AuErr("%s duplicated\n", add->path);
		}
		goto out;
	}

	err = -ENOSPC; //-E2BIG;
	//if (LktrCond) bend = AUFS_BRANCH_MAX;
	if (unlikely(AUFS_BRANCH_MAX <= add->bindex
		     || AUFS_BRANCH_MAX - 1 <= bend)) {
		AuErr("number of branches exceeded %s\n", add->path);
		goto out;
	}

	err = -EDOM;
	if (unlikely(add->bindex < 0 || bend + 1 < add->bindex)) {
		AuErr("bad index %d\n", add->bindex);
		goto out;
	}

	inode = add->nd.dentry->d_inode;
	AuDebugOn(!inode || !S_ISDIR(inode->i_mode));
	err = -ENOENT;
	if (unlikely(!inode->i_nlink)) {
		AuErr("no existence %s\n", add->path);
		goto out;
	}

	err = -EINVAL;
	if (unlikely(inode->i_sb == sb)) {
		AuErr("%s must be outside\n", add->path);
		goto out;
	}

#ifndef CONFIG_AUFS_ROBR
	if (unlikely(au_test_aufs(inode->i_sb)
		     || !strcmp(au_sbtype(inode->i_sb), "unionfs"))) {
		AuErr("nested " AUFS_NAME " %s\n", add->path);
		goto out;
	}
#endif

	if (unlikely(au_test_nfs(inode->i_sb))) {
#ifdef AuNoNfsBranch
		AuErr(AuNoNfsBranchMsg ". %s\n", add->path);
		goto out;
#endif
#if 0 //LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
#define Msg "nfs4 brach is supported in linux-2.6.23 and later"
		if (unlikely(!strcmp(au_sbtype(inode->i_sb), "nfs4"))) {
			AuErr(Msg ". %s\n", add->path);
			goto out;
		}
#undef Msg
#endif
	}

	err = test_br(sb, add->nd.dentry->d_inode, add->perm, add->path);
	if (unlikely(err))
		goto out;

	if (unlikely(bend < 0))
		return 0; /* success */

	hidden_inode = au_h_dptr(root)->d_inode;
	if (unlikely(AuFlag(stosi(sb), f_warn_perm)
		     && ((hidden_inode->i_mode & S_IALLUGO)
			 != (inode->i_mode & S_IALLUGO)
			 || hidden_inode->i_uid != inode->i_uid
			 || hidden_inode->i_gid != inode->i_gid)))
		AuWarn("uid/gid/perm %s %u/%u/0%o, %u/%u/0%o\n",
		       add->path,
		       inode->i_uid, inode->i_gid, (inode->i_mode & S_IALLUGO),
		       hidden_inode->i_uid, hidden_inode->i_gid,
		       (hidden_inode->i_mode & S_IALLUGO));

	err = -EINVAL;
	for (bindex = 0; bindex <= bend; bindex++)
		if (unlikely(test_overlap(sb, add->nd.dentry,
					  au_h_dptr_i(root, bindex)))) {
			AuErr("%s is overlapped\n", add->path);
			goto out;
		}
	err = 0;

 out:
	AuTraceErr(err);
	return err;
}

int br_add(struct super_block *sb, struct au_opt_add *add, int remount)
{
	int err, sz;
	aufs_bindex_t bend, add_bindex;
	struct dentry *root;
	struct aufs_iinfo *iinfo;
	struct aufs_sbinfo *sbinfo;
	struct aufs_dinfo *dinfo;
	struct inode *root_inode;
	unsigned long long maxb;
	struct aufs_branch **branchp, *add_branch;
	struct aufs_hdentry *hdentryp;
	struct aufs_hinode *hinodep;

	LKTRTrace("b%d, %s, 0x%x, %.*s\n", add->bindex, add->path,
		  add->perm, AuDLNPair(add->nd.dentry));
	SiMustWriteLock(sb);
	root = sb->s_root;
	DiMustWriteLock(root);
	root_inode = root->d_inode;
	IMustLock(root_inode);
	IiMustWriteLock(root_inode);

	err = test_add(sb, add, remount);
	if (unlikely(err < 0))
		goto out;
	if (unlikely(err))
		return 0; /* success */

	bend = sbend(sb);
	add_branch = alloc_addbr(sb, bend + 2);
	err = PTR_ERR(add_branch);
	if (IS_ERR(add_branch))
		goto out;

	err = 0;
	rw_init_nolock(&add_branch->br_wh_rwsem);
	add_branch->br_plink = NULL;
	add_branch->br_wh = NULL;
	if (unlikely(br_writable(add->perm))) {
		err = init_br_wh(sb, /*bindex*/-1, add_branch, add->perm,
				 add->nd.dentry, add->nd.mnt);
		if (unlikely(err)) {
			kfree(add_branch);
			goto out;
		}
	}
	add_branch->br_xino = NULL;
	add_branch->br_mnt = mntget(add->nd.mnt);
	atomic_set(&add_branch->br_wh_running, 0);
	add_branch->br_id = new_br_id(sb);
	add_branch->br_perm = add->perm;
	atomic_set(&add_branch->br_count, 0);
	add_branch->br_bytes = 0;
	add_branch->br_xino_upper = AUFS_XINO_TRUNC_INIT;
	atomic_set(&add_branch->br_xino_running, 0);
	add_branch->br_generation = au_sigen(sb);
	//smp_mb(); /* atomic_set */

	sbinfo = stosi(sb);
	dinfo = dtodi(root);
	iinfo = itoii(root_inode);

	add_bindex = add->bindex;
	sz = sizeof(*(sbinfo->si_branch)) * (bend + 1 - add_bindex);
	branchp = sbinfo->si_branch + add_bindex;
	memmove(branchp + 1, branchp, sz);
	*branchp = add_branch;
	sz = sizeof(*hdentryp) * (bend + 1 - add_bindex);
	hdentryp = dinfo->di_hdentry + add_bindex;
	memmove(hdentryp + 1, hdentryp, sz);
	au_h_dentry_init(hdentryp);
	sz = sizeof(*hinodep) * (bend + 1 - add_bindex);
	hinodep = iinfo->ii_hinode + add_bindex;
	memmove(hinodep + 1, hinodep, sz);
	hinodep->hi_inode = NULL;
	au_hin_init(hinodep, NULL);

	sbinfo->si_bend++;
	dinfo->di_bend++;
	iinfo->ii_bend++;
	if (unlikely(bend < 0)) {
		sbinfo->si_bend = 0;
		dinfo->di_bstart = 0;
		iinfo->ii_bstart = 0;
	}
	set_h_dptr(root, add_bindex, dget(add->nd.dentry));
	set_h_iptr(root_inode, add_bindex, igrab(add->nd.dentry->d_inode), 0);
	if (!add_bindex)
		au_cpup_attr_all(root_inode);
	else
		au_add_nlink(root_inode, add->nd.dentry->d_inode);
	maxb = add->nd.dentry->d_sb->s_maxbytes;
	if (sb->s_maxbytes < maxb)
		sb->s_maxbytes = maxb;

	if (AuFlag(sbinfo, f_xino) != AuXino_NONE) {
		struct file *base_file = stobr(sb, 0)->br_xino;
		if (!add_bindex)
			base_file = stobr(sb, 1)->br_xino;
		err = xino_br(sb, add_bindex, base_file, /*do_test*/1);
		if (unlikely(err)) {
			AuDebugOn(add_branch->br_xino);
			/* bad action? */
			AuIOErr("err %d, force noxino\n", err);
			err = -EIO;
			xino_clr(sb);
		}
	}

 out:
	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

#define Verbose(sb, fmt, args...) do { \
	if (unlikely(AuFlag(stosi(sb), f_verbose))) \
		AuInfo(fmt, ##args); \
	else \
		LKTRTrace(fmt, ##args); \
} while (0)

/*
 * test if the branch is deletable or not.
 */
static int test_dentry_busy(struct dentry *root, aufs_bindex_t bindex,
			    au_gen_t sigen)
{
	int err, i, j, ndentry;
	struct au_dcsub_pages dpages;
	struct au_dpage *dpage;
	struct dentry *d;
	aufs_bindex_t bstart, bend;

	LKTRTrace("b%d, gen%d\n", bindex, sigen);
	SiMustWriteLock(root->d_sb);

	err = au_dpages_init(&dpages, GFP_TEMPORARY);
	if (unlikely(err))
		goto out;
	err = au_dcsub_pages(&dpages, root, NULL, NULL);
	if (unlikely(err))
		goto out_dpages;

	for (i = 0; !err && i < dpages.ndpage; i++) {
		dpage = dpages.dpages + i;
		ndentry = dpage->ndentry;
		for (j = 0; !err && j < ndentry; j++) {
			d = dpage->dentries[j];
			AuDebugOn(!atomic_read(&d->d_count));
			if (au_digen(d) == sigen)
				di_read_lock_child(d, AuLock_IR);
			else {
				di_write_lock_child(d);
				err = au_reval_dpath(d, sigen);
				if (!err)
					di_downgrade_lock(d, AuLock_IR);
				else {
					di_write_unlock(d);
					break;
				}
			}

			bstart = dbstart(d);
			bend = dbend(d);
			if (bstart <= bindex
			    && bindex <= bend
			    && au_h_dptr_i(d, bindex)
			    && (!S_ISDIR(d->d_inode->i_mode)
				|| bstart == bend)) {
				err = -EBUSY;
				Verbose(root->d_sb, "busy %.*s\n",
					AuDLNPair(d));
			}
			di_read_unlock(d, AuLock_IR);
		}
	}

 out_dpages:
	au_dpages_free(&dpages);
 out:
	AuTraceErr(err);
	return err;
}

static int test_inode_busy(struct super_block *sb, aufs_bindex_t bindex,
			   au_gen_t sigen)
{
	int err;
	struct inode *i;
	aufs_bindex_t bstart, bend;

	LKTRTrace("b%d, gen%d\n", bindex, sigen);
	SiMustWriteLock(sb);

	err = 0;
	list_for_each_entry(i, &sb->s_inodes, i_sb_list) {
		AuDebugOn(!atomic_read(&i->i_count));
		if (!list_empty(&i->i_dentry))
			continue;

		if (au_iigen(i) == sigen)
			ii_read_lock_child(i);
		else {
			ii_write_lock_child(i);
			err = au_refresh_hinode_self(i);
			if (!err)
				ii_downgrade_lock(i);
			else {
				ii_write_unlock(i);
				break;
			}
		}

		bstart = ibstart(i);
		bend = ibend(i);
		if (bstart <= bindex
		    && bindex <= bend
		    && au_h_iptr_i(i, bindex)
		    && (!S_ISDIR(i->i_mode) || bstart == bend)) {
			err = -EBUSY;
			Verbose(sb, "busy i%lu\n", i->i_ino);
			//au_debug_on();
			//DbgInode(i);
			//au_debug_off();
			ii_read_unlock(i);
			break;
		}
		ii_read_unlock(i);
	}

	AuTraceErr(err);
	return err;
}

static int test_children_busy(struct dentry *root, aufs_bindex_t bindex)
{
	int err;
	au_gen_t sigen;

	LKTRTrace("b%d\n", bindex);
	SiMustWriteLock(root->d_sb);
	DiMustWriteLock(root);
	AuDebugOn(!kernel_locked());

	sigen = au_sigen(root->d_sb);
	DiMustNoWaiters(root);
	IiMustNoWaiters(root->d_inode);
	di_write_unlock(root);
	err = test_dentry_busy(root, bindex, sigen);
	if (!err)
		err = test_inode_busy(root->d_sb, bindex, sigen);
	di_write_lock_child(root); /* aufs_write_lock() calls ..._child() */

	AuTraceErr(err);
	return err;
}

int br_del(struct super_block *sb, struct au_opt_del *del, int remount)
{
	int err, do_wh, rerr;
	struct dentry *root;
	struct inode *inode, *hidden_dir;
	aufs_bindex_t bindex, bend, br_id;
	struct aufs_sbinfo *sbinfo;
	struct aufs_dinfo *dinfo;
	struct aufs_iinfo *iinfo;
	struct aufs_branch *br;

	//au_debug_on();
	LKTRTrace("%s, %.*s\n", del->path, AuDLNPair(del->h_root));
	SiMustWriteLock(sb);
	root = sb->s_root;
	DiMustWriteLock(root);
	inode = root->d_inode;
	IiMustWriteLock(inode);

	err = 0;
	bindex = au_find_dbindex(root, del->h_root);
	if (unlikely(bindex < 0)) {
		if (remount)
			goto out; /* success */
		err = -ENOENT;
		AuErr("%s no such branch\n", del->path);
		goto out;
	}
	LKTRTrace("bindex b%d\n", bindex);

	err = -EBUSY;
	bend = sbend(sb);
	if (unlikely(!bend)) {
		Verbose(sb, "no more branches left\n");
		goto out;
	}
	br = stobr(sb, bindex);
	if (unlikely(br_count(br))) {
		Verbose(sb, "%d file(s) opened\n", br_count(br));
		goto out;
	}

	do_wh = 0;
	hidden_dir = del->h_root->d_inode;
	if (unlikely(br->br_wh || br->br_plink)) {
#if 0 // rfu
		/* remove whiteout base */
		err = init_br_wh(sb, bindex, br, AuBr_RO, del->h_root,
				 br->br_mnt);
		if (unlikely(err))
			goto out;
#else
		dput(br->br_wh);
		dput(br->br_plink);
		br->br_plink = NULL;
		br->br_wh = NULL;
#endif
		do_wh = 1;
	}

	err = test_children_busy(root, bindex);
	if (unlikely(err)) {
		if (unlikely(do_wh))
			goto out_wh;
		goto out;
	}

	err = 0;
	sbinfo = stosi(sb);
	dinfo = dtodi(root);
	iinfo = itoii(inode);

	dput(au_h_dptr_i(root, bindex));
	aufs_hiput(iinfo->ii_hinode + bindex);
	br_id = br->br_id;
	free_branch(br);

	//todo: realloc and shrink memeory
	if (bindex < bend) {
		const aufs_bindex_t n = bend - bindex;
		struct aufs_branch **brp;
		struct aufs_hdentry *hdp;
		struct aufs_hinode *hip;

		brp = sbinfo->si_branch + bindex;
		memmove(brp, brp + 1, sizeof(*brp) * n);
		hdp = dinfo->di_hdentry + bindex;
		memmove(hdp, hdp + 1, sizeof(*hdp) * n);
		hip = iinfo->ii_hinode + bindex;
		memmove(hip, hip + 1, sizeof(*hip) * n);
	}
	sbinfo->si_branch[0 + bend] = NULL;
	dinfo->di_hdentry[0 + bend].hd_dentry = NULL;
	iinfo->ii_hinode[0 + bend].hi_inode = NULL;
	au_hin_init(iinfo->ii_hinode + bend, NULL);

	sbinfo->si_bend--;
	dinfo->di_bend--;
	iinfo->ii_bend--;
	if (!bindex)
		au_cpup_attr_all(inode);
	else
		au_sub_nlink(inode, del->h_root->d_inode);
	if (AuFlag(sbinfo, f_plink))
		au_half_refresh_plink(sb, br_id);

	if (sb->s_maxbytes == del->h_root->d_sb->s_maxbytes) {
		bend--;
		sb->s_maxbytes = 0;
		for (bindex = 0; bindex <= bend; bindex++) {
			unsigned long long maxb;
			maxb = sbr_sb(sb, bindex)->s_maxbytes;
			if (sb->s_maxbytes < maxb)
				sb->s_maxbytes = maxb;
		}
	}
	goto out; /* success */

 out_wh:
	/* revert */
	rerr = init_br_wh(sb, bindex, br, br->br_perm, del->h_root, br->br_mnt);
	if (rerr)
		AuWarn("failed re-creating base whiteout, %s. (%d)\n",
		       del->path, rerr);
 out:
	AuTraceErr(err);
	//au_debug_off();
	return err;
}

static int do_need_sigen_inc(int a, int b)
{
	return (br_whable(a) && !br_whable(b));
}

static int need_sigen_inc(int old, int new)
{
	return (do_need_sigen_inc(old, new)
		|| do_need_sigen_inc(new, old));
}

int br_mod(struct super_block *sb, struct au_opt_mod *mod, int remount,
	   int *do_update)
{
	int err;
	struct dentry *root;
	aufs_bindex_t bindex;
	struct aufs_branch *br;
	struct inode *hidden_dir;

	LKTRTrace("%s, %.*s, 0x%x\n",
		  mod->path, AuDLNPair(mod->h_root), mod->perm);
	SiMustWriteLock(sb);
	root = sb->s_root;
	DiMustWriteLock(root);
	IiMustWriteLock(root->d_inode);

	bindex = au_find_dbindex(root, mod->h_root);
	if (unlikely(bindex < 0)) {
		if (remount)
			return 0; /* success */
		err = -ENOENT;
		AuErr("%s no such branch\n", mod->path);
		goto out;
	}
	LKTRTrace("bindex b%d\n", bindex);

	hidden_dir = mod->h_root->d_inode;
	err = test_br(sb, hidden_dir, mod->perm, mod->path);
	if (unlikely(err))
		goto out;

	br = stobr(sb, bindex);
	if (unlikely(br->br_perm == mod->perm))
		return 0; /* success */

	if (br_writable(br->br_perm)) {
#if 1
		/* remove whiteout base */
		//todo: mod->perm?
		err = init_br_wh(sb, bindex, br, AuBr_RO, mod->h_root,
				 br->br_mnt);
		if (unlikely(err))
			goto out;
#else
		dput(br->br_wh);
		dput(br->br_plink);
		br->br_plink = NULL;
		br->br_wh = NULL;
#endif

		if (!br_writable(mod->perm)) {
			/* rw --> ro, file might be mmapped */
			struct file *file, *hf;

#if 1 // test here
			DiMustNoWaiters(root);
			IiMustNoWaiters(root->d_inode);
			di_write_unlock(root);

			/*
			 * no need file_list_lock()
			 * since BKL (and sbinfo) is locked
			 */
			AuDebugOn(!kernel_locked());
			list_for_each_entry(file, &sb->s_files, f_u.fu_list) {
				LKTRTrace("%.*s\n", AuDLNPair(file->f_dentry));
				if (unlikely(!au_test_aufs_file(file)))
					continue;

				fi_read_lock(file);
				if (!S_ISREG(file->f_dentry->d_inode->i_mode)
				    || !(file->f_mode & FMODE_WRITE)
				    || fbstart(file) != bindex) {
					FiMustNoWaiters(file);
					fi_read_unlock(file);
					continue;
				}

				// todo: already flushed?
				hf = au_h_fptr(file);
				hf->f_flags = au_file_roflags(hf->f_flags);
				hf->f_mode &= ~FMODE_WRITE;
				put_write_access(hf->f_dentry->d_inode);
				FiMustNoWaiters(file);
				fi_read_unlock(file);
			}

			/* aufs_write_lock() calls ..._child() */
			di_write_lock_child(root);
#endif
		}
	}

	*do_update |= need_sigen_inc(br->br_perm, mod->perm);
	br->br_perm = mod->perm;

 out:
	AuTraceErr(err);
	return err;
}

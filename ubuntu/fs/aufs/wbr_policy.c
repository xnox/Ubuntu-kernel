/*
 * Copyright (C) 2007 Junjiro Okajima
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

/* $Id: wbr_policy.c,v 1.5 2007/11/26 01:34:50 sfjro Exp $ */

#include <linux/statfs.h>
#include "aufs.h"

static int au_cpdown_attr(struct dentry *h_dst, struct dentry *h_src, int dlgt)
{
	int err, sbits;
	struct iattr ia;
	struct inode *h_idst, *h_isrc;
	struct vfsub_args vargs;

	LKTRTrace("%.*s\n", AuDLNPair(h_dst));
	h_idst = h_dst->d_inode;
	//IMustLock(h_idst);
	h_isrc = h_src->d_inode;
	//IMustLock(h_isrc);

	ia.ia_valid = ATTR_FORCE | ATTR_MODE | ATTR_UID | ATTR_GID;
	ia.ia_mode = h_isrc->i_mode;
	ia.ia_uid = h_isrc->i_uid;
	ia.ia_gid = h_isrc->i_gid;
	sbits = !!(ia.ia_mode & (S_ISUID | S_ISGID));

	vfsub_args_init(&vargs, NULL, dlgt, /*force_unlink*/0);
	err = vfsub_notify_change(h_dst, &ia, &vargs);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	/* is this nfs only? */
	if (!err && sbits && au_test_nfs(h_dst->d_sb)) {
		ia.ia_valid = ATTR_FORCE | ATTR_MODE;
		ia.ia_mode = h_isrc->i_mode;
		err = vfsub_notify_change(h_dst, &ia, &vargs);
	}
#endif
	if (!err)
		h_idst->i_flags = h_isrc->i_flags; //??

	AuTraceErr(err);
	return err;
}

struct au_cpdown_dir_args {
	struct dentry *parent;
	unsigned int parent_opq:1;
};

static int au_cpdown_dir(struct dentry *dentry, aufs_bindex_t bdst,
			 struct dentry *h_parent, void *arg)
{
	int err, parent_opq, whed, dlgt, do_opq, made_dir, diropq, rerr;
	struct au_cpdown_dir_args *args = arg;
	aufs_bindex_t bend, bopq;
	struct dentry *h_dentry, *opq_dentry, *wh_dentry;
	struct inode *h_dir, *h_inode, *inode;

	LKTRTrace("%.*s, b%d\n", AuDLNPair(dentry), bdst);
	AuDebugOn(dbstart(dentry) <= bdst
		  && bdst <= dbend(dentry)
		  && au_h_dptr_i(dentry, bdst));
	AuDebugOn(!h_parent);
	h_dir = h_parent->d_inode;
	AuDebugOn(!h_dir);
	IMustLock(h_dir);

	err = au_lkup_neg(dentry, bdst);
	if (unlikely(err < 0))
		goto out;
	h_dentry = au_h_dptr_i(dentry, bdst);
	dlgt = au_need_dlgt(dentry->d_sb);
	err = vfsub_sio_mkdir(h_dir, h_dentry, 0755, dlgt);
	if (unlikely(err))
		goto out_put;

	made_dir = 1;
	bend = dbend(dentry);
	bopq = dbdiropq(dentry);
	whed = (dbwh(dentry) == bdst);
	if (!args->parent_opq)
		args->parent_opq |= (bopq <= bdst);
	parent_opq = (args->parent_opq && args->parent == dentry);
	do_opq = 0;
	diropq = 0;
	h_inode = h_dentry->d_inode;
	vfsub_i_lock_nested(h_inode, AuLsc_I_CHILD);
	if (whed || (parent_opq && do_opq)) {
		opq_dentry = create_diropq(dentry, bdst, dlgt);
		err = PTR_ERR(opq_dentry);
		if (IS_ERR(opq_dentry)) {
			vfsub_i_unlock(h_inode);
			goto out_dir;
		}
		dput(opq_dentry);
		diropq = 1;
	}

	err = au_cpdown_attr(h_dentry, au_h_dptr(dentry), dlgt);
	vfsub_i_unlock(h_inode);
	if (unlikely(err))
		goto out_opq;

	wh_dentry = NULL;
	if (whed) {
		wh_dentry = lkup_wh(h_parent, &dentry->d_name, /*ndx*/NULL);
		err = PTR_ERR(wh_dentry);
		if (IS_ERR(wh_dentry))
			goto out_opq;
		err = 0;
		if (wh_dentry->d_inode)
			err = au_unlink_wh_dentry(h_dir, wh_dentry, dentry,
						  NULL, dlgt);
		dput(wh_dentry);
		if (unlikely(err))
			goto out_opq;
	}

	inode = dentry->d_inode;
	if (ibend(inode) < bdst)
		set_ibend(inode, bdst);
	set_h_iptr(inode, bdst, igrab(h_inode), au_hi_flags(inode, 1));
	goto out; /* success */

	/* revert */
 out_opq:
	if (diropq) {
		vfsub_i_lock_nested(h_inode, AuLsc_I_CHILD);
		rerr = remove_diropq(dentry, bdst, dlgt);
		vfsub_i_unlock(h_inode);
		if (unlikely(rerr)) {
			AuIOErr("failed removing diropq for %.*s b%d (%d)\n",
				AuDLNPair(dentry), bdst, rerr);
			err = -EIO;
			goto out;
		}
	}
 out_dir:
	if (made_dir) {
		rerr = vfsub_sio_rmdir(h_dir, h_dentry, dlgt);
		if (unlikely(rerr)) {
			AuIOErr("failed removing %.*s b%d (%d)\n",
				AuDLNPair(dentry), bdst, rerr);
			err = -EIO;
		}
	}
 out_put:
	set_h_dptr(dentry, bdst, NULL);
	if (dbend(dentry) == bdst)
		au_update_dbend(dentry);
 out:
	AuTraceErr(err);
	return err;
}

int au_cpdown_dirs(struct dentry *dentry, aufs_bindex_t bdst,
		   struct dentry *locked)
{
	int err;
	struct au_cpdown_dir_args args = {
		.parent		= dget_parent(dentry),
		.parent_opq	= 0
	};

	LKTRTrace("%.*s, b%d\n", AuDLNPair(dentry), bdst);

	err = au_cp_dirs(dentry, bdst, locked, au_cpdown_dir, &args);
	dput(args.parent);

	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

#if 0
/*
 * returns writable branch index, otherwise an error.
 * todo: customizable writable-branch-policy
 */
static int find_rw_parent(struct dentry *dentry, aufs_bindex_t bend)
{
	int err;
	aufs_bindex_t bindex, candidate;
	struct super_block *sb;
	struct dentry *parent, *hidden_parent;

	err = bend;
	sb = dentry->d_sb;
	parent = dget_parent(dentry);
#if 1 // branch policy
	hidden_parent = au_h_dptr_i(parent, bend);
	if (hidden_parent && !br_rdonly(stobr(sb, bend)))
		goto out; /* success */
#endif

	candidate = -1;
	for (bindex = dbstart(parent); bindex <= bend; bindex++) {
		hidden_parent = au_h_dptr_i(parent, bindex);
		if (hidden_parent && !br_rdonly(stobr(sb, bindex))) {
#if 0 // branch policy
			if (candidate == -1)
				candidate = bindex;
			if (!au_test_perm(hidden_parent->d_inode, MAY_WRITE))
				return bindex;
#endif
			err = bindex;
			goto out; /* success */
		}
	}
#if 0 // branch policy
	err = candidate;
	if (candidate != -1)
		goto out; /* success */
#endif
	err = -EROFS;

 out:
	dput(parent);
	return err;
}

int find_rw_br(struct super_block *sb, aufs_bindex_t bend)
{
	aufs_bindex_t bindex;

	for (bindex = bend; bindex >= 0; bindex--)
		if (!br_rdonly(stobr(sb, bindex)))
			return bindex;
	return -EROFS;
}

int find_rw_parent_br(struct dentry *dentry, aufs_bindex_t bend)
{
	int err;

	err = find_rw_parent(dentry, bend);
	if (err >= 0)
		return err;
	return find_rw_br(dentry->d_sb, bend);
}

#if 0 // branch policy
/*
 * dir_cpdown/nodir_cpdown(def)
 * wr_br_policy=dir | branch
 */
int au_rw(struct dentry *dentry, aufs_bindex_t bend)
{
	int err;
	struct super_block *sb;

	sb = dentry->d_sb;
	SiMustAnyLock(sb);

	if (!au_flag_test(sb, AuFlag_DIR_CPDOWN)) {
		dpages;
	}
}
#endif
#endif

/* ---------------------------------------------------------------------- */

/* policies for create */

static int au_wbr_bu(struct super_block *sb, aufs_bindex_t bindex)
{
	for (; bindex >= 0; bindex--)
		if (!br_rdonly(stobr(sb, bindex)))
			return bindex;
	return -EROFS;
}

/* top down parent */
static int au_wbr_create_tdp(struct dentry *dentry, int isdir)
{
	int err;
	struct super_block *sb;
	aufs_bindex_t bstart, bindex;
	struct dentry *parent, *h_parent;

	LKTRTrace("%.*s, dir %d\n", AuDLNPair(dentry), isdir);

	sb = dentry->d_sb;
	bstart = dbstart(dentry);
	err = bstart;
	if (!br_rdonly(stobr(sb, bstart)))
		goto out;

	err = -EROFS;
	parent = dget_parent(dentry);
	for (bindex = dbstart(parent); bindex < bstart; bindex++) {
		h_parent = au_h_dptr_i(parent, bindex);
		if (h_parent && !br_rdonly(stobr(sb, bindex))) {
			err = bindex;
			break;
		}
	}
	dput(parent);

	/* bottom up here */
	if (unlikely(err < 0))
		err = au_wbr_bu(sb, bstart - 1);

 out:
	LKTRTrace("b%d\n", err);
	return err;
}

/* ---------------------------------------------------------------------- */

/* an exception for the policy other than tdp */
static int au_wbr_create_exp(struct dentry *dentry)
{
	int err;
	struct dentry *parent;
	aufs_bindex_t bwh, bdiropq;

	LKTRTrace("%.*s\n", AuDLNPair(dentry));

	err = -1;
	bwh = dbwh(dentry);
	parent = dget_parent(dentry);
	bdiropq = dbdiropq(parent);
	if (bwh >= 0) {
		if (bdiropq >= 0)
			err = min(bdiropq, bwh);
		else
			err = bwh;
		LKTRTrace("%d\n", err);
	} else if (bdiropq >= 0) {
		err = bdiropq;
		LKTRTrace("%d\n", err);
	}
	dput(parent);

	if (err >= 0 && br_rdonly(stobr(dentry->d_sb, err)))
		err = -1;

	LKTRTrace("%d\n", err);
	return err;
}

/* ---------------------------------------------------------------------- */

/* round robin */
static int au_wbr_create_init_rr(struct super_block *sb)
{
	int err;

	err = au_wbr_bu(sb, sbend(sb));
	atomic_set(&stosi(sb)->si_wbr_rr_next, -err); /* less important */

	LKTRTrace("b%d\n", err);
	return err;
}

static int au_wbr_create_rr(struct dentry *dentry, int isdir)
{
	int err, nbr;
	struct super_block *sb;
	atomic_t *next;
	unsigned int u;
	aufs_bindex_t bindex, bend;

	//au_debug_on();
	LKTRTrace("%.*s, dir %d\n", AuDLNPair(dentry), isdir);

	sb = dentry->d_sb;
	next = NULL;
	err = au_wbr_create_exp(dentry);
	if (err >= 0)
		goto out;

	next = &stosi(sb)->si_wbr_rr_next;
	bend = sbend(sb);
	nbr =  bend + 1;
	for (bindex = 0; bindex <= bend; bindex++) {
		if (!isdir) {
			err = atomic_dec_return(next) + 1;
			/* modulo for 0 is meaningless */
			if (unlikely(!err))
				err = atomic_dec_return(next) + 1;
		} else
			err = atomic_read(next);
		LKTRTrace("%d\n", err);
		u = err;
		err = u % nbr;
		LKTRTrace("%d\n", err);
		if (!br_rdonly(stobr(sb, err)))
			break;
		err = -EROFS;
	}

 out:
	LKTRTrace("%d\n", err);
	//au_debug_off();
	return err;
}

/* ---------------------------------------------------------------------- */

/* most free space */
static void *au_wbr_statfs_arg(struct aufs_branch *br, struct super_block *sb,
			       aufs_bindex_t bindex)
{
	struct super_block *h_sb;

	h_sb = br->br_mnt->mnt_sb;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
	return h_sb;
#else
	if (!au_test_nfs(h_sb))
		return h_sb->s_root;

	/* sigh,,, why nfs s_root has wrong inode? */
	return dtodi(sb->s_root)->di_hdentry[0 + bindex].hd_dentry;
#endif
}

static void au_mfs(struct dentry *dentry)
{
	struct super_block *sb;
	aufs_bindex_t bindex, bend;
	int dlgt, err;
	struct kstatfs st;
	u64 b, bavail;
	void *arg;
	struct aufs_branch *br;
	struct au_wbr_mfs *mfs;

	LKTRTrace("%.*s\n", AuDLNPair(dentry));

	bavail = 0;
	sb = dentry->d_sb;
	mfs = &stosi(sb)->si_wbr_mfs;
	mfs->mfs_bindex = -EROFS;
	mfs->mfsrr_bytes = 0;
	dlgt = au_need_dlgt(sb);
	bend = sbend(sb);
	for (bindex = 0; bindex <= bend; bindex++) {
		br = stobr(sb, bindex);
		if (br_rdonly(br))
			continue;
		arg = au_wbr_statfs_arg(br, sb, bindex);
		if (!arg)
			continue;

		err = vfsub_statfs(arg, &st, dlgt);
		LKTRTrace("b%d, %d, %Lu\n",
			  bindex, err, (unsigned long long)st.f_bavail);
		if (unlikely(err)) {
			AuWarn1("failed statfs, b%d, %d\n", bindex, err);
			continue;
		}

		/* when the available size is equal, select lower one */
		b = st.f_bavail * st.f_bsize;
		br->br_bytes = b;
		if (b >= bavail) {
			bavail = b;
			mfs->mfs_bindex = bindex;
			mfs->mfs_jiffy = jiffies;
		}
	}

	mfs->mfsrr_bytes = bavail;
	LKTRTrace("b%d\n", mfs->mfs_bindex);
}

static int au_wbr_create_mfs(struct dentry *dentry, int isdir)
{
	int err;
	struct super_block *sb;
	struct au_wbr_mfs *mfs;

	//au_debug_on();
	LKTRTrace("%.*s\n", AuDLNPair(dentry));

	sb = dentry->d_sb;
	err = au_wbr_create_exp(dentry);
	if (err >= 0)
		goto out;

	mfs = &stosi(sb)->si_wbr_mfs;
	mutex_lock(&mfs->mfs_lock);
	if (unlikely(time_after(jiffies, mfs->mfs_jiffy + mfs->mfs_expire)
		     || mfs->mfs_bindex < 0
		     || br_rdonly(stobr(sb, mfs->mfs_bindex))))
		au_mfs(dentry);
	mutex_unlock(&mfs->mfs_lock);
	err = mfs->mfs_bindex;

 out:
	LKTRTrace("b%d\n", err);
	//au_debug_off();
	return err;
}

static int au_wbr_create_init_mfs(struct super_block *sb)
{
	struct au_wbr_mfs *mfs;

	mfs = &stosi(sb)->si_wbr_mfs;
	LKTRTrace("expire %lu\n", mfs->mfs_expire);

	mutex_init(&mfs->mfs_lock);
	mfs->mfs_jiffy = 0;
	mfs->mfs_bindex = -EROFS;

	return 0;
}

static int au_wbr_create_fin_mfs(struct super_block *sb)
{
	AuTraceEnter();
	mutex_destroy(&stosi(sb)->si_wbr_mfs.mfs_lock);
	return 0;
}

/* ---------------------------------------------------------------------- */

/* most free space and then round robin */
static int au_wbr_create_mfsrr(struct dentry *dentry, int isdir)
{
	int err;
	struct au_wbr_mfs *mfs;

	//au_debug_on();
	LKTRTrace("%.*s, %d\n", AuDLNPair(dentry), isdir);

	err = au_wbr_create_mfs(dentry, isdir);
	if (err >= 0) {
		mfs = &stosi(dentry->d_sb)->si_wbr_mfs;
		LKTRTrace("%Lu bytes, %Lu wmark\n",
			  mfs->mfsrr_bytes, mfs->mfsrr_watermark);
		if (unlikely(mfs->mfsrr_bytes < mfs->mfsrr_watermark))
			err = au_wbr_create_rr(dentry, isdir);
	}

	LKTRTrace("b%d\n", err);
	//au_debug_off();
	return err;
}

static int au_wbr_create_init_mfsrr(struct super_block *sb)
{
	int err;
	//au_debug_on();
	au_wbr_create_init_mfs(sb); /* ignore */
	err = au_wbr_create_init_rr(sb);
	//au_debug_off();
	return err;
}

/* ---------------------------------------------------------------------- */

/* top down parent and most free space */
static int au_wbr_create_pmfs(struct dentry *dentry, int isdir)
{
	int err, e2;
	struct super_block *sb;
	struct dentry *parent;
	aufs_bindex_t bindex, bstart, bend;
	struct aufs_branch *br;
	u64 b;

	//au_debug_on();
	LKTRTrace("%.*s, %d\n", AuDLNPair(dentry), isdir);

	err = au_wbr_create_tdp(dentry, isdir);
	if (unlikely(err < 0))
		goto out;
	parent = dget_parent(dentry);
	bstart = dbstart(parent);
	bend = dbtaildir(parent);
	if (bstart == bend)
		goto out_parent; /* success */

	e2 = au_wbr_create_mfs(dentry, isdir);
	if (unlikely(e2 < 0))
		goto out_parent; /* success */

	/* when the available size is equal, select upper one */
	sb = dentry->d_sb;
	br = stobr(sb, err);
	b = br->br_bytes;
	LKTRTrace("b%d, %Lu\n", err, b);
	for (bindex = bstart; bindex <= bend; bindex++) {
		if (!au_h_dptr_i(parent, bindex))
			continue;
		br = stobr(sb, bindex);
		if (!br_rdonly(br) && br->br_bytes > b) {
			b = br->br_bytes;
			err = bindex;
			LKTRTrace("b%d, %Lu\n", err, b);
		}
	}

 out_parent:
	dput(parent);
 out:
	LKTRTrace("b%d\n", err);
	//au_debug_off();
	return err;
}

/* ---------------------------------------------------------------------- */

/* policies for copyup */

/* top down parent */
static int au_wbr_copyup_tdp(struct dentry *dentry)
{
	return au_wbr_create_tdp(dentry, /*isdir, anything is ok*/0);
}

/* bottom up parent */
static int au_wbr_copyup_bup(struct dentry *dentry)
{
	int err;
	struct dentry *parent, *h_parent;
	aufs_bindex_t bindex, bstart;
	struct super_block *sb;

	LKTRTrace("%.*s\n", AuDLNPair(dentry));

	err = -EROFS;
	sb = dentry->d_sb;
	parent = dget_parent(dentry);
	bstart = dbstart(parent);
	for (bindex = dbstart(dentry); bindex >= bstart; bindex--) {
		h_parent = au_h_dptr_i(parent, bindex);
		if (h_parent && !br_rdonly(stobr(sb, bindex))) {
			err = bindex;
			break;
		}
	}
	dput(parent);

	/* bottom up here */
	if (unlikely(err < 0))
		err = au_wbr_bu(sb, bstart - 1);

	LKTRTrace("b%d\n", err);
	return err;
}

/* bottom up */
static int au_wbr_copyup_bu(struct dentry *dentry)
{
	int err;

	LKTRTrace("%.*s\n", AuDLNPair(dentry));

	err = au_wbr_bu(dentry->d_sb, dbstart(dentry));

	LKTRTrace("b%d\n", err);
	return err;
}

/* ---------------------------------------------------------------------- */

struct au_wbr_copyup_operations au_wbr_copyup_ops[] = {
	[AuWbrCopyup_TDP]	= {
		.copyup	= au_wbr_copyup_tdp
	},
	[AuWbrCopyup_BUP]	= {
		.copyup	= au_wbr_copyup_bup
	},
	[AuWbrCopyup_BU]	= {
		.copyup	= au_wbr_copyup_bu
	}
};

struct au_wbr_create_operations au_wbr_create_ops[] = {
	[AuWbrCreate_TDP]	= {
		.create	= au_wbr_create_tdp
	},
	[AuWbrCreate_RR]	= {
		.create	= au_wbr_create_rr,
		.init	= au_wbr_create_init_rr
	},
	[AuWbrCreate_MFS]	= {
		.create	= au_wbr_create_mfs,
		.init	= au_wbr_create_init_mfs,
		.fin	= au_wbr_create_fin_mfs
	},
	[AuWbrCreate_MFSV]	= {
		.create	= au_wbr_create_mfs,
		.init	= au_wbr_create_init_mfs,
		.fin	= au_wbr_create_fin_mfs
	},
	[AuWbrCreate_MFSRR]	= {
		.create	= au_wbr_create_mfsrr,
		.init	= au_wbr_create_init_mfsrr,
		.fin	= au_wbr_create_fin_mfs
	},
	[AuWbrCreate_MFSRRV]	= {
		.create	= au_wbr_create_mfsrr,
		.init	= au_wbr_create_init_mfsrr,
		.fin	= au_wbr_create_fin_mfs
	},
	[AuWbrCreate_PMFS]	= {
		.create	= au_wbr_create_pmfs,
		.init	= au_wbr_create_init_mfs,
		.fin	= au_wbr_create_fin_mfs
	},
	[AuWbrCreate_PMFSV]	= {
		.create	= au_wbr_create_pmfs,
		.init	= au_wbr_create_init_mfs,
		.fin	= au_wbr_create_fin_mfs
	}
};

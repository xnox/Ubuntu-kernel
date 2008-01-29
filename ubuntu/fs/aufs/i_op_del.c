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

/* $Id: i_op_del.c,v 1.57 2008/01/28 05:02:03 sfjro Exp $ */

#include "aufs.h"

/* returns,
 * 0: wh is unnecessary
 * plus: wh is necessary
 * minus: error
 */
int au_wr_dir_need_wh(struct dentry *dentry, int isdir, aufs_bindex_t *bcpup,
		      struct dentry *locked)
{
	int need_wh, err;
	aufs_bindex_t bstart;
	struct dentry *h_dentry;
	struct super_block *sb;

	LKTRTrace("%.*s, isdir %d, *bcpup %d, locked %p\n",
		  AuDLNPair(dentry), isdir, *bcpup, locked);
	sb = dentry->d_sb;

	bstart = dbstart(dentry);
	LKTRTrace("bcpup %d, bstart %d\n", *bcpup, bstart);
	h_dentry = au_h_dptr(dentry);
	if (*bcpup < 0) {
		*bcpup = bstart;
		if (au_test_ro(sb, bstart, dentry->d_inode)) {
			err = AuWbrCopyup(stosi(sb), dentry);
			*bcpup = err;
			//err = -1;
			if (unlikely(err < 0))
				goto out;
		}
	} else
		AuDebugOn(bstart < *bcpup
			  || au_test_ro(sb, *bcpup, dentry->d_inode));
	LKTRTrace("bcpup %d, bstart %d\n", *bcpup, bstart);

	if (*bcpup != bstart) {
		err = au_cpup_dirs(dentry, *bcpup, locked);
		//err = -1;
		if (unlikely(err))
			goto out;
		need_wh = 1;
	} else {
		//struct nameidata nd;
		aufs_bindex_t old_bend, new_bend, bdiropq = -1;
		old_bend = dbend(dentry);
		if (isdir) {
			bdiropq = dbdiropq(dentry);
			set_dbdiropq(dentry, -1);
		}
		need_wh = au_lkup_dentry(dentry, bstart + 1, /*type*/0,
					 /*nd*/NULL);
		err = need_wh;
		//err = -1;
		if (isdir)
			set_dbdiropq(dentry, bdiropq);
		if (unlikely(err < 0))
			goto out;
		new_bend = dbend(dentry);
		if (!need_wh && old_bend != new_bend) {
			set_h_dptr(dentry, new_bend, NULL);
			set_dbend(dentry, old_bend);
#if 0 // remove this?
		} else if (!au_h_dptr_i(dentry, new_bend)->d_inode) {
			LKTRTrace("negative\n");
			set_h_dptr(dentry, new_bend, NULL);
			set_dbend(dentry, old_bend);
			need_wh = 0;
#endif
		}
	}
	LKTRTrace("need_wh %d\n", need_wh);
	err = need_wh;

 out:
	AuTraceErr(err);
	return err;
}

static struct dentry *
lock_hdir_create_wh(struct dentry *dentry, int isdir, aufs_bindex_t *rbcpup,
		    struct au_dtime *dt)
{
	struct dentry *wh_dentry;
	int err, need_wh;
	struct dentry *h_parent, *parent, *gparent;
	struct inode *dir, *h_dir, *gdir;
	struct aufs_ndx ndx;
	struct super_block *sb;
	struct aufs_hinode *hgdir;
	aufs_bindex_t bcpup;

	LKTRTrace("%.*s, isdir %d\n", AuDLNPair(dentry), isdir);

	need_wh = au_wr_dir_need_wh(dentry, isdir, rbcpup, NULL);
	err = need_wh;
	//err = -1;
	wh_dentry = ERR_PTR(err);
	if (unlikely(err < 0))
		goto out;

	//todo: meaningless lock if CONFIG_AUFS_DEBUG is disabled.
	hgdir = NULL;
	bcpup = *rbcpup;
	sb = dentry->d_sb;
	parent = dentry->d_parent; /* dir inode is locked */
	if (unlikely(au_flag_test_udba_inotify(sb) && !IS_ROOT(parent))) {
		gparent = dget_parent(parent);
		gdir = gparent->d_inode;
		ii_read_lock_parent2(gdir);
		hgdir = itohi(gdir, bcpup);
		ii_read_unlock(gdir);
		dput(gparent);
	}
	dir = parent->d_inode;
	IMustLock(dir);
	h_parent = au_h_dptr_i(parent, bcpup);
	h_dir = h_parent->d_inode;
#ifdef DbgUdbaRace
	AuDbgSleep(DbgUdbaRace);
#endif
	hdir_lock(h_dir, dir, bcpup);

	/*
	 * someone else might change our parent-child relationship directly,
	 * bypassing aufs, while we are handling a systemcall for aufs.
	 */
	wh_dentry = ERR_PTR(-ENOENT);
	if (unlikely(dbstart(dentry) == bcpup
		     && h_parent != au_h_dptr_i(dentry, bcpup)->d_parent))
		goto out_dir;
#if 0
	revalidate h_positive
#endif

	au_dtime_store(dt, parent, h_parent, hgdir);
	wh_dentry = NULL;
	if (!need_wh)
		goto out; /* success, no need to create whiteout */

	ndx.nfsmnt = au_nfsmnt(sb, bcpup);
	ndx.dlgt = au_need_dlgt(sb);
	ndx.nd = NULL;
	//ndx.br = NULL;
	wh_dentry = simple_create_wh(dir, dentry, bcpup, h_parent, &ndx);
	//wh_dentry = ERR_PTR(-1);
	if (!IS_ERR(wh_dentry))
		goto out; /* success */
	/* returns with the parent is locked and wh_dentry is DGETed */

 out_dir:
	hdir_unlock(h_dir, dir, bcpup);
 out:
	AuTraceErrPtr(wh_dentry);
	return wh_dentry;
}

static int renwh_and_rmdir(struct dentry *dentry, aufs_bindex_t bindex,
			   struct aufs_nhash *whlist, struct inode *dir)
{
	int rmdir_later, err;
	struct dentry *hidden_dentry;
	struct inode *inode, *h_inode;

	LKTRTrace("%.*s, b%d\n", AuDLNPair(dentry), bindex);

	inode = NULL;
	h_inode = NULL;
	if (unlikely(au_flag_test_udba_inotify(dentry->d_sb))) {
		inode = dentry->d_inode;
		h_inode = au_h_iptr_i(inode, bindex);
		hdir2_lock(h_inode, inode, bindex);
	}
	err = rename_whtmp(dir, dentry, bindex, /*noself*/1);
	if (unlikely(inode))
		hdir_unlock(h_inode, inode, bindex);
	//err = -1;
	if (unlikely(err))
		goto out;

	hidden_dentry = au_h_dptr_i(dentry, bindex);
	if (!au_test_nfs(hidden_dentry->d_sb)) {
		const int dirwh = stosi(dentry->d_sb)->si_dirwh;
		rmdir_later = (dirwh <= 1);
		if (!rmdir_later)
			rmdir_later = nhash_test_longer_wh(whlist, bindex,
							   dirwh);
		if (rmdir_later)
			return rmdir_later;
	}

	err = rmdir_whtmp(hidden_dentry, whlist, bindex, dir, dentry->d_inode,
			  /*noself*/1);
	//err = -1;
	if (unlikely(err)) {
		AuIOErr("rmdir %.*s, b%d failed, %d. ignored\n",
			AuDLNPair(hidden_dentry), bindex, err);
		err = 0;
	}

 out:
	AuTraceErr(err);
	return err;
}

static void epilog(struct inode *dir, struct dentry *dentry,
		   aufs_bindex_t bindex)
{
	//todo: unnecessary?
	d_drop(dentry);
	dentry->d_inode->i_ctime = dir->i_ctime;

	if (atomic_read(&dentry->d_count) == 1) {
		set_h_dptr(dentry, dbstart(dentry), NULL);
		au_update_dbstart(dentry);
	}
	if (ibstart(dir) == bindex)
		au_cpup_attr_timesizes(dir);
	dir->i_version++;
}

struct revert_flags {
	unsigned int	dlgt:1;
};

static int do_revert(int err, struct dentry *wh_dentry, struct dentry *dentry,
		     aufs_bindex_t bwh, struct au_dtime *dt,
		     struct revert_flags *flags)
{
	int rerr;
	struct inode *dir;

	dir = wh_dentry->d_parent->d_inode; /* dir inode is locked */
	IMustLock(dir);
	rerr = au_unlink_wh_dentry(dir, wh_dentry, dentry, dir, !!flags->dlgt);
	//rerr = -1;
	if (!rerr) {
		set_dbwh(dentry, bwh);
		au_dtime_revert(dt);
		return 0;
	}

	AuIOErr("%.*s reverting whiteout failed(%d, %d)\n",
		AuDLNPair(dentry), err, rerr);
	return -EIO;
}

/* ---------------------------------------------------------------------- */

int aufs_unlink(struct inode *dir, struct dentry *dentry)
{
	int err, dlgt;
	struct inode *inode, *h_dir;
	struct dentry *parent, *wh_dentry, *h_dentry;
	struct au_dtime dt;
	aufs_bindex_t bwh, bindex, bstart;
	struct super_block *sb;
	struct vfsub_args vargs;

	LKTRTrace("i%lu, %.*s\n", dir->i_ino, AuDLNPair(dentry));
	IMustLock(dir);
	inode = dentry->d_inode;
	if (unlikely(!inode))
		return -ENOENT; /* possible? */
	IMustLock(inode);

	aufs_read_lock(dentry, AuLock_DW);
	parent = dentry->d_parent; /* dir inode is locked */
	di_write_lock_parent(parent);

	bstart = dbstart(dentry);
	bwh = dbwh(dentry);
	bindex = -1;
	wh_dentry = lock_hdir_create_wh(dentry, /*isdir*/0, &bindex, &dt);
	//wh_dentry = ERR_PTR(-1);
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out;

	sb = dir->i_sb;
	dlgt = au_need_dlgt(sb);
	h_dentry = au_h_dptr(dentry);
	dget(h_dentry);

	if (bindex == bstart) {
		vfsub_args_init(&vargs, NULL, dlgt, 0);
		h_dir = h_dentry->d_parent->d_inode; /* dir inode is locked */
		IMustLock(h_dir);
		err = vfsub_unlink(h_dir, h_dentry, &vargs);
		//err = -1;
	} else {
		/* dir inode is locked */
		AuDebugOn(!wh_dentry
			  || wh_dentry->d_parent != au_h_dptr_i(parent,
								bindex));
		h_dir = wh_dentry->d_parent->d_inode;
		IMustLock(h_dir);
		err = 0;
	}

	if (!err) {
		inode->i_nlink--;
#if 0 //todo: update plink
		if (unlikely(!inode->i_nlink
			     && au_test_plink(sb, inode)
			     /* && atomic_read(&inode->i_count) == 2) */)) {
			au_debug_on();
			DbgInode(inode);
			au_debug_off();
		}
#endif
		epilog(dir, dentry, bindex);
		goto out_unlock; /* success */
	}

	/* revert */
	if (wh_dentry) {
		int rerr;
		struct revert_flags rev_flags = {
			.dlgt	= dlgt
		};
		rerr = do_revert(err, wh_dentry, dentry, bwh, &dt, &rev_flags);
		if (rerr)
			err = rerr;
	}

 out_unlock:
	hdir_unlock(h_dir, dir, bindex);
	dput(wh_dentry);
	dput(h_dentry);
 out:
	di_write_unlock(parent);
	aufs_read_unlock(dentry, AuLock_DW);
	AuTraceErr(err);
	return err;
}

int aufs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int err, rmdir_later;
	struct inode *inode, *h_dir;
	struct dentry *parent, *wh_dentry, *h_dentry;
	struct au_dtime dt;
	aufs_bindex_t bwh, bindex, bstart;
	struct rmdir_whtmp_args *args;
	struct aufs_nhash *whlist;
	struct super_block *sb;

	LKTRTrace("i%lu, %.*s\n", dir->i_ino, AuDLNPair(dentry));
	IMustLock(dir);
	inode = dentry->d_inode;
	if (unlikely(!inode))
		return -ENOENT; /* possible? */
	IMustLock(inode);

	whlist = nhash_new(GFP_TEMPORARY);
	err = PTR_ERR(whlist);
	if (IS_ERR(whlist))
		goto out;

	err = -ENOMEM;
	args = kmalloc(sizeof(*args), GFP_TEMPORARY);
	//args = NULL;
	if (unlikely(!args))
		goto out_whlist;

	aufs_read_lock(dentry, AuLock_DW);
	parent = dentry->d_parent; /* dir inode is locked */
	di_write_lock_parent(parent);
	err = au_test_empty(dentry, whlist);
	//err = -1;
	if (unlikely(err))
		goto out_args;

	bstart = dbstart(dentry);
	bwh = dbwh(dentry);
	bindex = -1;
	wh_dentry = lock_hdir_create_wh(dentry, /*isdir*/ 1, &bindex, &dt);
	//wh_dentry = ERR_PTR(-1);
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out_args;

	h_dentry = au_h_dptr(dentry);
	dget(h_dentry);

	rmdir_later = 0;
	if (bindex == bstart) {
		h_dir = h_dentry->d_parent->d_inode; /* dir inode is locked */
		IMustLock(h_dir);
		err = renwh_and_rmdir(dentry, bstart, whlist, dir);
		//err = -1;
		if (err > 0) {
			rmdir_later = err;
			err = 0;
		}
	} else {
		/* dir inode is locked */
		AuDebugOn(!wh_dentry
			  || wh_dentry->d_parent != au_h_dptr_i(parent,
								bindex));
		h_dir = wh_dentry->d_parent->d_inode;
		IMustLock(h_dir);
		err = 0;
	}

	sb = dentry->d_sb;
	if (!err) {
		//aufs_bindex_t bi, bend;

		au_reset_hinotify(inode, /*flags*/0);
		inode->i_nlink = 0;
		set_dbdiropq(dentry, -1);
		epilog(dir, dentry, bindex);

		if (rmdir_later) {
			kick_rmdir_whtmp(h_dentry, whlist, bstart, dir,
					 inode, /*noself*/1, args);
			args = NULL;
		}

		goto out_unlock; /* success */
	}

	/* revert */
	LKTRLabel(revert);
	if (wh_dentry) {
		int rerr;
		struct revert_flags rev_flags = {
			.dlgt	= au_need_dlgt(sb)
		};
		rerr = do_revert(err, wh_dentry, dentry, bwh, &dt, &rev_flags);
		if (rerr)
			err = rerr;
	}

 out_unlock:
	hdir_unlock(h_dir, dir, bindex);
	dput(wh_dentry);
	dput(h_dentry);
 out_args:
	di_write_unlock(parent);
	aufs_read_unlock(dentry, AuLock_DW);
	kfree(args);
 out_whlist:
	nhash_del(whlist);
 out:
	AuTraceErr(err);
	return err;
}

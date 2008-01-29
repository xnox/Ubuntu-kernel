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

/* $Id: i_op_ren.c,v 1.63 2008/01/28 05:02:03 sfjro Exp $ */

#include "aufs.h"

enum { SRC, DST };
struct rename_args {
	struct dentry *h_dentry[2], *parent[2], *h_parent[2];
	struct aufs_nhash whlist;
	aufs_bindex_t btgt, bstart[2];
	struct super_block *sb;

	unsigned int isdir:1;
	unsigned int issamedir:1;
	unsigned int whsrc:1;
	unsigned int whdst:1;
	unsigned int dlgt:1;
	unsigned int udba:1;
} __aligned(sizeof(long));

static int do_rename(struct inode *src_dir, struct dentry *src_dentry,
		     struct inode *dir, struct dentry *dentry,
		     struct rename_args *a)
{
	int err, need_diropq, bycpup, rerr;
	struct rmdir_whtmp_args *thargs;
	struct dentry *wh_dentry[2], *h_dst;
	struct inode *h_dir[2];
	aufs_bindex_t bindex, bend;
	struct aufs_hin_ignore ign;
	struct vfsub_args vargs;
	struct aufs_ndx ndx = {
		.dlgt	= a->dlgt,
		.nd	= NULL,
		//.br	= NULL
	};
	struct au_cpup_flags cflags;

	LKTRTrace("%.*s/%.*s, %.*s/%.*s, "
		  "hd{%p, %p}, hp{%p, %p}, wh %p, btgt %d, bstart{%d, %d}, "
		  "flags{%d, %d, %d, %d}\n",
		  AuDLNPair(a->parent[SRC]), AuDLNPair(src_dentry),
		  AuDLNPair(a->parent[DST]), AuDLNPair(dentry),
		  a->h_dentry[SRC], a->h_dentry[DST],
		  a->h_parent[SRC], a->h_parent[DST],
		  &a->whlist, a->btgt,
		  a->bstart[SRC], a->bstart[DST],
		  a->isdir, a->issamedir, a->whsrc, a->whdst);

	h_dir[SRC] = a->h_parent[SRC]->d_inode;
	h_dir[DST] = a->h_parent[DST]->d_inode;

	/* prepare workqueue args */
	h_dst = NULL;
	thargs = NULL;
	if (a->isdir && a->h_dentry[DST]->d_inode) {
		err = -ENOMEM;
		thargs = kmalloc(sizeof(*thargs), GFP_TEMPORARY);
		//thargs = NULL;
		if (unlikely(!thargs))
			goto out;
		h_dst = dget(a->h_dentry[DST]);
	}

	wh_dentry[SRC] = NULL;
	wh_dentry[DST] = NULL;
	ndx.nfsmnt = au_nfsmnt(a->sb, a->btgt);
	/* create whiteout for src_dentry */
	if (a->whsrc) {
		wh_dentry[SRC] = simple_create_wh(src_dir, src_dentry, a->btgt,
						  a->h_parent[SRC], &ndx);
		//wh_dentry[SRC] = ERR_PTR(-1);
		err = PTR_ERR(wh_dentry[SRC]);
		if (IS_ERR(wh_dentry[SRC]))
			goto out_thargs;
	}

	/* lookup whiteout for dentry */
	if (a->whdst) {
		struct dentry *d;
		d = lkup_wh(a->h_parent[DST], &dentry->d_name, &ndx);
		//d = ERR_PTR(-1);
		err = PTR_ERR(d);
		if (IS_ERR(d))
			goto out_whsrc;
		if (!d->d_inode)
			dput(d);
		else
			wh_dentry[DST] = d;
	}

	/* rename dentry to tmpwh */
	if (thargs) {
		err = rename_whtmp(dir, dentry, a->btgt, /*noself*/0);
		//err = -1;
		if (unlikely(err))
			goto out_whdst;
		set_h_dptr(dentry, a->btgt, NULL);
		err = au_lkup_neg(dentry, a->btgt);
		//err = -1;
		if (unlikely(err))
			goto out_whtmp;
		a->h_dentry[DST] = au_h_dptr_i(dentry, a->btgt);
	}

	/* cpup src */
	if (a->h_dentry[DST]->d_inode && a->bstart[SRC] != a->btgt) {
		cflags.dtime = 0;
		vfsub_i_lock_nested(a->h_dentry[SRC]->d_inode, AuLsc_I_CHILD);
		err = au_sio_cpup_simple(src_dentry, a->btgt, -1, &cflags);
		//err = -1; // untested dir
		vfsub_i_unlock(a->h_dentry[SRC]->d_inode);
		if (unlikely(err))
			goto out_whtmp;
	}

	/* rename by vfs_rename or cpup */
	need_diropq = a->isdir
		&& (wh_dentry[DST]
		    || dbdiropq(dentry) == a->btgt
		    /* hide the lower to keep xino */
		    || a->btgt < dbend(dentry)
		    || AuFlag(stosi(a->sb), f_always_diropq));
	bycpup = 0;
	if (dbstart(src_dentry) == a->btgt) {
		if (need_diropq && dbdiropq(src_dentry) == a->btgt)
			need_diropq = 0;
		vfsub_args_init(&vargs, &ign, a->dlgt, 0);
		if (unlikely(a->udba && a->isdir))
			vfsub_ign_hinode(&vargs, IN_MOVE_SELF,
					 itohi(src_dentry->d_inode, a->btgt));
		err = vfsub_rename(h_dir[SRC], au_h_dptr(src_dentry),
				   h_dir[DST], a->h_dentry[DST], &vargs);
		//err = -1;
	} else {
		bycpup = 1;
		cflags.dtime = 0;
		vfsub_i_lock_nested(a->h_dentry[SRC]->d_inode, AuLsc_I_CHILD);
		set_dbstart(src_dentry, a->btgt);
		set_h_dptr(src_dentry, a->btgt, dget(a->h_dentry[DST]));
		err = au_sio_cpup_single(src_dentry, a->btgt, a->bstart[SRC],
					 -1, &cflags);
		//err = -1; // untested dir
		if (unlikely(err)) {
			set_h_dptr(src_dentry, a->btgt, NULL);
			set_dbstart(src_dentry, a->bstart[SRC]);
		}
		vfsub_i_unlock(a->h_dentry[SRC]->d_inode);
	}
	if (unlikely(err))
		goto out_whtmp;

	/* make dir opaque */
	if (need_diropq) {
		struct dentry *diropq;
		struct inode *h_inode;

		h_inode = au_h_dptr_i(src_dentry, a->btgt)->d_inode;
		hdir_lock(h_inode, src_dentry->d_inode, a->btgt);
		diropq = create_diropq(src_dentry, a->btgt, a->dlgt);
		//diropq = ERR_PTR(-1);
		hdir_unlock(h_inode, src_dentry->d_inode, a->btgt);
		err = PTR_ERR(diropq);
		if (IS_ERR(diropq))
			goto out_rename;
		dput(diropq);
	}

	/* remove whiteout for dentry */
	if (wh_dentry[DST]) {
		err = au_unlink_wh_dentry(h_dir[DST], wh_dentry[DST],
					  dentry, dir, /*dlgt*/0);
		//err = -1;
		if (unlikely(err))
			goto out_diropq;
	}

	/* remove whtmp */
	if (thargs) {
		if (au_test_nfs(h_dst->d_sb)
		    || !nhash_test_longer_wh(&a->whlist, a->btgt,
					     stosi(a->sb)->si_dirwh)) {
			err = rmdir_whtmp(h_dst, &a->whlist, a->btgt, dir,
					  dentry->d_inode, /*noself*/0);
			if (unlikely(err))
				AuWarn("failed removing whtmp dir %.*s (%d), "
				       "ignored.\n", AuDLNPair(h_dst), err);
		} else {
			kick_rmdir_whtmp(h_dst, &a->whlist, a->btgt, dir,
					 dentry->d_inode, /*noself*/0, thargs);
			dput(h_dst);
			thargs = NULL;
		}
	}
	err = 0;
	goto out_success;

#define RevertFailure(fmt, args...) do { \
		AuIOErrWhck("revert failure: " fmt " (%d, %d)\n", \
			    ##args, err, rerr); \
		err = -EIO; \
	} while (0)

 out_diropq:
	if (need_diropq) {
		struct inode *h_inode;

		h_inode = au_h_dptr_i(src_dentry, a->btgt)->d_inode;
		//br_wh_read_lock(stobr(a->sb, a->btgt));
		/* i_lock simply since inotify is not set to h_inode. */
		vfsub_i_lock_nested(h_inode, AuLsc_I_PARENT);
		//hdir_lock(h_inode, src_dentry->d_inode, a->btgt);
		rerr = remove_diropq(src_dentry, a->btgt, a->dlgt);
		//rerr = -1;
		//hdir_unlock(h_inode, src_dentry->d_inode, a->btgt);
		vfsub_i_unlock(h_inode);
		//br_wh_read_unlock(stobr(a->sb, a->btgt));
		if (rerr)
			RevertFailure("remove diropq %.*s",
				      AuDLNPair(src_dentry));
	}
 out_rename:
	if (!bycpup) {
		struct dentry *d;
		struct qstr *name = &src_dentry->d_name;
		d = au_lkup_one(name->name, a->h_parent[SRC], name->len, &ndx);
		//d = ERR_PTR(-1);
		rerr = PTR_ERR(d);
		if (IS_ERR(d)) {
			RevertFailure("au_lkup_one %.*s",
				      AuDLNPair(src_dentry));
			goto out_whtmp;
		}
		AuDebugOn(d->d_inode);
		vfsub_args_init(&vargs, &ign, a->dlgt, 0);
		if (unlikely(a->udba && a->isdir))
			vfsub_ign_hinode(&vargs, IN_MOVE_SELF,
					 itohi(src_dentry->d_inode, a->btgt));
		rerr = vfsub_rename
			(h_dir[DST], au_h_dptr_i(src_dentry, a->btgt),
			 h_dir[SRC], d, &vargs);
		//rerr = -1;
		d_drop(d);
		dput(d);
		//set_h_dptr(src_dentry, a->btgt, NULL);
		if (rerr)
			RevertFailure("rename %.*s", AuDLNPair(src_dentry));
	} else {
		vfsub_args_init(&vargs, NULL, a->dlgt, 0);
		rerr = vfsub_unlink(h_dir[DST], a->h_dentry[DST], &vargs);
		//rerr = -1;
		set_h_dptr(src_dentry, a->btgt, NULL);
		set_dbstart(src_dentry, a->bstart[SRC]);
		if (rerr)
			RevertFailure("unlink %.*s",
				      AuDLNPair(a->h_dentry[DST]));
	}
 out_whtmp:
	if (thargs) {
		struct dentry *d;
		struct qstr *name = &dentry->d_name;
		LKTRLabel(here);
		d = au_lkup_one(name->name, a->h_parent[DST], name->len, &ndx);
		//d = ERR_PTR(-1);
		rerr = PTR_ERR(d);
		if (IS_ERR(d)) {
			RevertFailure("lookup %.*s", AuLNPair(name));
			goto out_whdst;
		}
		if (d->d_inode) {
			d_drop(d);
			dput(d);
			goto out_whdst;
		}
		AuDebugOn(d->d_inode);
		vfsub_args_init(&vargs, &ign, a->dlgt, 0);
		if (unlikely(0 && a->udba && a->isdir))
			vfsub_ign_hinode(&vargs, IN_MOVE_SELF,
					 itohi(dentry->d_inode, a->btgt));
		rerr = vfsub_rename(h_dir[DST], h_dst, h_dir[DST], d, &vargs);
		//rerr = -1;
		d_drop(d);
		dput(d);
		if (rerr) {
			RevertFailure("rename %.*s", AuDLNPair(h_dst));
			goto out_whdst;
		}
		set_h_dptr(dentry, a->btgt, NULL);
		set_h_dptr(dentry, a->btgt, dget(h_dst));
	}
 out_whdst:
	dput(wh_dentry[DST]);
	wh_dentry[DST] = NULL;
 out_whsrc:
	if (wh_dentry[SRC]) {
		LKTRLabel(here);
		rerr = au_unlink_wh_dentry(h_dir[SRC], wh_dentry[SRC],
					   src_dentry, src_dir, /*dlgt*/0);
		//rerr = -1;
		if (rerr)
			RevertFailure("unlink %.*s", AuDLNPair(wh_dentry[SRC]));
	}
#undef RevertFailure
	d_drop(src_dentry);
	bend = dbend(src_dentry);
	for (bindex = dbstart(src_dentry); bindex <= bend; bindex++) {
		struct dentry *hd;
		hd = au_h_dptr_i(src_dentry, bindex);
		if (hd)
			d_drop(hd);
	}
	d_drop(dentry);
	bend = dbend(dentry);
	for (bindex = dbstart(dentry); bindex <= bend; bindex++) {
		struct dentry *hd;
		hd = au_h_dptr_i(dentry, bindex);
		if (hd)
			d_drop(hd);
	}
	au_update_dbstart(dentry);
	if (thargs)
		d_drop(h_dst);
 out_success:
	dput(wh_dentry[SRC]);
	dput(wh_dentry[DST]);
 out_thargs:
	if (thargs) {
		dput(h_dst);
		kfree(thargs);
	}
 out:
	AuTraceErr(err);
	return err;
}

/*
 * test if @dentry dir can be rename destination or not.
 * success means, it is a logically empty dir.
 */
static int may_rename_dstdir(struct dentry *dentry, aufs_bindex_t btgt,
			     struct aufs_nhash *whlist)
{
	LKTRTrace("%.*s\n", AuDLNPair(dentry));

	return au_test_empty(dentry, whlist);
}

/*
 * test if @dentry dir can be rename source or not.
 * if it can, return 0 and @children is filled.
 * success means,
 * - or, it is a logically empty dir.
 * - or, it exists on writable branch and has no children including whiteouts
 *       on the lower branch.
 */
static int may_rename_srcdir(struct dentry *dentry, aufs_bindex_t btgt)
{
	int err;
	aufs_bindex_t bstart;

	LKTRTrace("%.*s\n", AuDLNPair(dentry));

	bstart = dbstart(dentry);
	if (bstart != btgt) {
		struct aufs_nhash *whlist;

		whlist = nhash_new(GFP_TEMPORARY);
		err = PTR_ERR(whlist);
		if (IS_ERR(whlist))
			goto out;
		err = au_test_empty(dentry, whlist);
		nhash_del(whlist);
		goto out;
	}

	if (bstart == dbtaildir(dentry))
		return 0; /* success */

	err = au_test_empty_lower(dentry);

 out:
	if (/* unlikely */(err == -ENOTEMPTY)) {
		AuWarn1("renaming dir who has child(ren) on multiple branches,"
		      " is not supported\n");
		err = -EXDEV;
	}
	AuTraceErr(err);
	return err;
}

/* mainly for link(2) and rename(2) */
int au_wbr(struct dentry *dentry, aufs_bindex_t btgt)
{
	aufs_bindex_t bdiropq, bwh;
	struct dentry *parent;

	LKTRTrace("%.*s, b%d\n", AuDLNPair(dentry), btgt);
	parent = dentry->d_parent;
	IMustLock(parent->d_inode); /* dir is locked */

	bdiropq = dbdiropq(parent);
	bwh = dbwh(dentry);
	if (unlikely(br_rdonly(stobr(dentry->d_sb, btgt))
		     || (0 <= bdiropq && bdiropq < btgt)
		     || (0 <= bwh && bwh < btgt)))
		btgt = -1;

	LKTRTrace("btgt %d\n", btgt);
	return btgt;
}

//todo: meaningless lock if CONFIG_AUFS_DEBUG is disabled.
static void au_hgdirs(struct aufs_hinode **hgdir, struct rename_args *a)
{
	struct dentry *gparent[2];
	struct inode *gdir;

	if (!a->udba)
		return;

	gparent[SRC] = NULL;
	if (!IS_ROOT(a->parent[SRC])) {
		gparent[SRC] = dget_parent(a->parent[SRC]);
		gdir = gparent[SRC]->d_inode;
		if (gparent[SRC] != a->parent[DST]) {
			ii_read_lock_parent3(gdir);
			hgdir[SRC] = itohi(gdir, a->btgt);
			ii_read_unlock(gdir);
		} else
			hgdir[SRC] = itohi(gdir, a->btgt);
		dput(gparent[SRC]);
	}

	if (!a->issamedir
	    && !IS_ROOT(a->parent[DST])
	    && a->parent[DST] != gparent[SRC]) {
		gparent[DST] = dget_parent(a->parent[DST]);
		gdir = gparent[DST]->d_inode;
		if (gparent[DST] != a->parent[SRC]) {
			ii_read_lock_parent3(gdir);
			hgdir[DST] = itohi(gdir, a->btgt);
			ii_read_unlock(gdir);
		} else
			hgdir[DST] = itohi(gdir, a->btgt);
		dput(gparent[DST]);
	}
}

int aufs_rename(struct inode *src_dir, struct dentry *src_dentry,
		struct inode *dir, struct dentry *dentry)
{
	int err, do_dt_dstdir, flags;
	aufs_bindex_t bend, bindex;
	struct inode *inode[2], *dirs[2];
	struct aufs_hinode *hgdir[2];
	enum { PARENT, CHILD };
	/* reduce stack space */
	struct {
		struct rename_args a;
		struct au_dtime dt[2][2];
	} *p;
	struct au_wr_dir_args wr_dir_args = {
		//.force_btgt	= -1,
		.add_entry	= 1,
		.do_lock_srcdir	= 0,
		//.isdir	= 0
	};

	LKTRTrace("i%lu, %.*s, i%lu, %.*s\n",
		  src_dir->i_ino, AuDLNPair(src_dentry),
		  dir->i_ino, AuDLNPair(dentry));
	IMustLock(src_dir);
	IMustLock(dir);
	inode[DST] = dentry->d_inode;
	if (inode[DST]) {
		IMustLock(inode[DST]);
		igrab(inode[DST]);
	}

	err = -ENOMEM;
	BUILD_BUG_ON(sizeof(*p) > PAGE_SIZE);
	p = kmalloc(sizeof(*p), GFP_TEMPORARY);
	if (unlikely(!p))
		goto out;

	err = -ENOTDIR;
	p->a.sb = src_dentry->d_sb;
	inode[SRC] = src_dentry->d_inode;
	p->a.isdir = !!S_ISDIR(inode[SRC]->i_mode);
	if (unlikely(p->a.isdir && inode[DST]
		     && !S_ISDIR(inode[DST]->i_mode)))
		goto out_free;

	flags = 0;
	if (p->a.isdir)
		flags = AuLock_DIR;
	aufs_read_and_write_lock2(dentry, src_dentry, flags);
	p->a.dlgt = !!au_need_dlgt(p->a.sb);
	p->a.udba = !!au_flag_test_udba_inotify(p->a.sb);
	p->a.parent[SRC] = src_dentry->d_parent; /* dir inode is locked */
	p->a.parent[DST] = dentry->d_parent; /* dir inode is locked */
	p->a.issamedir = (src_dir == dir);
	if (p->a.issamedir)
		di_write_lock_parent(p->a.parent[DST]);
	else
		di_write_lock2_parent(p->a.parent[SRC], p->a.parent[DST],
				      /*isdir*/1);

	/* which branch we process */
	p->a.bstart[SRC] = dbstart(src_dentry);
	p->a.bstart[DST] = dbstart(dentry);
	wr_dir_args.isdir = p->a.isdir;
	wr_dir_args.force_btgt = p->a.bstart[SRC];
	if (dentry->d_inode && p->a.bstart[DST] < p->a.bstart[SRC])
		wr_dir_args.force_btgt = p->a.bstart[DST];
	wr_dir_args.force_btgt = au_wbr(dentry, wr_dir_args.force_btgt);
	err = au_wr_dir(dentry, src_dentry, &wr_dir_args);
	p->a.btgt = err;
	if (unlikely(err < 0))
		goto out_unlock;

	/* are they available to be renamed */
	err = 0;
	nhash_init(&p->a.whlist);
	if (p->a.isdir && inode[DST]) {
		set_dbstart(dentry, p->a.bstart[DST]);
		err = may_rename_dstdir(dentry, p->a.btgt, &p->a.whlist);
		set_dbstart(dentry, p->a.btgt);
	}
	p->a.h_dentry[DST] = au_h_dptr(dentry);
	if (unlikely(err))
		goto out_unlock;
	//todo: minor optimize, their sb may be same while their bindex differs.
	p->a.h_dentry[SRC] = au_h_dptr(src_dentry);
	if (p->a.isdir) {
		err = may_rename_srcdir(src_dentry, p->a.btgt);
		if (unlikely(err))
			goto out_children;
	}

	/* prepare the writable parent dir on the same branch */
	err = au_wr_dir_need_wh(src_dentry, p->a.isdir, &p->a.btgt,
				p->a.issamedir ? NULL : p->a.parent[DST]);
	if (unlikely(err < 0))
		goto out_children;
	p->a.whsrc = !!err;
	p->a.whdst = (p->a.bstart[DST] == p->a.btgt);
	if (!p->a.whdst) {
		err = au_cpup_dirs(dentry, p->a.btgt,
				   p->a.issamedir ? NULL : p->a.parent[SRC]);
		if (unlikely(err))
			goto out_children;
	}

	hgdir[SRC] = NULL;
	hgdir[DST] = NULL;
	au_hgdirs(hgdir, &p->a);
	p->a.h_parent[SRC] = au_h_dptr_i(p->a.parent[SRC], p->a.btgt);
	p->a.h_parent[DST] = au_h_dptr_i(p->a.parent[DST], p->a.btgt);
	dirs[0] = src_dir;
	dirs[1] = dir;
#ifdef DbgUdbaRace
	AuDbgSleep(DbgUdbaRace);
#endif
	hdir_lock_rename(p->a.h_parent, dirs, p->a.btgt, p->a.issamedir);

	/*
	 * someone else might change our parent-child relationship directly,
	 * bypassing aufs, while we are handling a systemcall for aufs.
	 */
	err = -ENOENT;
	if (unlikely(p->a.bstart[SRC] == p->a.btgt
		     && p->a.h_parent[SRC]
		     != au_h_dptr_i(src_dentry, p->a.btgt)->d_parent))
		goto out_hdir;
	err = -EIO;
	if (unlikely(p->a.bstart[DST] == p->a.btgt
		     && p->a.h_parent[DST]
		     != au_h_dptr_i(dentry, p->a.btgt)->d_parent))
		goto out_hdir;
#if 0
	// revalidate h_dentries
	// test parent-gparent relationship
#endif

	/* store timestamps to be revertible */
	au_dtime_store(p->dt[PARENT] + SRC, p->a.parent[SRC],
		       p->a.h_parent[SRC], hgdir[SRC]);
	if (!p->a.issamedir)
		au_dtime_store(p->dt[PARENT] + DST, p->a.parent[DST],
			       p->a.h_parent[DST], hgdir[DST]);
	do_dt_dstdir = 0;
	if (p->a.isdir) {
		au_dtime_store
			(p->dt[CHILD] + SRC, src_dentry, p->a.h_dentry[SRC],
			 itohi(p->a.parent[SRC]->d_inode, p->a.btgt));
		if (p->a.h_dentry[DST]->d_inode) {
			do_dt_dstdir = 1;
			au_dtime_store
				(p->dt[CHILD] + DST, dentry, p->a.h_dentry[DST],
				 itohi(p->a.parent[DST]->d_inode, p->a.btgt));
		}
	}

	err = do_rename(src_dir, src_dentry, dir, dentry, &p->a);
	if (unlikely(err))
		goto out_dt;
	hdir_unlock_rename(p->a.h_parent, dirs, p->a.btgt, p->a.issamedir);

	/* update dir attributes */
	dir->i_version++;
	if (unlikely(p->a.isdir)) {
		/* is this updating defined in POSIX? */
		//vfsub_i_lock(inode[SRC]);
		au_cpup_attr_timesizes(inode[SRC]);
		//vfsub_i_unlock(inode[SRC]);

		au_cpup_attr_nlink(dir);
		if (unlikely(inode[DST])) {
			inode[DST]->i_nlink--;
			if (unlikely(p->a.isdir))
				inode[DST]->i_nlink = 0;
			au_cpup_attr_timesizes(inode[DST]);
		}
	}
	if (ibstart(dir) == p->a.btgt)
		au_cpup_attr_timesizes(dir);

	if (!p->a.issamedir) {
		src_dir->i_version++;
		if (unlikely(p->a.isdir))
			au_cpup_attr_nlink(src_dir);
		if (ibstart(src_dir) == p->a.btgt)
			au_cpup_attr_timesizes(src_dir);
	}

#if 0 // todo: test it
	d_drop(src_dentry);
#else
	/* dput/iput all lower dentries */
	set_dbwh(src_dentry, -1);
	bend = dbend(src_dentry);
	for (bindex = p->a.btgt + 1; bindex <= bend; bindex++) {
		struct dentry *hd;
		hd = au_h_dptr_i(src_dentry, bindex);
		if (hd)
			set_h_dptr(src_dentry, bindex, NULL);
	}
	set_dbend(src_dentry, p->a.btgt);

	bend = ibend(inode[SRC]);
	for (bindex = p->a.btgt + 1; bindex <= bend; bindex++) {
		struct inode *hi;
		hi = au_h_iptr_i(inode[SRC], bindex);
		if (hi) {
			//AuDbg("hi%lu, i%lu\n", hi->i_ino, 0LU);
			xino_write0(p->a.sb, bindex, hi->i_ino, 0);
			/* ignore this error */
			set_h_iptr(inode[SRC], bindex, NULL, 0);
		}
	}
	set_ibend(inode[SRC], p->a.btgt);
#endif

#if 0 // remove this
	if (unlikely(inode[DST])) {
		struct inode *h_i;

		bend = ibend(inode[DST]);
		for (bindex = ibstart(inode[DST]); bindex <= bend; bindex++) {
			h_i = au_h_iptr_i(inode[DST], bindex);
			if (h_i)
				xino_write0(p->a.sb, bindex, h_i->i_ino, 0);
			/* ignore this error */
			/* bad action? */
		}
	}
#endif

	goto out_children; /* success */

 out_dt:
	au_dtime_revert(p->dt[PARENT] + SRC);
	if (!p->a.issamedir)
		au_dtime_revert(p->dt[PARENT] + DST);
	if (p->a.isdir && err != -EIO) {
		struct dentry *hd;

		hd = p->dt[CHILD][SRC].dt_h_dentry;
		vfsub_i_lock_nested(hd->d_inode, AuLsc_I_CHILD);
		au_dtime_revert(p->dt[CHILD] + SRC);
		vfsub_i_unlock(hd->d_inode);
		if (do_dt_dstdir) {
			hd = p->dt[CHILD][DST].dt_h_dentry;
			vfsub_i_lock_nested(hd->d_inode, AuLsc_I_CHILD);
			au_dtime_revert(p->dt[CHILD] + DST);
			vfsub_i_unlock(hd->d_inode);
		}
	}
 out_hdir:
	hdir_unlock_rename(p->a.h_parent, dirs, p->a.btgt, p->a.issamedir);
 out_children:
	nhash_fin(&p->a.whlist);
 out_unlock:
	//if (unlikely(err /* && p->a.isdir */)) {
	if (unlikely(err && p->a.isdir)) {
		au_update_dbstart(dentry);
		d_drop(dentry);
	}
	if (p->a.issamedir)
		di_write_unlock(p->a.parent[DST]);
	else
		di_write_unlock2(p->a.parent[SRC], p->a.parent[DST]);
	aufs_read_and_write_unlock2(dentry, src_dentry);
 out_free:
	kfree(p);
 out:
	iput(inode[DST]);
	AuTraceErr(err);
	return err;
}

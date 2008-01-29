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

/* $Id: i_op_add.c,v 1.58 2008/01/28 05:02:00 sfjro Exp $ */

#include "aufs.h"

/*
 * final procedure of adding a new entry, except link(2).
 * remove whiteout, instantiate, copyup the parent dir's times and size
 * and update version.
 * if it failed, re-create the removed whiteout.
 */
static int epilog(struct inode *dir, struct dentry *wh_dentry,
		  struct dentry *dentry)
{
	int err, rerr;
	aufs_bindex_t bwh;
	struct inode *inode, *h_dir;
	struct dentry *wh;
	struct aufs_ndx ndx;

	LKTRTrace("wh %p, %.*s\n", wh_dentry, AuDLNPair(dentry));

	bwh = -1;
	if (wh_dentry) {
		h_dir = wh_dentry->d_parent->d_inode; /* dir inode is locked */
		IMustLock(h_dir);
		bwh = dbwh(dentry);
		err = au_unlink_wh_dentry(h_dir, wh_dentry, dentry, dir,
					  /*dlgt*/0);
		//err = -1;
		if (unlikely(err))
			goto out;
	}

	inode = au_new_inode(dentry);
	//inode = ERR_PTR(-1);
	if (!IS_ERR(inode)) {
		d_instantiate(dentry, inode);
		dir = dentry->d_parent->d_inode; /* dir inode is locked */
		IMustLock(dir);
		/* or always cpup dir mtime? */
		if (ibstart(dir) == dbstart(dentry))
			au_cpup_attr_timesizes(dir);
		dir->i_version++;
		return 0; /* success */
	}

	err = PTR_ERR(inode);
	if (!wh_dentry)
		goto out;

	/* revert */
	ndx.dlgt = au_need_dlgt(dentry->d_sb);
	ndx.nfsmnt = au_nfsmnt(dentry->d_sb, bwh);
	ndx.nd = NULL;
	//ndx.br = NULL;
	/* dir inode is locked */
	wh = simple_create_wh(dir, dentry, bwh, wh_dentry->d_parent, &ndx);
	//wh = ERR_PTR(-1);
	rerr = PTR_ERR(wh);
	if (!IS_ERR(wh)) {
		dput(wh);
		goto out;
	}
	AuIOErr("%.*s reverting whiteout failed(%d, %d)\n",
		AuDLNPair(dentry), err, rerr);
	err = -EIO;

 out:
	AuTraceErr(err);
	return err;
}

/*
 * initial procedure of adding a new entry.
 * prepare writable branch and the parent dir, lock it,
 * lookup whiteout for the new entry.
 */
static struct dentry *
lock_hdir_lkup_wh(struct dentry *dentry, struct au_dtime *dt,
		  struct dentry *src_dentry, struct au_wr_dir_args *wr_dir_args)
{
	struct dentry *wh_dentry, *parent, *h_parent, *gparent;
	int err;
	aufs_bindex_t bstart, bcpup;
	struct inode *dir, *h_dir, *gdir;
	struct aufs_ndx ndx;
	struct super_block *sb;
	struct aufs_hinode *hgdir;

	LKTRTrace("%.*s, src %p\n", AuDLNPair(dentry), src_dentry);

	parent = dentry->d_parent; /* dir inode is locked */
	IMustLock(parent->d_inode);
	bstart = dbstart(dentry);
	err = au_wr_dir(dentry, src_dentry, wr_dir_args);
	bcpup = err;
	//err = -1;
	wh_dentry = ERR_PTR(err);
	if (unlikely(err < 0))
		goto out;

	sb = parent->d_sb;
	//todo: meaningless lock if CONFIG_AUFS_DEBUG is disabled.
	hgdir = NULL;
	if (unlikely(dt && au_flag_test_udba_inotify(sb) && !IS_ROOT(parent))) {
		gparent = dget_parent(parent);
		gdir = gparent->d_inode;
		ii_read_lock_parent2(gdir);
		hgdir = itohi(gdir, bcpup);
		ii_read_unlock(gdir);
		dput(gparent);
	}
	dir = parent->d_inode;
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
	wh_dentry = ERR_PTR(-EIO);
	if (unlikely(h_parent != au_h_dptr_i(dentry, bcpup)->d_parent))
		goto out_dir;
#if 0
	revalidate h_negative
#endif

	if (dt)
		au_dtime_store(dt, parent, h_parent, hgdir);
	wh_dentry = NULL;
	if (/* bcpup != bstart || */ bcpup != dbwh(dentry))
		goto out; /* success */

	ndx.nfsmnt = au_nfsmnt(sb, bcpup);
	ndx.dlgt = au_need_dlgt(sb);
	ndx.nd = NULL;
	//ndx.br = NULL;
	wh_dentry = lkup_wh(h_parent, &dentry->d_name, &ndx);
	//wh_dentry = ERR_PTR(-1);

 out_dir:
	if (IS_ERR(wh_dentry))
		hdir_unlock(h_dir, dir, bcpup);
 out:
	AuTraceErrPtr(wh_dentry);
	return wh_dentry;
}

/* ---------------------------------------------------------------------- */

enum { Mknod, Symlink, Creat };
struct simple_arg {
	int type;
	union {
		struct {
			int mode;
			struct nameidata *nd;
		} c;
		struct {
			const char *symname;
		} s;
		struct {
			int mode;
			dev_t dev;
		} m;
	} u;
};

static int add_simple(struct inode *dir, struct dentry *dentry,
		      struct simple_arg *arg)
{
	int err, dlgt, created;
	struct dentry *h_dentry, *h_parent, *wh_dentry, *parent;
	struct inode *h_dir;
	struct au_dtime dt;
	struct vfsub_args vargs;
	struct super_block *sb;
	struct au_wr_dir_args wr_dir_args = {
		.force_btgt	= -1,
		.add_entry	= 1,
		.do_lock_srcdir	= 0,
		.isdir		= 0
	};

	LKTRTrace("type %d, %.*s\n", arg->type, AuDLNPair(dentry));
	IMustLock(dir);

	aufs_read_lock(dentry, AuLock_DW);
	parent = dentry->d_parent; /* dir inode is locked */
	di_write_lock_parent(parent);
	wh_dentry = lock_hdir_lkup_wh(dentry, &dt, /*src_dentry*/NULL,
				      &wr_dir_args);
	//wh_dentry = ERR_PTR(-1);
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out;

	h_dentry = au_h_dptr(dentry);
	h_parent = h_dentry->d_parent; /* dir inode is locked */
	h_dir = h_parent->d_inode;
	IMustLock(h_dir);
	sb = dir->i_sb;
	dlgt = au_need_dlgt(sb);

#if 1 // partial testing
	switch (arg->type) {
	case Creat:
		AuDebugOn(au_test_nfs(h_dir->i_sb) && !arg->u.c.nd);
		err = au_h_create(h_dir, h_dentry, arg->u.c.mode, dlgt,
				  arg->u.c.nd, au_nfsmnt(sb, dbstart(dentry)));
		break;
	case Symlink:
		err = vfsub_symlink(h_dir, h_dentry,
				    arg->u.s.symname, S_IALLUGO, dlgt);
		break;
	case Mknod:
		err = vfsub_mknod(h_dir, h_dentry,
				  arg->u.m.mode, arg->u.m.dev, dlgt);
		break;
	default:
		BUG();
	}
#else
	err = -1;
#endif
	created = !err;
	if (!err)
		err = epilog(dir, wh_dentry, dentry);
	//err = -1;

	/* revert */
	if (unlikely(created && err && h_dentry->d_inode)) {
		int rerr;
		vfsub_args_init(&vargs, NULL, dlgt, 0);
		rerr = vfsub_unlink(h_dir, h_dentry, &vargs);
		//rerr = -1;
		if (rerr) {
			AuIOErr("%.*s revert failure(%d, %d)\n",
				AuDLNPair(dentry), err, rerr);
			err = -EIO;
		}
		//todo: inotify will be fired to the grand parent dir
		au_dtime_revert(&dt);
		d_drop(dentry);
	}

	hdir_unlock(h_dir, dir, dbstart(dentry));
	dput(wh_dentry);

 out:
	if (unlikely(err)) {
		au_update_dbstart(dentry);
		d_drop(dentry);
	}
	di_write_unlock(parent);
	aufs_read_unlock(dentry, AuLock_DW);
	AuTraceErr(err);
	return err;
}

int aufs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	struct simple_arg arg = {
		.type = Mknod,
		.u.m = {
			.mode	= mode,
			.dev	= dev
		}
	};
	return add_simple(dir, dentry, &arg);
}

int aufs_symlink(struct inode *dir, struct dentry *dentry, const char *symname)
{
	struct simple_arg arg = {
		.type = Symlink,
		.u.s.symname = symname
	};
	return add_simple(dir, dentry, &arg);
}

int aufs_create(struct inode *dir, struct dentry *dentry, int mode,
		struct nameidata *nd)
{
	struct simple_arg arg = {
		.type = Creat,
		.u.c = {
			.mode	= mode,
			.nd	= nd
		}
	};
	return add_simple(dir, dentry, &arg);
}

/* ---------------------------------------------------------------------- */

struct link_arg {
	aufs_bindex_t bdst, bsrc;
	int issamedir, dlgt;
	struct dentry *src_parent, *parent, *h_dentry;
	struct inode *h_dir, *inode, *dir;
};

static int cpup_before_link(struct dentry *src_dentry, struct inode *dir,
			    struct link_arg *a)
{
	int err;
	struct inode *hi, *h_dir, *src_dir, *gdir;
	struct au_cpup_flags cflags;
	struct dentry *gparent;

	AuTraceEnter();

	gparent = NULL;
	gdir = NULL;
	if (unlikely(au_flag_test_udba_inotify(src_dentry->d_sb)
		     && !IS_ROOT(a->src_parent))) {
		gparent = dget_parent(a->src_parent);
		gdir = gparent->d_inode;
		if (gdir == dir) {
			dput(gparent);
			gparent = NULL;
		}
	}
	src_dir = a->src_parent->d_inode;
	h_dir = NULL;

	if (!a->issamedir) {
		/* this temporary unlock/lock is safe */
		hdir_unlock(a->h_dir, dir, a->bdst);
		di_read_lock_parent2(a->src_parent, AuLock_IR);
		err = au_test_and_cpup_dirs(src_dentry, a->bdst, a->parent);
		//err = -1;
		if (unlikely(err)) {
			hdir_lock(a->h_dir, dir, a->bdst);
			goto out;
		}

		//todo: meaningless lock if CONFIG_AUFS_DEBUG is disabled.
		if (unlikely(gparent))
			ii_read_lock_parent3(gdir);
		h_dir = au_h_iptr_i(src_dir, a->bdst);
		hdir_lock(h_dir, src_dir, a->bdst);
	} else if (unlikely(gparent)) {
		/* this temporary unlock/lock is safe */
		hdir_unlock(a->h_dir, dir, a->bdst);
		ii_read_lock_parent3(gdir);
		hdir_lock(a->h_dir, dir, a->bdst);
	}
	//todo: test parent-gparent relationship

	cflags.dtime = 1;
	hi = au_h_dptr(src_dentry)->d_inode;
	vfsub_i_lock_nested(hi, AuLsc_I_CHILD);
	err = au_sio_cpup_simple(src_dentry, a->bdst, -1, &cflags);
	//err = -1;
	vfsub_i_unlock(hi);

	if (unlikely(gparent)) {
		ii_read_unlock(gdir);
		dput(gparent);
	}

 out:
	if (h_dir) {
		hdir_unlock(h_dir, src_dir, a->bdst);
		hdir_lock(a->h_dir, dir, a->bdst);
	}
	if (!a->issamedir)
		di_read_unlock(a->src_parent, AuLock_IR);

	AuTraceErr(err);
	return err;
}

static int cpup_or_link(struct dentry *src_dentry, struct link_arg *a)
{
	int err;
	struct inode *inode, *h_inode, *h_dst_inode;
	struct dentry *h_dentry;
	aufs_bindex_t bstart;
	struct super_block *sb;
	struct au_cpup_flags cflags;

	AuTraceEnter();

	sb = src_dentry->d_sb;
	inode = src_dentry->d_inode;
	h_dentry = au_h_dptr(src_dentry);
	h_inode = h_dentry->d_inode;
	bstart = ibstart(inode);
	h_dst_inode = NULL;
	if (bstart <= a->bdst)
		h_dst_inode = au_h_iptr_i(inode, a->bdst);

	if (!h_dst_inode || !h_dst_inode->i_nlink) {
		/* copyup src_dentry as the name of dentry. */
		cflags.dtime = 0;
		set_dbstart(src_dentry, a->bdst);
		set_h_dptr(src_dentry, a->bdst, dget(a->h_dentry));
		vfsub_i_lock_nested(h_inode, AuLsc_I_CHILD);
		err = au_sio_cpup_single(src_dentry, a->bdst, a->bsrc, -1,
					 &cflags);
		//err = -1;
		vfsub_i_unlock(h_inode);
		set_h_dptr(src_dentry, a->bdst, NULL);
		set_dbstart(src_dentry, a->bsrc);
	} else {
		/* the inode of src_dentry already exists on a.bdst branch */
		h_dentry = d_find_alias(h_dst_inode);
		if (h_dentry) {
			err = vfsub_link(h_dentry, a->h_dir,
					 a->h_dentry, a->dlgt);
			dput(h_dentry);
		} else {
			AuIOErr("no dentry found for i%lu on b%d\n",
				h_dst_inode->i_ino, a->bdst);
			err = -EIO;
		}
	}

	if (!err)
		au_append_plink(sb, a->inode, a->h_dentry, a->bdst);

	AuTraceErr(err);
	return err;
}

int aufs_link(struct dentry *src_dentry, struct inode *dir,
	      struct dentry *dentry)
{
	int err, rerr;
	struct dentry *h_parent, *wh_dentry, *h_src_dentry;
	struct au_dtime dt;
	struct link_arg a;
	struct super_block *sb;
	struct vfsub_args vargs;
	struct au_wr_dir_args wr_dir_args = {
		//.force_btgt	= -1,
		.add_entry	= 1,
		//.do_lock_srcdir = 0,
		.isdir		= 0
	};

	LKTRTrace("src %.*s, i%lu, dst %.*s\n",
		  AuDLNPair(src_dentry), dir->i_ino, AuDLNPair(dentry));
	IMustLock(dir);
	IMustLock(src_dentry->d_inode);
	AuDebugOn(S_ISDIR(src_dentry->d_inode->i_mode));

	aufs_read_and_write_lock2(dentry, src_dentry, /*flags*/0);
	sb = dentry->d_sb;
	a.dir = dir;
	a.src_parent = dget_parent(src_dentry);
	a.parent = dentry->d_parent; /* dir inode is locked */
	a.issamedir = (a.src_parent == a.parent);
	wr_dir_args.do_lock_srcdir = !a.issamedir;
	wr_dir_args.force_btgt = dbstart(src_dentry);
	di_write_lock_parent(a.parent);
	wr_dir_args.force_btgt = au_wbr(dentry, wr_dir_args.force_btgt);
	wh_dentry = lock_hdir_lkup_wh(dentry, &dt, src_dentry, &wr_dir_args);
	//wh_dentry = ERR_PTR(-1);
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out;

	a.inode = src_dentry->d_inode;
	a.h_dentry = au_h_dptr(dentry);
	h_parent = a.h_dentry->d_parent; /* dir inode is locked */
	a.h_dir = h_parent->d_inode;
	IMustLock(a.h_dir);

	err = 0;
	a.dlgt = au_need_dlgt(sb);

	//todo: minor optimize, their sb may be same while their bindex differs.
	a.bsrc = dbstart(src_dentry);
	a.bdst = dbstart(dentry);
	h_src_dentry = au_h_dptr(src_dentry);
	if (unlikely(!AuFlag(stosi(sb), f_plink))) {
		/*
		 * copyup src_dentry to the branch we process,
		 * and then link(2) to it.
		 * gave up 'pseudo link by cpup' approach,
		 * since nlink may be one and some applications will not work.
		 */
		if (a.bdst < a.bsrc
		    /* && h_src_dentry->d_sb != a.h_dentry->d_sb */)
			err = cpup_before_link(src_dentry, dir, &a);
		if (!err) {
			h_src_dentry = au_h_dptr(src_dentry);
			err = vfsub_link(h_src_dentry, a.h_dir,
					 a.h_dentry, a.dlgt);
			//err = -1;
		}
	} else {
		if (a.bdst < a.bsrc
		    /* && h_src_dentry->d_sb != a.h_dentry->d_sb */)
			err = cpup_or_link(src_dentry, &a);
		else {
			h_src_dentry = au_h_dptr(src_dentry);
			err = vfsub_link(h_src_dentry, a.h_dir,
					 a.h_dentry, a.dlgt);
			//err = -1;
		}
	}
	if (unlikely(err))
		goto out_unlock;
	if (wh_dentry) {
		err = au_unlink_wh_dentry(a.h_dir, wh_dentry, dentry,
					  dir, /*dlgt*/0);
		//err = -1;
		if (unlikely(err))
			goto out_revert;
	}

#if 0 // cannot support it
	/* fuse has different memory inode for the same inode number */
	if (unlikely(au_test_fuse(a.h_dentry->d_sb))) {
		LKTRLabel(here);
		d_drop(a.h_dentry);
		//d_drop(h_src_dentry);
		//d_drop(src_dentry);
		a.inode->i_nlink++;
		a.inode->i_ctime = dir->i_ctime;
	}
#endif

	dir->i_version++;
	if (ibstart(dir) == dbstart(dentry))
		au_cpup_attr_timesizes(dir);
	if (!d_unhashed(a.h_dentry)
	    /* || h_old_inode->i_nlink <= nlink */
	    /* || SB_NFS(h_src_dentry->d_sb) */) {
		dentry->d_inode = igrab(a.inode);
		d_instantiate(dentry, a.inode);
		a.inode->i_nlink++;
		a.inode->i_ctime = dir->i_ctime;
	} else
		/* nfs case (< 2.6.15) */
		d_drop(dentry);
#if 0 // debug
	//au_debug_on();
	AuDbgInode(a.inode);
	//au_debug_off();
	{
		aufs_bindex_t i;
		for (i = ibstart(a.inode); i <= ibend(a.inode); i++) {
			struct xino_entry xinoe;
			struct inode *hi;
			hi = au_h_iptr_i(a.inode, i);
			if (hi) {
				xino_read(sb, i, hi->i_ino, &xinoe);
				AuDbg("hi%lu, i%lu\n", hi->i_ino, xinoe.ino);
			}
		}
	}
#endif
	goto out_unlock; /* success */

 out_revert:
#if 0 // remove
	if (d_unhashed(a.h_dentry)) {
		/* hardlink on nfs (< 2.6.15) */
		struct dentry *d;
		const struct qstr *name = &a.h_dentry->d_name;
		AuDebugOn(a.h_dentry->d_parent->d_inode != a.h_dir);
		/* do not superio. */
		d = au_lkup_one(name->name, a.h_dentry->d_parent,
				name->len, au_nfsmnt(sb, a.bdst)??,
				au_need_dlgt(sb));
		rerr = PTR_ERR(d);
		if (IS_ERR(d))
			goto out_rerr;
		dput(a.h_dentry);
		a.h_dentry = d;
		AuDebugOn(!d->d_inode);
	}
#endif
	vfsub_args_init(&vargs, NULL, a.dlgt, 0);
	rerr = vfsub_unlink(a.h_dir, a.h_dentry, &vargs);
	//rerr = -1;
	if (!rerr)
		goto out_dt;
// out_rerr:
	AuIOErr("%.*s reverting failed(%d, %d)\n",
		AuDLNPair(dentry), err, rerr);
	err = -EIO;
 out_dt:
	d_drop(dentry);
	au_dtime_revert(&dt);
 out_unlock:
	hdir_unlock(a.h_dir, dir, a.bdst);
	dput(wh_dentry);
 out:
	if (unlikely(err)) {
		au_update_dbstart(dentry);
		d_drop(dentry);
	}
	di_write_unlock(a.parent);
	dput(a.src_parent);
	aufs_read_and_write_unlock2(dentry, src_dentry);
	AuTraceErr(err);
	return err;
}

int aufs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	int err, rerr, diropq, dlgt;
	struct dentry *h_dentry, *h_parent, *wh_dentry, *parent,
		*opq_dentry;
	struct inode *h_dir, *h_inode;
	struct au_dtime dt;
	aufs_bindex_t bindex;
	struct super_block *sb;
	struct vfsub_args vargs;
	struct au_wr_dir_args wr_dir_args = {
		.force_btgt	= -1,
		.add_entry	= 1,
		.do_lock_srcdir = 0,
		.isdir		= 1
	};

	LKTRTrace("i%lu, %.*s, mode 0%o\n",
		  dir->i_ino, AuDLNPair(dentry), mode);
	IMustLock(dir);

	aufs_read_lock(dentry, AuLock_DW);
	parent = dentry->d_parent; /* dir inode is locked */
	di_write_lock_parent(parent);
	wh_dentry = lock_hdir_lkup_wh(dentry, &dt, /*src_dentry*/NULL,
				      &wr_dir_args);
	//wh_dentry = ERR_PTR(-1);
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out;

	sb = dentry->d_sb;
	bindex = dbstart(dentry);
	h_dentry = au_h_dptr(dentry);
	h_parent = h_dentry->d_parent; /* dir inode is locked */
	h_dir = h_parent->d_inode;
	IMustLock(h_dir);
	dlgt = au_need_dlgt(sb);

	err = vfsub_mkdir(h_dir, h_dentry, mode, dlgt);
	//err = -1;
	if (unlikely(err))
		goto out_unlock;
	h_inode = h_dentry->d_inode;

	/* make the dir opaque */
	diropq = 0;
	if (unlikely(wh_dentry || AuFlag(stosi(sb), f_always_diropq))) {
		vfsub_i_lock_nested(h_inode, AuLsc_I_CHILD);
		opq_dentry = create_diropq(dentry, bindex, /*dlgt*/0);
		//opq_dentry = ERR_PTR(-1);
		vfsub_i_unlock(h_inode);
		err = PTR_ERR(opq_dentry);
		if (IS_ERR(opq_dentry))
			goto out_dir;
		dput(opq_dentry);
		diropq = 1;
	}

	err = epilog(dir, wh_dentry, dentry);
	//err = -1;
	if (!err) {
		dir->i_nlink++;
		goto out_unlock; /* success */
	}

	/* revert */
	if (unlikely(diropq)) {
		LKTRLabel(revert opq);
		vfsub_i_lock_nested(h_inode, AuLsc_I_CHILD);
		rerr = remove_diropq(dentry, bindex, dlgt);
		//rerr = -1;
		vfsub_i_unlock(h_inode);
		if (rerr) {
			AuIOErr("%.*s reverting diropq failed(%d, %d)\n",
				AuDLNPair(dentry), err, rerr);
			err = -EIO;
		}
	}

 out_dir:
	LKTRLabel(revert dir);
	vfsub_args_init(&vargs, NULL, dlgt, 0);
	rerr = vfsub_rmdir(h_dir, h_dentry, &vargs);
	//rerr = -1;
	if (rerr) {
		AuIOErr("%.*s reverting dir failed(%d, %d)\n",
			AuDLNPair(dentry), err, rerr);
		err = -EIO;
	}
	d_drop(dentry);
	au_dtime_revert(&dt);
 out_unlock:
	hdir_unlock(h_dir, dir, bindex);
	dput(wh_dentry);
 out:
	if (unlikely(err)) {
		au_update_dbstart(dentry);
		d_drop(dentry);
	}
	di_write_unlock(parent);
	aufs_read_unlock(dentry, AuLock_DW);
	AuTraceErr(err);
	return err;
}

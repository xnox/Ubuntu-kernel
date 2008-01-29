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

/* $Id: hinotify.c,v 1.39 2008/01/21 04:57:48 sfjro Exp $ */

#include "aufs.h"

/* inotify events */
static const __u32 AuInMask = (IN_MOVE | IN_DELETE | IN_CREATE
			       /* | IN_ACCESS */
			       | IN_MODIFY | IN_ATTRIB
			       | IN_DELETE_SELF | IN_MOVE_SELF);
static struct inotify_handle *in_handle;

int au_hin_alloc(struct aufs_hinode *hinode, struct inode *inode,
		 struct inode *hidden_inode)
{
	int err, i;
	struct aufs_hinotify *hin;
	s32 wd;

	LKTRTrace("i%lu, hi%lu\n", inode->i_ino, hidden_inode->i_ino);

	err = -ENOMEM;
	hin = au_cache_alloc_hinotify();
	if (hin) {
		AuDebugOn(hinode->hi_notify);
		hinode->hi_notify = hin;
		hin->hin_aufs_inode = inode;
		for (i = 0; i < au_hin_nignore; i++)
			atomic_set(hin->hin_ignore + i, 0);

		inotify_init_watch(&hin->hin_watch);
		wd = inotify_add_watch(in_handle, &hin->hin_watch, hidden_inode,
				       AuInMask);
		if (wd >= 0)
			return 0; /* success */

		err = wd;
		put_inotify_watch(&hin->hin_watch);
		au_cache_free_hinotify(hin);
		hinode->hi_notify = NULL;
	}

	AuTraceErr(err);
	return err;
}

void au_hin_free(struct aufs_hinode *hinode)
{
	int err;
	struct aufs_hinotify *hin;

	AuTraceEnter();

	hin = hinode->hi_notify;
	if (unlikely(hin)) {
		err = 0;
		if (atomic_read(&hin->hin_watch.count))
			err = inotify_rm_watch(in_handle, &hin->hin_watch);

		if (!err) {
			au_cache_free_hinotify(hin);
			hinode->hi_notify = NULL;
		} else
			AuIOErr1("failed inotify_rm_watch() %d\n", err);
	}
}

/* ---------------------------------------------------------------------- */

static void ctl_hinotify(struct aufs_hinode *hinode, const __u32 mask)
{
	struct inode *h_inode;
	struct inotify_watch *watch;

	h_inode = hinode->hi_inode;
	LKTRTrace("hi%lu, sb %p, 0x%x\n", h_inode->i_ino, h_inode->i_sb, mask);
	IMustLock(h_inode);
	if (!hinode->hi_notify)
		return;

	watch = &hinode->hi_notify->hin_watch;
#if 0 // temp
	{
		u32 wd;
		wd = inotify_find_update_watch(in_handle, h_inode, mask);
		AuTraceErr(wd);
		/* ignore an err; */
	}
#else
	/* struct inotify_handle is hidden */
	mutex_lock(&h_inode->inotify_mutex);
	//mutex_lock(&watch->ih->mutex);
	watch->mask = mask;
	//mutex_unlock(&watch->ih->mutex);
	mutex_unlock(&h_inode->inotify_mutex);
#endif
	LKTRTrace("watch %p, mask %u\n", watch, watch->mask);
}

#define suspend_hinotify(hi)	ctl_hinotify(hi, 0)
#define resume_hinotify(hi)	ctl_hinotify(hi, AuInMask)

void au_do_hdir_lock(struct inode *h_dir, struct inode *dir,
		     aufs_bindex_t bindex, unsigned int lsc)
{
	struct aufs_hinode *hinode;

	LKTRTrace("i%lu, b%d, lsc %d\n", dir->i_ino, bindex, lsc);
	AuDebugOn(!S_ISDIR(dir->i_mode));
	hinode = itoii(dir)->ii_hinode + bindex;
	AuDebugOn(h_dir != hinode->hi_inode);

	vfsub_i_lock_nested(h_dir, lsc);
	suspend_hinotify(hinode);
}

void hdir_unlock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex)
{
	struct aufs_hinode *hinode;

	LKTRTrace("i%lu, b%d\n", dir->i_ino, bindex);
	AuDebugOn(!S_ISDIR(dir->i_mode));
	hinode = itoii(dir)->ii_hinode + bindex;
	AuDebugOn(h_dir != hinode->hi_inode);

	resume_hinotify(hinode);
	vfsub_i_unlock(h_dir);
}

void hdir_lock_rename(struct dentry **h_parents, struct inode **dirs,
		      aufs_bindex_t bindex, int issamedir)
{
	struct aufs_hinode *hinode;

	LKTRTrace("%.*s, %.*s\n",
		  AuDLNPair(h_parents[0]), AuDLNPair(h_parents[1]));

	vfsub_lock_rename(h_parents[0], h_parents[1]);
	hinode = itoii(dirs[0])->ii_hinode + bindex;
	AuDebugOn(h_parents[0]->d_inode != hinode->hi_inode);
	suspend_hinotify(hinode);
	if (issamedir)
		return;
	hinode = itoii(dirs[1])->ii_hinode + bindex;
	AuDebugOn(h_parents[1]->d_inode != hinode->hi_inode);
	suspend_hinotify(hinode);
}

void hdir_unlock_rename(struct dentry **h_parents, struct inode **dirs,
			aufs_bindex_t bindex, int issamedir)
{
	struct aufs_hinode *hinode;

	LKTRTrace("%.*s, %.*s\n",
		  AuDLNPair(h_parents[0]), AuDLNPair(h_parents[1]));

	hinode = itoii(dirs[0])->ii_hinode + bindex;
	AuDebugOn(h_parents[0]->d_inode != hinode->hi_inode);
	resume_hinotify(hinode);
	if (!issamedir) {
		hinode = itoii(dirs[1])->ii_hinode + bindex;
		AuDebugOn(h_parents[1]->d_inode != hinode->hi_inode);
		resume_hinotify(hinode);
	}
	vfsub_unlock_rename(h_parents[0], h_parents[1]);
}

void au_reset_hinotify(struct inode *inode, unsigned int flags)
{
	aufs_bindex_t bindex, bend;
	struct inode *hi;
	struct dentry *iwhdentry;

	LKTRTrace("i%lu, 0x%x\n", inode->i_ino, flags);

	bend = ibend(inode);
	for (bindex = ibstart(inode); bindex <= bend; bindex++) {
		hi = au_h_iptr_i(inode, bindex);
		if (hi) {
			//vfsub_i_lock_nested(hi, AuLsc_I_CHILD);
			iwhdentry = au_hi_wh(inode, bindex);
			if (unlikely(iwhdentry))
				dget(iwhdentry);
			igrab(hi);
			set_h_iptr(inode, bindex, NULL, 0);
			set_h_iptr(inode, bindex, igrab(hi),
				   flags & ~AUFS_HI_XINO);
			iput(hi);
			dput(iwhdentry);
			//vfsub_i_unlock(hi);
		}
	}
}

/* ---------------------------------------------------------------------- */

void au_hin_ignore(struct aufs_hinode *hinode, __u32 events)
{
	int i;
	atomic_t *ign;

	AuDebugOn(!hinode || !events);
	LKTRTrace("hi%lu, 0x%x\n", hinode->hi_inode->i_ino, events);
#ifdef DbgInotify
	AuDbg("hi%lu, 0x%x\n", hinode->hi_inode->i_ino, events);
#endif
	AuDebugOn(!hinode->hi_notify);

	ign = hinode->hi_notify->hin_ignore;
	for (i = 0; i < au_hin_nignore; i++)
		if (1U << i & events)
			atomic_inc_return(ign + i);
}

void au_hin_unignore(struct aufs_hinode *hinode, __u32 events)
{
	int i;
	atomic_t *ign;

	AuDebugOn(!hinode || !events);
	LKTRTrace("hi%lu, 0x%x\n", hinode->hi_inode->i_ino, events);
#ifdef DbgInotify
	AuDbg("hi%lu, 0x%x\n", hinode->hi_inode->i_ino, events);
#endif
	AuDebugOn(!hinode->hi_notify);

	ign = hinode->hi_notify->hin_ignore;
	for (i = 0; i < au_hin_nignore; i++)
		if (1U << i & events)
			atomic_dec_return(ign + i);
}

/* ---------------------------------------------------------------------- */

static char *in_name(u32 mask)
{
#ifdef CONFIG_AUFS_DEBUG
#define test_ret(flag)	if (mask & flag) return #flag;
	test_ret(IN_ACCESS);
	test_ret(IN_MODIFY);
	test_ret(IN_ATTRIB);
	test_ret(IN_CLOSE_WRITE);
	test_ret(IN_CLOSE_NOWRITE);
	test_ret(IN_OPEN);
	test_ret(IN_MOVED_FROM);
	test_ret(IN_MOVED_TO);
	test_ret(IN_CREATE);
	test_ret(IN_DELETE);
	test_ret(IN_DELETE_SELF);
	test_ret(IN_MOVE_SELF);
	test_ret(IN_UNMOUNT);
	test_ret(IN_Q_OVERFLOW);
	test_ret(IN_IGNORED);
	return "";
#undef test_ret
#else
	return "??";
#endif
}

/* ---------------------------------------------------------------------- */

static struct dentry *lookup_wlock_by_name(char *name, unsigned int nlen,
					   struct inode *dir)
{
	struct dentry *dentry, *d, *parent;
	struct qstr *dname;

	LKTRTrace("%.*s, dir%lu\n", nlen, name, dir->i_ino);

	parent = d_find_alias(dir);
	if (!parent)
		return NULL;

	dentry = NULL;
	spin_lock(&dcache_lock);
	list_for_each_entry(d, &parent->d_subdirs, d_u.d_child) {
		LKTRTrace("%.*s\n", AuDLNPair(d));
		dname = &d->d_name;
		if (dname->len != nlen
		    || memcmp(dname->name, name, nlen))
			continue;
		if (!atomic_read(&d->d_count)) {
			spin_lock(&d->d_lock);
			__d_drop(d);
			spin_unlock(&d->d_lock);
			continue;
		}

		dentry = dget(d);
		break;
	}
	spin_unlock(&dcache_lock);
	dput(parent);

	if (dentry)
		di_write_lock_child(dentry);
	return dentry;
}

static struct inode *lookup_wlock_by_ino(struct super_block *sb,
					 aufs_bindex_t bindex, ino_t h_ino)
{
	struct inode *inode;
	struct xino_entry xinoe;
	int err;

	LKTRTrace("b%d, hi%lu\n", bindex, h_ino);
	AuDebugOn(AuFlag(stosi(sb), f_xino) == AuXino_NONE);

	inode = NULL;
	err = xino_read(sb, bindex, h_ino, &xinoe);
	if (!err && xinoe.ino)
		inode = ilookup(sb, xinoe.ino);
	if (!inode)
		goto out;
	if (unlikely(inode->i_ino == AUFS_ROOT_INO)) {
		AuWarn("wrong root branch\n");
		iput(inode);
		inode = NULL;
		goto out;
	}

	ii_write_lock_child(inode);
#if 0 // debug
	if (au_iigen(inode) == au_sigen(sb))
		goto out; /* success */

	err = au_refresh_hinode_self(inode);
	if (!err)
		goto out; /* success */

	AuIOErr1("err %d ignored, but ino will be broken\n", err);
	ii_write_unlock(inode);
	iput(inode);
	inode = NULL;
#endif

 out:
	return inode;
}

static int hin_xino(struct inode *inode, struct inode *h_inode)
{
	int err;
	aufs_bindex_t bindex, bend, bfound;

	LKTRTrace("i%lu, hi%lu\n", inode->i_ino, h_inode->i_ino);

	err = 0;
	if (unlikely(inode->i_ino == AUFS_ROOT_INO)) {
		AuWarn("branch root dir was changed\n");
		goto out;
	}

	bfound = -1;
	bend = ibend(inode);
	for (bindex = ibstart(inode); bindex <= bend; bindex++) {
		if (au_h_iptr_i(inode, bindex) == h_inode) {
			bfound = bindex;
			break;
		}
	}
	if (bfound < 0)
		goto out;

	err = xino_write0(inode->i_sb, bindex, h_inode->i_ino, 0);
	/* ignore this error */
	/* bad action? */

	/* children inode number will be broken */

 out:
	AuTraceErr(err);
	return err;
}

static int hin_iigen(struct inode *inode)
{
	LKTRTrace("i%lu\n", inode->i_ino);

	if (inode->i_ino != AUFS_ROOT_INO)
		au_iigen_dec(inode);
	else
		AuWarn("branch root dir was changed\n");
	return 0;
}

static int hin_digen_tree(struct dentry *dentry, int dec_iigen)
{
	int err, i, j, ndentry;
	struct au_dcsub_pages dpages;
	struct au_dpage *dpage;
	struct dentry **dentries;

	LKTRTrace("%.*s, iigen %d\n", AuDLNPair(dentry), dec_iigen);

	err = au_dpages_init(&dpages, GFP_TEMPORARY);
	if (unlikely(err))
		goto out;
	err = au_dcsub_pages(&dpages, dentry, NULL, NULL);
	if (unlikely(err))
		goto out_dpages;

	for (i = 0; i < dpages.ndpage; i++) {
		dpage = dpages.dpages + i;
		dentries = dpage->dentries;
		ndentry = dpage->ndentry;
		for (j = 0; j < ndentry; j++) {
			struct dentry *d;
			d = dentries[j];
			LKTRTrace("%.*s\n", AuDLNPair(d));
			if (IS_ROOT(d))
				continue;
			d_drop(d);
			au_digen_dec(d);
			if (dec_iigen && d->d_inode)
				//reset children xino? cached children only?
				hin_iigen(d->d_inode);
		}
	}

 out_dpages:
	au_dpages_free(&dpages);

	/* discard children */
	dentry_unhash(dentry);
	dput(dentry);
 out:
	AuTraceErr(err);
	return err;
}

/*
 * return 0 if processed.
 */
static int hin_digen_by_inode(char *name, unsigned int nlen,
			      struct inode *inode, int dec_iigen)
{
	int err;
	struct dentry *d;
	struct qstr *dname;

	LKTRTrace("%.*s, i%lu, iigen %d\n",
		  nlen, name, inode->i_ino, dec_iigen);

	err = 1;
	if (unlikely(inode->i_ino == AUFS_ROOT_INO)) {
		AuWarn("branch root dir was changed\n");
		err = 0;
		goto out;
	}

	if (!S_ISDIR(inode->i_mode)) {
		AuDebugOn(!name);
		spin_lock(&dcache_lock);
		list_for_each_entry(d, &inode->i_dentry, d_alias) {
			dname = &d->d_name;
			if (dname->len != nlen
			    && memcmp(dname->name, name, nlen))
				continue;
			err = 0;
			spin_lock(&d->d_lock);
			__d_drop(d);
			au_digen_dec(d);
			spin_unlock(&d->d_lock);
			break;
		}
		spin_unlock(&dcache_lock);
	} else {
		d = d_find_alias(inode);
		if (d) {
			dname = &d->d_name;
			if (dname->len == nlen
			    && !memcmp(dname->name, name, nlen))
				err = hin_digen_tree(d, dec_iigen);
			dput(d);
		}
	}

 out:
	AuTraceErr(err);
	return err;
}

static int hin_digen_by_name(struct dentry *dentry, int dec_iigen)
{
	struct inode *inode;

	LKTRTrace("%.*s, iigen %d\n", AuDLNPair(dentry), dec_iigen);

	if (IS_ROOT(dentry)) {
		AuWarn("branch root dir was changed\n");
		return 0;
	}

	inode = dentry->d_inode;
	if (!inode) {
		d_drop(dentry);
		au_digen_dec(dentry);
	} else if (unlikely(inode->i_ino == AUFS_ROOT_INO))
		AuWarn("branch root dir was changed\n");
	else {
		if (!S_ISDIR(inode->i_mode)) {
			au_digen_dec(dentry);
			d_drop(dentry);
		} else
			hin_digen_tree(dentry, dec_iigen);
	}

	return 0;
}

/* ---------------------------------------------------------------------- */

union hin_job {
	unsigned int flags;
	struct {
		unsigned int xino0:1;
		unsigned int iigen:1;
		unsigned int digen:1;
		unsigned int dirent:1;
		unsigned int attr:1;
	};
};

struct hin_job_args {
	union hin_job *jobs;
	struct inode *inode, *h_inode, *dir, *h_dir;
	struct dentry *dentry;
	char *h_name;
	int h_nlen;
};

static int hin_job(struct hin_job_args *a)
{
	/* reset xino */
	if (a->inode && a->jobs->xino0)
		hin_xino(a->inode, a->h_inode);
	/* ignore this error */

	/* make the generation obsolete */
	if (a->jobs->iigen && a->inode)
		hin_iigen(a->inode);
	/* ignore this error */

	if (a->jobs->digen) {
		int err;
		err = -1;
		if (a->inode)
			err = hin_digen_by_inode(a->h_name, a->h_nlen, a->inode,
						 a->jobs->iigen);
		if (err && a->dentry)
			hin_digen_by_name(a->dentry, a->jobs->iigen);
		/* ignore this error */
	}

	/* make dir entries obsolete */
	if (a->jobs->dirent) {
		struct aufs_vdir *vdir;
		vdir = ivdir(a->inode);
		if (vdir)
			vdir->vd_jiffy = 0;
		IMustLock(a->inode);
		a->inode->i_version++;
	}

	/* update the attr */
	if (a->jobs->attr
	    && a->inode
	    && au_h_iptr(a->inode) == a->h_inode)
		au_cpup_attr_all(a->inode);
	return 0;
}

/* ---------------------------------------------------------------------- */

enum { CHILD, PARENT };
struct postproc_args {
	struct inode *h_dir, *dir, *h_child_inode;
	u32 mask;
	union hin_job jobs[2];
	unsigned int h_child_nlen;
	char h_child_name[];
};

static void postproc(void *_args)
{
	struct postproc_args *a = _args;
	struct super_block *sb;
	aufs_bindex_t bindex, bend, bfound;
	int xino, err;
	struct inode *inode;
	ino_t h_ino;
	struct hin_job_args args;
	struct dentry *dentry;
	struct aufs_sbinfo *sbinfo;

	//au_debug_on();
	LKTRTrace("mask 0x%x %s, i%lu, hi%lu, hci%lu\n",
		  a->mask, in_name(a->mask), a->dir->i_ino, a->h_dir->i_ino,
		  a->h_child_inode ? a->h_child_inode->i_ino : 0);

	inode = NULL;
	dentry = NULL;
	vfsub_i_lock(a->dir);
	sb = a->dir->i_sb;
	sbinfo = stosi(sb);
	si_read_lock(sb, !AuLock_FLUSH);

	ii_read_lock_parent(a->dir);
	bfound = -1;
	bend = ibend(a->dir);
	for (bindex = ibstart(a->dir); bindex <= bend; bindex++)
		if (au_h_iptr_i(a->dir, bindex) == a->h_dir) {
			bfound = bindex;
			break;
		}
	ii_read_unlock(a->dir);
	if (unlikely(bfound < 0))
		goto out;

	xino = (AuFlag(sbinfo, f_xino) != AuXino_NONE);
	h_ino = 0;
	if (a->h_child_inode)
		h_ino = a->h_child_inode->i_ino;

	if (a->h_child_nlen && a->jobs[CHILD].digen)
		dentry = lookup_wlock_by_name(a->h_child_name, a->h_child_nlen,
					      a->dir);
	if (dentry)
		inode = dentry->d_inode;
	if (xino && !inode && h_ino
	    && (a->jobs[CHILD].xino0
		|| a->jobs[CHILD].iigen
		|| a->jobs[CHILD].digen
		|| a->jobs[CHILD].attr))
		inode = lookup_wlock_by_ino(sb, bfound, h_ino);

	args.jobs = a->jobs + CHILD;
	args.dentry = dentry;
	args.inode = inode;
	args.h_inode = a->h_child_inode;
	args.dir = a->dir;
	args.h_dir = a->h_dir;
	args.h_name = a->h_child_name;
	args.h_nlen = a->h_child_nlen;
	err = hin_job(&args);
	if (dentry) {
		di_write_unlock(dentry);
		dput(dentry);
	} else if (inode) {
		ii_write_unlock(inode);
		iput(inode);
	}

	ii_write_lock_parent(a->dir);
	args.jobs = a->jobs + PARENT;
	args.dentry = NULL;
	args.inode = a->dir;
	args.h_inode = a->h_dir;
	args.dir = NULL;
	args.h_dir = NULL;
	args.h_name = NULL;
	args.h_nlen = 0;
	err = hin_job(&args);
	ii_write_unlock(a->dir);

 out:
	si_read_unlock(sb);
	vfsub_i_unlock(a->dir);
	au_nwt_dec(&sbinfo->si_nowait);

	iput(a->h_child_inode);
	iput(a->h_dir);
	iput(a->dir);
	kfree(a);
	//au_debug_off();
}

//todo: endian?
#ifndef ilog2
#define ilog2(n) ffz(~(n))
#endif

static void aufs_inotify(struct inotify_watch *watch, u32 wd, u32 mask,
			 u32 cookie, const char *h_child_name,
			 struct inode *h_child_inode)
{
	struct aufs_hinotify *hinotify;
	struct postproc_args *args;
	int len, wkq_err, isdir, isroot, wh, idx;
	char *p;
	struct inode *dir;
	union hin_job jobs[2];
	struct super_block *sb;
	atomic_t *cnt;

	LKTRTrace("i%lu, wd %d, mask 0x%x %s, cookie 0x%x, hcname %s, hi%lu\n",
		  watch->inode->i_ino, wd, mask, in_name(mask), cookie,
		  h_child_name ? h_child_name : "",
		  h_child_inode ? h_child_inode->i_ino : 0);
#if 0 //defined(ForceInotify) || defined(DbgInotify)
	AuDbg("i%lu, wd %d, mask 0x%x %s, cookie 0x%x, hcname %s, hi%lu\n",
	      watch->inode->i_ino, wd, mask, in_name(mask), cookie,
	      h_child_name ? h_child_name : "",
	      h_child_inode ? h_child_inode->i_ino : 0);
#endif
	/* if IN_UNMOUNT happens, there must be another bug */
	if (mask & (IN_IGNORED | IN_UNMOUNT)) {
		//WARN_ON(watch->inode->i_ino == 15);
		put_inotify_watch(watch);
		return;
	}

#ifdef DbgInotify
	if (!h_child_name || strcmp(h_child_name, AUFS_XINO_FNAME))
		AuDbg("i%lu, wd %d, mask 0x%x %s, cookie 0x%x, hcname %s,"
		      " hi%lu\n",
		      watch->inode->i_ino, wd, mask, in_name(mask), cookie,
		      h_child_name ? h_child_name : "",
		      h_child_inode ? h_child_inode->i_ino : 0);
	//WARN_ON(1);
#endif

	hinotify = container_of(watch, struct aufs_hinotify, hin_watch);
	AuDebugOn(!hinotify || !hinotify->hin_aufs_inode);
	idx = ilog2(mask & IN_ALL_EVENTS);
	AuDebugOn(au_hin_nignore <= idx);
	cnt = hinotify->hin_ignore + idx;
	if (0 <= atomic_dec_return(cnt))
		return;
	atomic_inc_return(cnt);
#ifdef DbgInotify
#if 0
	AuDbg("i%lu, wd %d, mask 0x%x %s, cookie 0x%x, hcname %s, hi%lu\n",
	      watch->inode->i_ino, wd, mask, in_name(mask), cookie,
	      h_child_name ? h_child_name : "",
	      h_child_inode ? h_child_inode->i_ino : 0);
#endif
#if 0
	if (!h_child_name || strcmp(h_child_name, AUFS_XINO_FNAME))
		WARN_ON(1);
#endif
#endif

	dir = hinotify->hin_aufs_inode;
	isroot = (dir->i_ino == AUFS_ROOT_INO);
	len = 0;
	wh = 0;
	if (h_child_name) {
		len = strlen(h_child_name);
		if (unlikely(!memcmp(h_child_name, AUFS_WH_PFX,
				     AUFS_WH_PFX_LEN))) {
			h_child_name += AUFS_WH_PFX_LEN;
			len -= AUFS_WH_PFX_LEN;
			wh = 1;
		}
	}

	isdir = 0;
	if (h_child_inode)
		isdir = !!S_ISDIR(h_child_inode->i_mode);
	jobs[PARENT].flags = 0;
	jobs[CHILD].flags = 0;
	switch (mask & IN_ALL_EVENTS) {
	case IN_MODIFY:
		/*FALLTHROUGH*/
	case IN_ATTRIB:
		if (h_child_inode) {
			if (!wh)
				jobs[CHILD].attr = 1;
		} else
			jobs[PARENT].attr = 1;
		break;

		/* IN_MOVED_FROM is the first event in rename(2) */
	case IN_MOVED_FROM:
	case IN_MOVED_TO:
		AuDebugOn(!h_child_name || !h_child_inode);
		jobs[CHILD].iigen = 1;
		jobs[CHILD].attr = 1;
		jobs[CHILD].xino0 = 1;//!!isdir;
		jobs[CHILD].digen = 1;
		jobs[PARENT].attr = 1;
		jobs[PARENT].dirent = 1;
		break;

	case IN_CREATE:
		AuDebugOn(!h_child_name || !h_child_inode);
		jobs[PARENT].attr = 1;
		jobs[PARENT].dirent = 1;
		jobs[CHILD].digen = 1;
		jobs[CHILD].iigen = 1;
		/* hard link */
		jobs[CHILD].attr = (!isdir && h_child_inode->i_nlink > 1);
		break;

	case IN_DELETE:
		/*
		 * aufs never be able to get this child inode.
		 * revalidation should be in d_revalidate()
		 * by checking i_nlink, i_generation or d_unhashed().
		 */
		AuDebugOn(!h_child_name);
		jobs[PARENT].attr = 1;
		jobs[PARENT].dirent = 1;
		jobs[CHILD].iigen = 1;
		jobs[CHILD].digen = 1;
		break;

	case IN_DELETE_SELF:
		jobs[PARENT].iigen = !isroot;
		/*FALLTHROUGH*/

	case IN_MOVE_SELF:
		AuDebugOn(h_child_name || h_child_inode);
		if (unlikely(isroot)) {
			AuWarn("root branch was moved\n");
			return;
		}
#if 1
		return;
#else
		jobs[PARENT].xino0 = !isroot;
		jobs[PARENT].iigen = !isroot;
		jobs[PARENT].digen = !isroot;
		jobs[PARENT].attr = !isroot;
		jobs[PARENT].dirent = !isroot;
		break;
#endif
	case IN_ACCESS:
	default:
		AuDebugOn(1);
	}

#if 0 //def DbgInotify
	WARN_ON(1);
#endif

	if (wh)
		h_child_inode = NULL;

	/* iput() and kfree() will be called in postproc() */
	args = kmalloc(sizeof(*args) + len + 1, GFP_TEMPORARY);
	if (unlikely(!args)) {
		AuErr1("no memory\n");
		return;
	}
	memcpy(args->jobs, jobs, sizeof(jobs));
	args->mask = mask;
	args->dir = igrab(dir);
	args->h_dir = igrab(watch->inode);
	if (h_child_inode)
		igrab(h_child_inode);
	args->h_child_inode = h_child_inode;
	args->h_child_nlen = len;
	if (len) {
		p = (void *)args;
		p += sizeof(*args);
		memcpy(p, h_child_name, len + 1);
	}

	sb = dir->i_sb;
	au_nwt_inc(&stosi(sb)->si_nowait);
	wkq_err = au_wkq_nowait(postproc, args, sb, /*dlgt*/0);
	if (unlikely(wkq_err)) {
		AuErr("wkq %d\n", wkq_err);
		au_nwt_dec(&stosi(sb)->si_nowait);
	}
}

static void aufs_inotify_destroy(struct inotify_watch *watch)
{
	return;
}

static struct inotify_operations aufs_inotify_ops = {
	.handle_event	= aufs_inotify,
	.destroy_watch	= aufs_inotify_destroy
};

/* ---------------------------------------------------------------------- */

int __init au_inotify_init(void)
{
	au_hin_nignore = 6;
	while (1U << au_hin_nignore < AuInMask)
		au_hin_nignore++;
	//AuDbg("au_hin_nignore %d\n", au_hin_nignore);
	AuDebugOn(au_hin_nignore != 12);

	in_handle = inotify_init(&aufs_inotify_ops);
	if (!IS_ERR(in_handle))
		return 0;
	AuTraceErrPtr(in_handle);
	return PTR_ERR(in_handle);
}

void au_inotify_fin(void)
{
	inotify_destroy(in_handle);
}

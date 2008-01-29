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

/* $Id: export.c,v 1.23 2008/01/21 04:57:48 sfjro Exp $ */

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
#include <linux/exportfs.h>
#endif
#include "aufs.h"

/* ---------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
static struct dentry *
au_call_decode_fh(struct vfsmount *h_mnt, __u32 *fh, int fh_len, int fh_type,
		  int (*acceptable)(void *, struct dentry *), void *context)
{
	/* in linux-2.6.24, it takes struct fid * as file handle */
	return exportfs_decode_fh(h_mnt, (void *)fh, fh_len, fh_type,
				  acceptable, context);
}

static int
au_call_encode_fh(struct dentry *h_dentry, __u32 *fh, int *max_len,
		  int connectable)
{
	/* in linux-2.6.24, it takes struct fid * as file handle */
	return exportfs_encode_fh(h_dentry, (void *)fh, max_len, connectable);
}
#else
extern struct export_operations export_op_default;
#define CALL(ops, func) \
	(((ops)->func) ? ((ops)->func) : export_op_default.func)

static struct dentry *
au_call_decode_fh(struct vfsmount *h_mnt, __u32 *fh, int fh_len, int fh_type,
		  int (*acceptable)(void *, struct dentry *), void *context)
{
	return CALL(h_mnt->mnt_sb->s_export_op, decode_fh)
		(h_mnt->mnt_sb, fh, fh_len, fh_type, acceptable, context);
}

static int
au_call_encode_fh(struct dentry *h_dentry, __u32 *fh, int *max_len,
		  int connectable)
{
	return CALL(h_dentry->d_sb->s_export_op, encode_fh)
		(h_dentry, fh, max_len, connectable);
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23) */

#define AuTestAnon(d)	((d)->d_flags & DCACHE_DISCONNECTED)

/* ---------------------------------------------------------------------- */

union conv {
#if BITS_PER_LONG == 32
	__u32 a[1];
#else
	__u32 a[2];
#endif
	ino_t ino;
};

static ino_t decode_ino(__u32 *a)
{
	union conv u;
	u.a[0] = a[0];
#if BITS_PER_LONG == 64
	u.a[1] = a[1];
#endif
	return u.ino;
}

static void encode_ino(__u32 *a, ino_t ino)
{
	union conv u;
	u.ino = ino;
	a[0] = u.a[0];
#if BITS_PER_LONG == 64
	a[1] = u.a[1];
#endif
}

/* NFS file handle */
enum {
	/* support 64bit inode number */
	/* but untested */
	Fh_br_id,
	Fh_sigen,
	Fh_ino1,
#if BITS_PER_LONG == 64
	Fh_ino2,
#endif
	Fh_dir_ino1,
#if BITS_PER_LONG == 64
	Fh_dir_ino2,
#endif
	Fh_h_ino1,
#if BITS_PER_LONG == 64
	Fh_h_ino2,
#endif
	Fh_h_igen,
	Fh_h_type,
	Fh_tail,

	Fh_ino = Fh_ino1,
	Fh_dir_ino = Fh_dir_ino1,
	Fh_h_ino = Fh_h_ino1,
};

/* ---------------------------------------------------------------------- */

static struct dentry *decode_by_ino(struct super_block *sb, ino_t ino,
				    ino_t dir_ino)
{
	struct dentry *dentry, *parent;
	struct inode *inode;

	LKTRTrace("i%lu, diri%lu\n", ino, dir_ino);

	dentry = NULL;
	inode = ilookup(sb, ino);
	if (unlikely(!inode))
		goto out;

	dentry = ERR_PTR(-ESTALE);
	if (unlikely(is_bad_inode(inode)))
		goto out_iput;

	dentry = NULL;
	if (!S_ISDIR(inode->i_mode)) {
		struct dentry *d;
		spin_lock(&dcache_lock);
		list_for_each_entry(d, &inode->i_dentry, d_alias)
			/* dcache_lock is locked */
			if (!AuTestAnon(d)
			    && d->d_parent->d_inode->i_ino == dir_ino) {
				dentry = dget_locked(d);
				break;
			}
		spin_unlock(&dcache_lock);
	} else {
		dentry = d_find_alias(inode);
		if (dentry && !AuTestAnon(dentry)) {
			int same_ino;
			parent = dget_parent(dentry);
			same_ino = (parent->d_inode->i_ino == dir_ino);
			dput(parent);
			if (same_ino)
				goto out_iput; /* success */
		}

		dput(dentry);
		dentry = NULL;
	}

 out_iput:
	iput(inode);
 out:
	AuTraceErrPtr(dentry);
	return dentry;
}

/* ---------------------------------------------------------------------- */

struct find_name_by_ino {
	int called, found;
	ino_t ino;
	char *name;
	int namelen;
};

static int
find_name_by_ino(void *arg, const char *name, int namelen, loff_t offset,
		 au_filldir_ino_t ino, unsigned int d_type)
{
	struct find_name_by_ino *a = arg;

	a->called++;
	if (a->ino != ino)
		return 0;

	memcpy(a->name, name, namelen);
	a->namelen = namelen;
	a->found = 1;
	return 1;
}

static struct dentry *decode_by_dir_ino(struct super_block *sb, ino_t ino,
					ino_t dir_ino)
{
	struct dentry *dentry, *parent;
	struct inode *dir;
	struct find_name_by_ino arg;
	struct file *file;
	int err;

	LKTRTrace("i%lu, diri%lu\n", ino, dir_ino);

	dentry = NULL;
	dir = ilookup(sb, dir_ino);
	if (unlikely(!dir))
		goto out;

	dentry = ERR_PTR(-ESTALE);
	if (unlikely(is_bad_inode(dir)))
		goto out_iput;

	dentry = NULL;
	parent = d_find_alias(dir);
	if (parent) {
		if (unlikely(AuTestAnon(parent))) {
			dput(parent);
			goto out_iput;
		}
	} else
		goto out_iput;

	file = dentry_open(parent, NULL, au_dir_roflags);
	dentry = (void *)file;
	if (IS_ERR(file))
		goto out_iput;

	dentry = ERR_PTR(-ENOMEM);
	arg.name = __getname();
	if (unlikely(!arg.name))
		goto out_fput;
	arg.ino = ino;
	arg.found = 0;

	do {
		arg.called = 0;
		//smp_mb();
		err = vfsub_readdir(file, find_name_by_ino, &arg, /*dlgt*/0);
	} while (!err && !arg.found && arg.called);
	dentry = ERR_PTR(err);
	if (arg.found) {
		/* do not call au_lkup_one(), nor dlgt */
		vfsub_i_lock(dir);
		dentry = vfsub_lookup_one_len(arg.name, parent, arg.namelen);
		vfsub_i_unlock(dir);
		AuTraceErrPtr(dentry);
	}

	//out_putname:
	__putname(arg.name);
 out_fput:
	fput(file);
 out_iput:
	iput(dir);
 out:
	AuTraceErrPtr(dentry);
	return dentry;
}

/* ---------------------------------------------------------------------- */

struct append_name {
	int found, called, len;
	char *h_path;
	ino_t h_ino;
};

static int append_name(void *arg, const char *name, int len, loff_t pos,
		       au_filldir_ino_t ino, unsigned int d_type)
{
	struct append_name *a = arg;
	char *p;

	a->called++;
	if (ino != a->h_ino)
		return 0;

	AuDebugOn(len == 1 && *name == '.');
	AuDebugOn(len == 2 && name[0] == '.' && name[1] == '.');
	a->len = strlen(a->h_path);
	memmove(a->h_path - len - 1, a->h_path, a->len);
	a->h_path -= len + 1;
	p = a->h_path + a->len;
	*p++ = '/';
	memcpy(p, name, len);
	a->len += 1 + len;
	a->found++;
	return 1;
}

static int h_acceptable(void *expv, struct dentry *dentry)
{
	return 1;
}

static struct dentry *
decode_by_path(struct super_block *sb, aufs_bindex_t bindex, __u32 *fh,
	       int fh_len, void *context)
{
	struct dentry *dentry, *h_parent, *root, *h_root;
	struct super_block *h_sb;
	char *path, *p;
	struct vfsmount *h_mnt;
	struct append_name arg;
	int len, err;
	struct file *h_file;
	struct nameidata nd;
	struct aufs_branch *br;

	LKTRTrace("b%d\n", bindex);
	SiMustAnyLock(sb);

	br = stobr(sb, bindex);
	/* br_get(br); */
	h_mnt = br->br_mnt;
	h_sb = h_mnt->mnt_sb;
	LKTRTrace("%s, h_decode_fh\n", au_sbtype(h_sb));
	h_parent = au_call_decode_fh(h_mnt, fh + Fh_tail, fh_len - Fh_tail,
				     fh[Fh_h_type], h_acceptable,
				     /*context*/NULL);
	dentry = h_parent;
	if (unlikely(!h_parent || IS_ERR(h_parent))) {
		AuWarn1("%s decode_fh failed\n", au_sbtype(h_sb));
		goto out;
	}
	dentry = NULL;
	if (unlikely(AuTestAnon(h_parent))) {
		AuWarn1("%s decode_fh returned a disconnected dentry\n",
			au_sbtype(h_sb));
		dput(h_parent);
		goto out;
	}

	dentry = ERR_PTR(-ENOMEM);
	path = __getname();
	if (unlikely(!path)) {
		dput(h_parent);
		goto out;
	}

	root = sb->s_root;
	di_read_lock_parent(root, !AuLock_IR);
	h_root = au_h_dptr_i(root, bindex);
	di_read_unlock(root, !AuLock_IR);
	arg.h_path = d_path(h_root, h_mnt, path, PATH_MAX);
	dentry = (void *)arg.h_path;
	if (unlikely(!arg.h_path || IS_ERR(arg.h_path)))
		goto out_putname;
	len = strlen(arg.h_path);
	arg.h_path = d_path(h_parent, h_mnt, path, PATH_MAX);
	dentry = (void *)arg.h_path;
	if (unlikely(!arg.h_path || IS_ERR(arg.h_path)))
		goto out_putname;
	LKTRTrace("%s\n", arg.h_path);
	if (len != 1)
		arg.h_path += len;
	LKTRTrace("%p, %s, %ld\n",
		  arg.h_path, arg.h_path, (long)(arg.h_path - path));

	/* cf. fs/exportfs/expfs.c */
	h_file = dentry_open(h_parent, NULL, au_dir_roflags);
	dentry = (void *)h_file;
	if (IS_ERR(h_file))
		goto out_putname;

	arg.found = 0;
	arg.h_ino = decode_ino(fh + Fh_h_ino);
	do {
		arg.called = 0;
		err = vfsub_readdir(h_file, append_name, &arg, /*dlgt*/0);
	} while (!err && !arg.found && arg.called);
	LKTRTrace("%p, %s, %d\n", arg.h_path, arg.h_path, arg.len);

	p = d_path(root, stosi(sb)->si_mnt, path, PATH_MAX - arg.len);
	dentry = (void *)p;
	if (unlikely(!p || IS_ERR(p)))
		goto out_fput;
	p[strlen(p)] = '/';
	LKTRTrace("%s\n", p);

	err = vfsub_path_lookup(p, LOOKUP_FOLLOW, &nd);
	dentry = ERR_PTR(err);
	if (!err) {
		dentry = dget(nd.dentry);
		if (unlikely(AuTestAnon(dentry))) {
			dput(dentry);
			dentry = ERR_PTR(-ESTALE);
		}
		path_release(&nd);
	}

 out_fput:
	fput(h_file);
 out_putname:
	__putname(path);
 out:
	/* br_put(br); */
	AuTraceErrPtr(dentry);
	return dentry;
}

/* ---------------------------------------------------------------------- */

static struct dentry *
aufs_decode_fh(struct super_block *sb, __u32 *fh, int fh_len, int fh_type,
	       int (*acceptable)(void *context, struct dentry *de),
	       void *context)
{
	struct dentry *dentry;
	ino_t ino, dir_ino;
	aufs_bindex_t bindex, br_id;
	struct inode *inode, *h_inode;
	au_gen_t sigen;

	//au_debug_on();
	LKTRTrace("%d, fh{i%u, br_id %u, sigen %u, hi%u}\n",
		  fh_type, fh[Fh_ino], fh[Fh_br_id], fh[Fh_sigen],
		  fh[Fh_h_ino]);
	AuDebugOn(fh_len < Fh_tail);

	si_read_lock(sb, !AuLock_FLUSH);
	lockdep_off();

	/* branch id may be wrapped around */
	dentry = ERR_PTR(-ESTALE);
	br_id = fh[Fh_br_id];
	sigen = fh[Fh_sigen];
	bindex = find_brindex(sb, br_id);
	if (unlikely(bindex < 0 || sigen + AUFS_BRANCH_MAX <= au_sigen(sb)))
		goto out;

	/* is this inode still cached? */
	ino = decode_ino(fh + Fh_ino);
	dir_ino = decode_ino(fh + Fh_dir_ino);
	dentry = decode_by_ino(sb, ino, dir_ino);
	if (IS_ERR(dentry))
		goto out;
	if (dentry)
		goto accept;

	/* is the parent dir cached? */
	dentry = decode_by_dir_ino(sb, ino, dir_ino);
	if (IS_ERR(dentry))
		goto out;
	if (dentry)
		goto accept;

	/* lookup path */
	dentry = decode_by_path(sb, bindex, fh, fh_len, context);
	if (IS_ERR(dentry))
		goto out;
	if (unlikely(!dentry))
		goto out_stale;
	if (unlikely(dentry->d_inode->i_ino != ino))
		goto out_dput;

 accept:
	inode = dentry->d_inode;
	h_inode = NULL;
	ii_read_lock_child(inode);
	if (ibstart(inode) <= bindex && bindex <= ibend(inode))
		h_inode = au_h_iptr_i(inode, bindex);
	ii_read_unlock(inode);
	if (h_inode
	    && h_inode->i_generation == fh[Fh_h_igen]
	    && acceptable(context, dentry))
		goto out; /* success */
 out_dput:
	dput(dentry);
 out_stale:
	dentry = ERR_PTR(-ESTALE);
 out:
	lockdep_on();
	si_read_unlock(sb);
	AuTraceErrPtr(dentry);
	//au_debug_off();
	return dentry;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
static struct dentry *
aufs_fh_to_dentry(struct super_block *sb, struct fid *fid, int fh_len,
		  int fh_type)
{
	return aufs_decode_fh(sb, fid->raw, fh_len, fh_type, h_acceptable,
			      /*context*/NULL);
}
#endif /* KERNEL_VERSION */

/* ---------------------------------------------------------------------- */

static int aufs_encode_fh(struct dentry *dentry, __u32 *fh, int *max_len,
			  int connectable)
{
	int err;
	struct super_block *sb, *h_sb;
	struct inode *inode, *h_inode, *dir;
	aufs_bindex_t bindex;
	union conv u;
	struct dentry *parent, *h_parent;

	//au_debug_on();
	BUILD_BUG_ON(sizeof(u.ino) != sizeof(u.a));
	LKTRTrace("%.*s, max %d, conn %d\n",
		  AuDLNPair(dentry), *max_len, connectable);
	AuDebugOn(AuTestAnon(dentry));
	inode = dentry->d_inode;
	AuDebugOn(!inode);
	parent = dget_parent(dentry);
	AuDebugOn(AuTestAnon(parent));

	err = -ENOSPC;
	if (unlikely(*max_len <= Fh_tail)) {
		AuWarn1("NFSv2 client (max_len %d)?\n", *max_len);
		goto out;
	}

	sb = dentry->d_sb;
	si_read_lock(sb, AuLock_FLUSH);
	di_read_lock_child(dentry, AuLock_IR);
	di_read_lock_parent(parent, AuLock_IR);
#ifdef CONFIG_AUFS_DEBUG
	if (unlikely(AuFlag(stosi(sb), f_xino) == AuXino_NONE))
		AuWarn1("NFS-exporting requires xino\n");
#if 0 // temp
	if (unlikely(au_flag_test_udba_inotify(sb)))
		AuWarn1("udba=inotify is not recommended when exporting\n");
#endif
#endif

	err = -EPERM;
	bindex = ibstart(inode);
	h_sb = sbr_sb(sb, bindex);
	if (unlikely(!h_sb->s_export_op)) {
		AuErr1("%s branch is not exportable\n", au_sbtype(h_sb));
		goto out_unlock;
	}

#ifdef CONFIG_AUFS_ROBR
	if (unlikely(au_test_aufs(h_sb))) {
		AuErr1("aufs branch is not supported\n");
		goto out_unlock;
	}
#endif

	/* doesn't support pseudo-link */
	if (unlikely(bindex < dbstart(dentry)
		     || dbend(dentry) < bindex
		     || !au_h_dptr_i(dentry, bindex))) {
		AuErr("%.*s/%.*s, b%d, pseudo-link?\n",
		      AuDLNPair(parent), AuDLNPair(dentry), bindex);
		goto out_unlock;
	}

	fh[Fh_br_id] = sbr_id(sb, bindex);
	fh[Fh_sigen] = au_sigen(sb);
	encode_ino(fh + Fh_ino, inode->i_ino);
	dir = parent->d_inode;
	encode_ino(fh + Fh_dir_ino, dir->i_ino);
	h_inode = au_h_iptr(inode);
	encode_ino(fh + Fh_h_ino, h_inode->i_ino);
	fh[Fh_h_igen] = h_inode->i_generation;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	/* it should be set at exporting time */
	if (unlikely(!h_sb->s_export_op->find_exported_dentry)) {
		AuWarn("set default find_exported_dentry for %s\n",
		       au_sbtype(h_sb));
		h_sb->s_export_op->find_exported_dentry = find_exported_dentry;
	}
#endif

	*max_len -= Fh_tail;
	//LKTRTrace("Fh_tail %d, max_len %d\n", Fh_tail, *max_len);
	h_parent = au_h_dptr_i(parent, bindex);
	AuDebugOn(AuTestAnon(h_parent));
	fh[Fh_h_type] = au_call_encode_fh(h_parent, fh + Fh_tail, max_len,
					  connectable);
	err = fh[Fh_h_type];
	*max_len += Fh_tail;
	if (err != 255)
		err = 2; //??
	else
		AuWarn1("%s encode_fh failed\n", au_sbtype(h_sb));

 out_unlock:
	di_read_unlock(parent, AuLock_IR);
	aufs_read_unlock(dentry, AuLock_IR);
 out:
	dput(parent);
	AuTraceErr(err);
	//au_debug_off();
	if (unlikely(err < 0))
		err = 255;
	return err;
}

/* ---------------------------------------------------------------------- */

struct export_operations aufs_export_op = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	.fh_to_dentry	= aufs_fh_to_dentry,
#else
	.decode_fh	= aufs_decode_fh,
#endif
	.encode_fh	= aufs_encode_fh
};

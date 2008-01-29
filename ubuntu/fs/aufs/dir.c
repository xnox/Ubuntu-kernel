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

/* $Id: dir.c,v 1.49 2007/10/29 04:41:28 sfjro Exp $ */

#include "aufs.h"

static int reopen_dir(struct file *file)
{
	int err;
	struct dentry *dentry, *hidden_dentry;
	aufs_bindex_t bindex, btail, bstart;
	struct file *hidden_file;

	dentry = file->f_dentry;
	LKTRTrace("%.*s\n", AuDLNPair(dentry));
	AuDebugOn(!S_ISDIR(dentry->d_inode->i_mode));

	/* open all hidden dirs */
	bstart = dbstart(dentry);
#if 1
	for (bindex = fbstart(file); bindex < bstart; bindex++)
		set_h_fptr(file, bindex, NULL);
#endif
	set_fbstart(file, bstart);
	btail = dbtaildir(dentry);
#if 1
	for (bindex = fbend(file); btail < bindex; bindex--)
		set_h_fptr(file, bindex, NULL);
#endif
	set_fbend(file, btail);
	for (bindex = bstart; bindex <= btail; bindex++) {
		hidden_dentry = au_h_dptr_i(dentry, bindex);
		if (!hidden_dentry)
			continue;
		hidden_file = au_h_fptr_i(file, bindex);
		if (hidden_file) {
			AuDebugOn(hidden_file->f_dentry != hidden_dentry);
			continue;
		}

		hidden_file = au_h_open(dentry, bindex, file->f_flags, file);
		// unavailable
		//if (LktrCond) {fput(hidden_file);
		//br_put(stobr(dentry->d_sb, bindex));hidden_file=ERR_PTR(-1);}
		err = PTR_ERR(hidden_file);
		if (IS_ERR(hidden_file))
			goto out; // close all?
		//cpup_file_flags(hidden_file, file);
		set_h_fptr(file, bindex, hidden_file);
	}
	err = 0;

 out:
	AuTraceErr(err);
	return err;
}

static int do_open_dir(struct file *file, int flags)
{
	int err;
	aufs_bindex_t bindex, btail;
	struct dentry *dentry, *hidden_dentry;
	struct file *hidden_file;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, 0x%x\n", AuDLNPair(dentry), flags);
	AuDebugOn(!dentry->d_inode || !S_ISDIR(dentry->d_inode->i_mode));

	err = 0;
	set_fvdir_cache(file, NULL);
	file->f_version = dentry->d_inode->i_version;
	bindex = dbstart(dentry);
	set_fbstart(file, bindex);
	btail = dbtaildir(dentry);
	set_fbend(file, btail);
	for (; !err && bindex <= btail; bindex++) {
		hidden_dentry = au_h_dptr_i(dentry, bindex);
		if (!hidden_dentry)
			continue;

		hidden_file = au_h_open(dentry, bindex, flags, file);
		//if (LktrCond) {fput(hidden_file);
		//br_put(stobr(dentry->d_sb, bindex));hidden_file=ERR_PTR(-1);}
		if (!IS_ERR(hidden_file)) {
			set_h_fptr(file, bindex, hidden_file);
			continue;
		}
		err = PTR_ERR(hidden_file);
	}
	if (!err)
		return 0; /* success */

	/* close all */
	for (bindex = fbstart(file); !err && bindex <= btail; bindex++)
		set_h_fptr(file, bindex, NULL);
	set_fbstart(file, -1);
	set_fbend(file, -1);
	return err;
}

static int aufs_open_dir(struct inode *inode, struct file *file)
{
	return au_do_open(inode, file, do_open_dir);
}

static int aufs_release_dir(struct inode *inode, struct file *file)
{
	struct aufs_vdir *vdir_cache;
	struct super_block *sb;

	LKTRTrace("i%lu, %.*s\n", inode->i_ino, AuDLNPair(file->f_dentry));

	sb = file->f_dentry->d_sb;
	si_read_lock(sb, !AuLock_FLUSH);
	fi_write_lock(file);
	vdir_cache = fvdir_cache(file);
	if (vdir_cache)
		au_vdir_free(vdir_cache);
	fi_write_unlock(file);
	au_finfo_fin(file);
	si_read_unlock(sb);
	return 0;
}

static int fsync_dir(struct dentry *dentry, int datasync)
{
	int err;
	struct inode *inode;
	struct super_block *sb;
	aufs_bindex_t bend, bindex;

	LKTRTrace("%.*s, %d\n", AuDLNPair(dentry), datasync);
	DiMustAnyLock(dentry);
	sb = dentry->d_sb;
	SiMustAnyLock(sb);
	inode = dentry->d_inode;
	IMustLock(inode);
	IiMustAnyLock(inode);

	err = 0;
	bend = dbend(dentry);
	for (bindex = dbstart(dentry); !err && bindex <= bend; bindex++) {
		struct dentry *h_dentry;
		struct inode *h_inode;
		struct file_operations *fop;

		if (au_test_ro(sb, bindex, inode))
			continue;
		h_dentry = au_h_dptr_i(dentry, bindex);
		if (!h_dentry)
			continue;
		h_inode = h_dentry->d_inode;
		if (!h_inode)
			continue;

		/* cf. fs/nsfd/vfs.c and fs/nfsd/nfs4recover.c */
		//hdir_lock(h_inode, inode, bindex);
		vfsub_i_lock(h_inode);
		fop = (void *)h_inode->i_fop;
		err = filemap_fdatawrite(h_inode->i_mapping);
		if (!err && fop && fop->fsync)
			err = fop->fsync(NULL, h_dentry, datasync);
		if (!err)
			err = filemap_fdatawrite(h_inode->i_mapping);
		if (!err)
			au_update_fuse_h_inode(NULL, h_dentry); /*ignore*/
		//hdir_unlock(h_inode, inode, bindex);
		vfsub_i_unlock(h_inode);
	}

	AuTraceErr(err);
	return err;
}

/*
 * @file may be NULL
 */
static int aufs_fsync_dir(struct file *file, struct dentry *dentry,
			  int datasync)
{
	int err;
	struct inode *inode;
	struct file *h_file;
	struct super_block *sb;
	aufs_bindex_t bend, bindex;

	LKTRTrace("%.*s, %d\n", AuDLNPair(dentry), datasync);
	inode = dentry->d_inode;
	IMustLock(inode);

	err = 0;
	sb = dentry->d_sb;
	si_read_lock(sb, !AuLock_FLUSH);
	if (file) {
		err = au_reval_and_lock_finfo(file, reopen_dir, /*wlock*/1,
					      /*locked*/1);
		//err = -1;
		if (unlikely(err))
			goto out;
	} else
		di_read_lock_child(dentry, !AuLock_IW);

	ii_write_lock_child(inode);
	if (file) {
		bend = fbend(file);
		for (bindex = fbstart(file); !err && bindex <= bend; bindex++) {
			h_file = au_h_fptr_i(file, bindex);
			if (!h_file || au_test_ro(sb, bindex, inode))
				continue;

			err = -EINVAL;
			if (h_file->f_op && h_file->f_op->fsync) {
				// todo: try do_fsync() in fs/sync.c
#if 0 // debug
				AuDebugOn(h_file->f_dentry->d_inode
					  != au_h_iptr_i(inode, bindex));
				hdir_lock(h_file->f_dentry->d_inode, inode,
					  bindex);
#else
				vfsub_i_lock(h_file->f_dentry->d_inode);
#endif
				err = h_file->f_op->fsync
					(h_file, h_file->f_dentry, datasync);
				//err = -1;
				if (!err)
					au_update_fuse_h_inode
						(h_file->f_vfsmnt,
						 h_file->f_dentry);
				/*ignore*/
#if 0 // debug
				hdir_unlock(h_file->f_dentry->d_inode, inode,
					    bindex);
#else
				vfsub_i_unlock(h_file->f_dentry->d_inode);
#endif
			}
		}
	} else
		err = fsync_dir(dentry, datasync);
	au_cpup_attr_timesizes(inode);
	ii_write_unlock(inode);
	if (file)
		fi_write_unlock(file);
	else
		di_read_unlock(dentry, !AuLock_IW);

 out:
	si_read_unlock(sb);
	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static int aufs_readdir(struct file *file, void *dirent, filldir_t filldir)
{
	int err;
	struct dentry *dentry;
	struct inode *inode;
	struct super_block *sb;

	dentry = file->f_dentry;
	LKTRTrace("%.*s, pos %Ld\n", AuDLNPair(dentry), file->f_pos);
	inode = dentry->d_inode;
	IMustLock(inode);

	au_nfsd_lockdep_off();
	sb = dentry->d_sb;
	si_read_lock(sb, !AuLock_FLUSH);
	err = au_reval_and_lock_finfo(file, reopen_dir, /*wlock*/1,
				      /*locked*/1);
	if (unlikely(err))
		goto out;

	ii_write_lock_child(inode);
	err = au_vdir_init(file);
	if (unlikely(err)) {
		ii_write_unlock(inode);
		goto out_unlock;
	}
	//DbgVdir(fvdir_cache(file));// goto out_unlock;

	/* nfsd filldir calls lookup_one_len(). */
	ii_downgrade_lock(inode);
	err = au_fill_de(file, dirent, filldir);
	//DbgVdir(fvdir_cache(file));// goto out_unlock;

	inode->i_atime = au_h_iptr(inode)->i_atime;
	ii_read_unlock(inode);

 out_unlock:
	fi_write_unlock(file);
 out:
	si_read_unlock(sb);
	au_nfsd_lockdep_on();
#if 0 // debug
	if (LktrCond)
		igrab(inode);
#endif
	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

struct test_empty_arg {
	struct aufs_nhash *whlist;
	int whonly;
	aufs_bindex_t bindex;
	int err, called;
};

static int test_empty_cb(void *__arg, const char *__name, int namelen,
			 loff_t offset, au_filldir_ino_t ino,
			 unsigned int d_type)
{
	struct test_empty_arg *arg = __arg;
	char *name = (void *)__name;

	LKTRTrace("%.*s\n", namelen, name);

	arg->err = 0;
	arg->called++;
	//smp_mb();
	if (name[0] == '.'
	    && (namelen == 1 || (name[1] == '.' && namelen == 2)))
		return 0; /* success */

	if (namelen <= AUFS_WH_PFX_LEN
	    || memcmp(name, AUFS_WH_PFX, AUFS_WH_PFX_LEN)) {
		if (arg->whonly && !nhash_test_known_wh(arg->whlist, name,
							namelen))
			arg->err = -ENOTEMPTY;
		goto out;
	}

	name += AUFS_WH_PFX_LEN;
	namelen -= AUFS_WH_PFX_LEN;
	if (!nhash_test_known_wh(arg->whlist, name, namelen))
		arg->err = nhash_append_wh(arg->whlist, name, namelen,
					   arg->bindex);

 out:
	//smp_mb();
	AuTraceErr(arg->err);
	return arg->err;
}

static int do_test_empty(struct dentry *dentry, struct test_empty_arg *arg)
{
	int err, dlgt;
	struct file *hidden_file;

	LKTRTrace("%.*s, {%p, %d, %d}\n",
		  AuDLNPair(dentry), arg->whlist, arg->whonly, arg->bindex);

	hidden_file = au_h_open(dentry, arg->bindex,
				O_RDONLY | O_NONBLOCK | O_DIRECTORY
				| O_LARGEFILE, /*file*/NULL);
	err = PTR_ERR(hidden_file);
	if (IS_ERR(hidden_file))
		goto out;

	dlgt = au_need_dlgt(dentry->d_sb);
	//hidden_file->f_pos = 0;
	do {
		arg->err = 0;
		arg->called = 0;
		//smp_mb();
		err = vfsub_readdir(hidden_file, test_empty_cb, arg, dlgt);
		if (err >= 0)
			err = arg->err;
	} while (!err && arg->called);
	fput(hidden_file);
	sbr_put(dentry->d_sb, arg->bindex);

 out:
	AuTraceErr(err);
	return err;
}

struct do_test_empty_args {
	int *errp;
	struct dentry *dentry;
	struct test_empty_arg *arg;
};

static void call_do_test_empty(void *args)
{
	struct do_test_empty_args *a = args;
	*a->errp = do_test_empty(a->dentry, a->arg);
}

static int sio_test_empty(struct dentry *dentry, struct test_empty_arg *arg)
{
	int err, wkq_err;
	struct dentry *hidden_dentry;
	struct inode *hidden_inode;

	LKTRTrace("%.*s\n", AuDLNPair(dentry));
	hidden_dentry = au_h_dptr_i(dentry, arg->bindex);
	AuDebugOn(!hidden_dentry);
	hidden_inode = hidden_dentry->d_inode;
	AuDebugOn(!hidden_inode || !S_ISDIR(hidden_inode->i_mode));

	vfsub_i_lock_nested(hidden_inode, AuLsc_I_CHILD);
	err = au_test_perm(hidden_inode, MAY_EXEC | MAY_READ,
			   au_need_dlgt(dentry->d_sb));
	vfsub_i_unlock(hidden_inode);
	if (!err)
		err = do_test_empty(dentry, arg);
	else {
		struct do_test_empty_args args = {
			.errp	= &err,
			.dentry	= dentry,
			.arg	= arg
		};
		wkq_err = au_wkq_wait(call_do_test_empty, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			err = wkq_err;
	}

	AuTraceErr(err);
	return err;
}

int au_test_empty_lower(struct dentry *dentry)
{
	int err;
	struct inode *inode;
	struct test_empty_arg arg;
	struct aufs_nhash *whlist;
	aufs_bindex_t bindex, bstart, btail;

	LKTRTrace("%.*s\n", AuDLNPair(dentry));
	inode = dentry->d_inode;
	AuDebugOn(!inode || !S_ISDIR(inode->i_mode));

	whlist = nhash_new(GFP_TEMPORARY);
	err = PTR_ERR(whlist);
	if (IS_ERR(whlist))
		goto out;

	bstart = dbstart(dentry);
	arg.whlist = whlist;
	arg.whonly = 0;
	arg.bindex = bstart;
	err = do_test_empty(dentry, &arg);
	if (unlikely(err))
		goto out_whlist;

	arg.whonly = 1;
	btail = dbtaildir(dentry);
	for (bindex = bstart + 1; !err && bindex <= btail; bindex++) {
		struct dentry *hidden_dentry;
		hidden_dentry = au_h_dptr_i(dentry, bindex);
		if (hidden_dentry && hidden_dentry->d_inode) {
			AuDebugOn(!S_ISDIR(hidden_dentry->d_inode->i_mode));
			arg.bindex = bindex;
			err = do_test_empty(dentry, &arg);
		}
	}

 out_whlist:
	nhash_del(whlist);
 out:
	AuTraceErr(err);
	return err;
}

int au_test_empty(struct dentry *dentry, struct aufs_nhash *whlist)
{
	int err;
	struct inode *inode;
	struct test_empty_arg arg;
	aufs_bindex_t bindex, btail;

	LKTRTrace("%.*s\n", AuDLNPair(dentry));
	inode = dentry->d_inode;
	AuDebugOn(!inode || !S_ISDIR(inode->i_mode));

	err = 0;
	arg.whlist = whlist;
	arg.whonly = 1;
	btail = dbtaildir(dentry);
	for (bindex = dbstart(dentry); !err && bindex <= btail; bindex++) {
		struct dentry *hidden_dentry;
		hidden_dentry = au_h_dptr_i(dentry, bindex);
		if (hidden_dentry && hidden_dentry->d_inode) {
			AuDebugOn(!S_ISDIR(hidden_dentry->d_inode->i_mode));
			arg.bindex = bindex;
			err = sio_test_empty(dentry, &arg);
		}
	}

	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

void au_add_nlink(struct inode *dir, struct inode *h_dir)
{
	AuDebugOn(!S_ISDIR(dir->i_mode) || !S_ISDIR(h_dir->i_mode));
	dir->i_nlink += h_dir->i_nlink - 2;
	if (unlikely(h_dir->i_nlink < 2))
		dir->i_nlink += 2;
}

void au_sub_nlink(struct inode *dir, struct inode *h_dir)
{
	AuDebugOn(!S_ISDIR(dir->i_mode) || !S_ISDIR(h_dir->i_mode));
	dir->i_nlink -= h_dir->i_nlink - 2;
	if (unlikely(h_dir->i_nlink < 2))
		dir->i_nlink -= 2;
}

/* ---------------------------------------------------------------------- */

struct file_operations aufs_dir_fop = {
	.read		= generic_read_dir,
	.readdir	= aufs_readdir,
	.open		= aufs_open_dir,
	.release	= aufs_release_dir,
	.flush		= aufs_flush,
	.fsync		= aufs_fsync_dir,
};

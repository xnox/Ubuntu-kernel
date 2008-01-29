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

/* $Id: dentry.c,v 1.65 2008/01/21 04:57:48 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
#include "aufs.h"

#if 0
/* subset of nameidata */
struct au_ndsub {
	struct dentry	*dentry;
	struct vfsmount *mnt;
	unsigned int	flags;

	union {
		struct open_intent open;
	} intent;
};

static void au_ndsub_restore(struct nameidata *nd, struct au_ndsub *save)
{
	nd->dentry = save->dentry;
	nd->mnt = save->mnt;
	nd->flags = save->flags;
	nd->intent = save->intent;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) \
	&& !defined(AuNoNfsBranch)
static int au_fake_intent(/* struct au_ndsub *save,  */struct nameidata *nd,
			  int perm)
{
	int err;

	LKTRTrace("perm %d\n", perm);

	err = 0;
#if 0
	save->dentry = nd->dentry;
	save->mnt = nd->mnt;
	save->flags = nd->flags;
	save->intent = nd->intent;
#endif

	nd->intent.open.file = NULL;
	if (nd->flags & LOOKUP_OPEN) {
		err = -ENFILE;
		nd->intent.open.file = get_empty_filp();
		if (unlikely(!nd->intent.open.file)) {
			//nd->intent.open.file = save->intent.open.file;
			goto out;
		}

		err = 0;
		if (!br_writable(perm)) {
			nd->intent.open.flags = au_file_roflags
				(nd->intent.open.flags) | FMODE_READ;
			nd->flags &= ~LOOKUP_CREATE;
		}
	}

 out:
	AuTraceErr(err);
	return err;
}

static int au_hin_after_reval(struct nameidata *nd, struct dentry *dentry,
			      aufs_bindex_t bindex, struct file *file)
{
	int err;

	LKTRTrace("nd %p, %.*s, b%d, f %d\n",
		  nd, AuDLNPair(dentry), bindex, !!file);

	err = 0;
	if ((nd->flags & LOOKUP_OPEN)
	    && nd->intent.open.file
	    && !IS_ERR(nd->intent.open.file)) {
		if (nd->intent.open.file->f_dentry) {
			//AuDbgFile(nd->intent.open.file);
			err = au_set_h_intent(dentry, bindex, file,
					      nd->intent.open.file);
			if (!err)
				nd->intent.open.file = NULL;
		}
		if (unlikely(nd->intent.open.file))
			put_filp(nd->intent.open.file);
	}

	return err;
}

#ifdef CONFIG_AUFS_DLGT
struct au_lookup_hash_args {
	struct dentry **errp;
	struct qstr *name;
	struct dentry *base;
	struct nameidata *nd;
};

static void au_call_lookup_hash(void *args)
{
	struct au_lookup_hash_args *a = args;
	*a->errp = vfsub__lookup_hash(a->name, a->base, a->nd);
}
#endif /* CONFIG_AUFS_DLGT */

static struct dentry *au_lkup_hash(const char *name, struct dentry *parent,
				   int len, struct aufs_ndx *ndx)
{
	struct dentry *dentry;
	char *p;
	unsigned long hash;
	struct qstr this;
	unsigned int c;
	struct nameidata tmp_nd, *ndo;
	int err;

	dentry = ERR_PTR(-EACCES);
	this.name = name;
	this.len = len;
	if (unlikely(!len))
		goto out;

	p = (void *)name;
	hash = init_name_hash();
	while (len--) {
		c = *p++;
		if (unlikely(c == '/' || c == '\0'))
			goto out;
		hash = partial_name_hash(c, hash);
	}
	this.hash = end_name_hash(hash);

	ndo = ndx->nd;
	if (ndo) {
		tmp_nd = *ndo;
		err = au_fake_intent(&tmp_nd, ndx->br->br_perm);
		dentry = ERR_PTR(err);
		if (unlikely(err))
			goto out_intent;
	} else
		memset(&tmp_nd, 0, sizeof(tmp_nd));

	tmp_nd.dentry = dget(parent);
	tmp_nd.mnt = mntget(ndx->nfsmnt);
#ifndef CONFIG_AUFS_DLGT
	dentry = vfsub__lookup_hash(&this, parent, &tmp_nd);
#else
	if (!ndx->dlgt)
		dentry = vfsub__lookup_hash(&this, parent, &tmp_nd);
	else {
		int wkq_err;
		struct au_lookup_hash_args args = {
			.errp	= &dentry,
			.name	= &this,
			.base	= parent,
			.nd	= &tmp_nd
		};
		wkq_err = au_wkq_wait(au_call_lookup_hash, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			dentry = ERR_PTR(wkq_err);
	}
#endif /* CONFIG_AUFS_DLGT */
	if (0 && !IS_ERR(dentry))
		AuDbgDentry(dentry);
	if (!IS_ERR(dentry)) {
		/* why negative dentry for a new dir was unhashed? */
		if (unlikely(d_unhashed(dentry)))
			d_rehash(dentry);
		if (tmp_nd.intent.open.file
		    && tmp_nd.intent.open.file->f_dentry) {
			//AuDbgFile(tmp_nd.intent.open.file);
			ndx->nd_file = tmp_nd.intent.open.file;
			tmp_nd.intent.open.file = NULL;
			//br_get(ndx->br);
		}
	}
	path_release(&tmp_nd);

 out_intent:
	if (tmp_nd.intent.open.file)
		put_filp(tmp_nd.intent.open.file);
 out:
	AuTraceErrPtr(dentry);
	return dentry;
}
#else
static int au_fake_intent(struct nameidata *nd, int perm)
{
	return 0;
}

static int au_hin_after_reval(struct nameidata *nd, struct dentry *dentry,
			      aufs_bindex_t bindex, struct file *file)
{
	return 0;
}

#if !defined(AuNoNfsBranch) || defined(CONFIG_AUFS_DLGT)
static struct dentry *au_lkup_hash(const char *name, struct dentry *parent,
				   int len, struct aufs_ndx *ndx)
{
	return ERR_PTR(-ENOSYS);
}
#endif
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) && !AuNoNfsBranch */

/* ---------------------------------------------------------------------- */

#if !defined(AuNoNfsBranch) || defined(CONFIG_AUFS_DLGT)

#ifdef CONFIG_AUFS_DLGT
struct au_lookup_one_len_args {
	struct dentry **errp;
	const char *name;
	struct dentry *parent;
	int len;
};

static void au_call_lookup_one_len(void *args)
{
	struct au_lookup_one_len_args *a = args;
	*a->errp = vfsub_lookup_one_len(a->name, a->parent, a->len);
}
#endif /* CONFIG_AUFS_DLGT */

/* cf. lookup_one_len() in linux/fs/namei.c */
struct dentry *au_lkup_one(const char *name, struct dentry *parent, int len,
			   struct aufs_ndx *ndx)
{
	struct dentry *dentry;

	LKTRTrace("%.*s/%.*s, ndx{%d, %d}\n",
		  AuDLNPair(parent), len, name, !!ndx->nfsmnt, ndx->dlgt);

	ndx->nd_file = NULL;
	if (!ndx->nfsmnt) {
#ifndef CONFIG_AUFS_DLGT
		dentry = vfsub_lookup_one_len(name, parent, len);
#else
		if (!ndx->dlgt)
			dentry = vfsub_lookup_one_len(name, parent, len);
		else {
			int wkq_err;
			struct au_lookup_one_len_args args = {
				.errp	= &dentry,
				.name	= name,
				.parent	= parent,
				.len	= len
			};
			wkq_err = au_wkq_wait(au_call_lookup_one_len, &args,
					      /*dlgt*/1);
			if (unlikely(wkq_err))
				dentry = ERR_PTR(wkq_err);
		}
#endif /* CONFIG_AUFS_DLGT */

	} else
		dentry = au_lkup_hash(name, parent, len, ndx);

	AuTraceErrPtr(dentry);
	return dentry;
}
#endif /* !defined(AuNoNfsBranch) || defined(CONFIG_AUFS_DLGT) */

struct au_lkup_one_args {
	struct dentry **errp;
	const char *name;
	struct dentry *parent;
	int len;
	struct aufs_ndx *ndx;
};

static void au_call_lkup_one(void *args)
{
	struct au_lkup_one_args *a = args;
	*a->errp = au_lkup_one(a->name, a->parent, a->len, a->ndx);
}

struct au_do_lookup_args {
	unsigned int		allow_neg:1;
	unsigned int		dlgt:1;
	mode_t			type;
	struct nameidata	*nd;
};

/*
 * returns positive/negative dentry, NULL or an error.
 * NULL means whiteout-ed or not-found.
 */
static
struct dentry *au_do_lookup(struct dentry *h_parent, struct dentry *dentry,
			    aufs_bindex_t bindex, struct qstr *wh_name,
			    struct au_do_lookup_args *args)
{
	struct dentry *h_dentry;
	int wh_found, wh_able, opq;
	struct inode *h_dir, *h_inode;
	struct qstr *name;
	struct super_block *sb;
	struct nameidata tmp_nd;
	struct aufs_ndx ndx = {
		.dlgt	= args->dlgt,
		.nd	= args->nd
	};

	LKTRTrace("%.*s/%.*s, b%d, {allow_neg %d, type 0%o, dlgt %d, nd %d}\n",
		  AuDLNPair(h_parent), AuDLNPair(dentry), bindex,
		  args->allow_neg, args->type, args->dlgt, !!args->nd);
	AuDebugOn(IS_ROOT(dentry));
	h_dir = h_parent->d_inode;

	wh_found = 0;
	sb = dentry->d_sb;
	ndx.nfsmnt = au_nfsmnt(sb, bindex);
	LKTRTrace("nfsmnt %p\n", ndx.nfsmnt);
	ndx.br = stobr(sb, bindex);
	wh_able = br_whable(ndx.br->br_perm);
	name = &dentry->d_name;
	if (unlikely(wh_able)) {
#ifdef CONFIG_AUFS_ROBR
		wh_found = -EPERM;
		if (strncmp(name->name, AUFS_WH_PFX, AUFS_WH_PFX_LEN))
			wh_found = au_test_wh(h_parent, wh_name, /*try_sio*/0,
					      &ndx);
#else
		wh_found = au_test_wh(h_parent, wh_name, /*try_sio*/0, &ndx);
#endif
	}
	//if (LktrCond) wh_found = -1;
	h_dentry = ERR_PTR(wh_found);
	if (!wh_found)
		goto real_lookup;
	if (unlikely(wh_found < 0))
		goto out;

	/* We found a whiteout */
	//set_dbend(dentry, bindex);
	set_dbwh(dentry, bindex);
	if (!args->allow_neg)
		return NULL; /* success */
	if (unlikely(ndx.nd
		     && au_test_nfs(h_parent->d_sb)
		     && (ndx.nd->flags & LOOKUP_CREATE))) {
		tmp_nd = *ndx.nd;
		tmp_nd.flags &= ~(LOOKUP_OPEN | LOOKUP_CREATE);
		ndx.nd = &tmp_nd;
	}

 real_lookup:
	/* do not superio. */
	h_dentry = au_lkup_one(name->name, h_parent, name->len, &ndx);
	//if (LktrCond) {dput(h_dentry); h_dentry = ERR_PTR(-1);}
	if (IS_ERR(h_dentry))
		goto out;
	AuDebugOn(d_unhashed(h_dentry));
	h_inode = h_dentry->d_inode;
	if (!h_inode) {
		if (!args->allow_neg)
			goto out_neg;
	} else if (wh_found
		   || (args->type && args->type != (h_inode->i_mode & S_IFMT)))
		goto out_neg;

	if (dbend(dentry) <= bindex)
		set_dbend(dentry, bindex);
	if (dbstart(dentry) < 0 || bindex < dbstart(dentry))
		set_dbstart(dentry, bindex);
	set_h_dptr(dentry, bindex, h_dentry);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) \
	&& !defined(AuNoNfsBranch)
	if (unlikely(ndx.nd_file)) {
		int err;
		AuDebugOn(!args->nd);
		err = au_set_h_intent(dentry, bindex,
				      args->nd->intent.open.file, ndx.nd_file);
		if (unlikely(err)) {
			fput(ndx.nd_file);
			set_h_dptr(dentry, bindex, NULL);
			//todo: update bstart and bend
			h_dentry = ERR_PTR(err);
			goto out;
		}
	}
#endif

	if (!h_inode || !S_ISDIR(h_inode->i_mode) || !wh_able)
		return h_dentry; /* success */

	vfsub_i_lock_nested(h_inode, AuLsc_I_CHILD);
	opq = au_test_diropq(h_dentry, &ndx);
	//if (LktrCond) opq = -1;
	vfsub_i_unlock(h_inode);
	if (opq > 0)
		set_dbdiropq(dentry, bindex);
	else if (unlikely(opq < 0)) {
		set_h_dptr(dentry, bindex, NULL);
		h_dentry = ERR_PTR(opq);
	}
	goto out;

 out_neg:
	dput(h_dentry);
	h_dentry = NULL;
 out:
	AuTraceErrPtr(h_dentry);
	return h_dentry;
}

/*
 * returns the number of hidden positive dentries,
 * otherwise an error.
 */
int au_lkup_dentry(struct dentry *dentry, aufs_bindex_t bstart, mode_t type,
		   struct nameidata *nd)
{
	int npositive, err;
	struct dentry *parent;
	aufs_bindex_t bindex, btail;
	const struct qstr *name = &dentry->d_name;
	struct qstr whname;
	struct super_block *sb;
	struct au_do_lookup_args args = {
		.type	= type,
		.nd	= nd
	};

	LKTRTrace("%.*s, b%d, type 0%o\n", AuLNPair(name), bstart, type);
	AuDebugOn(bstart < 0 || IS_ROOT(dentry));

	/* dir may not be locked */
	parent = dget_parent(dentry);

#ifndef CONFIG_AUFS_ROBR
	err = -EPERM;
	if (unlikely(!strncmp(name->name, AUFS_WH_PFX, AUFS_WH_PFX_LEN)))
		goto out;
#endif

	err = au_alloc_whname(name->name, name->len, &whname);
	//if (LktrCond) {au_free_whname(&whname); err = -1;}
	if (unlikely(err))
		goto out;

	sb = dentry->d_sb;
	args.dlgt = !!au_need_dlgt(sb);
	args.allow_neg = !type;
	npositive = 0;
	btail = dbtaildir(parent);
	for (bindex = bstart; bindex <= btail; bindex++) {
		struct dentry *h_parent, *h_dentry;
		struct inode *h_inode, *h_dir;

		h_dentry = au_h_dptr_i(dentry, bindex);
		if (h_dentry) {
			if (h_dentry->d_inode)
				npositive++;
			if (type != S_IFDIR)
				break;
			continue;
		}
		h_parent = au_h_dptr_i(parent, bindex);
		if (!h_parent)
			continue;
		h_dir = h_parent->d_inode;
		if (!h_dir || !S_ISDIR(h_dir->i_mode))
			continue;

		vfsub_i_lock_nested(h_dir, AuLsc_I_PARENT);
		h_dentry = au_do_lookup(h_parent, dentry, bindex, &whname,
					&args);
		// do not dput for testing
		//if (LktrCond) {h_dentry = ERR_PTR(-1);}
		vfsub_i_unlock(h_dir);
		err = PTR_ERR(h_dentry);
		if (IS_ERR(h_dentry))
			goto out_wh;
		args.allow_neg = 0;

		if (dbwh(dentry) >= 0)
			break;
		if (!h_dentry)
			continue;
		h_inode = h_dentry->d_inode;
		if (!h_inode)
			continue;
		npositive++;
		if (!args.type)
			args.type = h_inode->i_mode & S_IFMT;
		if (args.type != S_IFDIR)
			break;
		else if (dbdiropq(dentry) >= 0)
			break;
	}

	if (npositive) {
		LKTRLabel(positive);
		au_update_dbstart(dentry);
	}
	err = npositive;

 out_wh:
	au_free_whname(&whname);
 out:
	dput(parent);
	AuTraceErr(err);
	return err;
}

struct dentry *au_sio_lkup_one(const char *name, struct dentry *parent, int len,
			       struct aufs_ndx *ndx)
{
	struct dentry *dentry;
	int wkq_err;

	LKTRTrace("%.*s/%.*s\n", AuDLNPair(parent), len, name);

	if (!au_test_perm(parent->d_inode, MAY_EXEC, ndx->dlgt))
		dentry = au_lkup_one(name, parent, len, ndx);
	else {
		// ugly
		int dlgt = ndx->dlgt;
		struct au_lkup_one_args args = {
			.errp	= &dentry,
			.name	= name,
			.parent	= parent,
			.len	= len,
			.ndx	= ndx
		};

		ndx->dlgt = 0;
		wkq_err = au_wkq_wait(au_call_lkup_one, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			dentry = ERR_PTR(wkq_err);
		ndx->dlgt = dlgt;
	}

	AuTraceErrPtr(dentry);
	return dentry;
}

/*
 * lookup @dentry on @bindex which should be negative.
 */
int au_lkup_neg(struct dentry *dentry, aufs_bindex_t bindex)
{
	int err;
	struct dentry *parent, *h_parent, *h_dentry;
	struct inode *h_dir;
	struct aufs_ndx ndx = {
		.nd	= NULL,
		//.br	= NULL
	};
	struct super_block *sb;

	LKTRTrace("%.*s, b%d\n", AuDLNPair(dentry), bindex);
	/* dir may not be locked */
	parent = dget_parent(dentry);
	AuDebugOn(!parent || !parent->d_inode
		  || !S_ISDIR(parent->d_inode->i_mode));
	h_parent = au_h_dptr_i(parent, bindex);
	AuDebugOn(!h_parent);
	h_dir = h_parent->d_inode;
	AuDebugOn(!h_dir || !S_ISDIR(h_dir->i_mode));

	sb = dentry->d_sb;
	ndx.nfsmnt = au_nfsmnt(sb, bindex);
	ndx.dlgt = au_need_dlgt(sb);
	h_dentry = au_sio_lkup_one(dentry->d_name.name, h_parent,
				   dentry->d_name.len, &ndx);
	//if (LktrCond) {dput(h_dentry); h_dentry = ERR_PTR(-1);}
	err = PTR_ERR(h_dentry);
	if (IS_ERR(h_dentry))
		goto out;
	if (unlikely(h_dentry->d_inode)) {
		err = -EIO;
		AuIOErr("b%d %.*s should be negative.\n",
			bindex, AuDLNPair(h_dentry));
		dput(h_dentry);
		goto out;
	}

	if (bindex < dbstart(dentry))
		set_dbstart(dentry, bindex);
	if (dbend(dentry) < bindex)
		set_dbend(dentry, bindex);
	set_h_dptr(dentry, bindex, h_dentry);
	err = 0;

 out:
	dput(parent);
	AuTraceErr(err);
	return err;
}

/*
 * returns the number of found hidden positive dentries,
 * otherwise an error.
 */
int au_refresh_hdentry(struct dentry *dentry, mode_t type)
{
	int npositive, new_sz;
	struct aufs_dinfo *dinfo;
	struct super_block *sb;
	struct dentry *parent;
	aufs_bindex_t bindex, parent_bend, parent_bstart, bwh, bdiropq, bend;
	struct aufs_hdentry *p;

	LKTRTrace("%.*s, type 0%o\n", AuDLNPair(dentry), type);
	DiMustWriteLock(dentry);
	sb = dentry->d_sb;
	AuDebugOn(IS_ROOT(dentry));
	parent = dget_parent(dentry);
	AuDebugOn(au_digen(parent) != au_sigen(sb));

	npositive = -ENOMEM;
	new_sz = sizeof(*dinfo->di_hdentry) * (sbend(sb) + 1);
	dinfo = dtodi(dentry);
	p = au_kzrealloc(dinfo->di_hdentry, sizeof(*p) * (dinfo->di_bend + 1),
			 new_sz, GFP_KERNEL);
	//p = NULL;
	if (unlikely(!p))
		goto out;
	dinfo->di_hdentry = p;

	bend = dinfo->di_bend;
	bwh = dinfo->di_bwh;
	bdiropq = dinfo->di_bdiropq;
	p += dinfo->di_bstart;
	for (bindex = dinfo->di_bstart; bindex <= bend; bindex++, p++) {
		struct dentry *hd, *hdp;
		struct aufs_hdentry tmp, *q;
		aufs_bindex_t new_bindex;

		hd = p->hd_dentry;
		if (!hd)
			continue;
		hdp = dget_parent(hd);
		if (hdp == au_h_dptr_i(parent, bindex)) {
			dput(hdp);
			continue;
		}

		new_bindex = au_find_dbindex(parent, hdp);
		dput(hdp);
		AuDebugOn(new_bindex == bindex);
		if (dinfo->di_bwh == bindex)
			bwh = new_bindex;
		if (dinfo->di_bdiropq == bindex)
			bdiropq = new_bindex;
		if (new_bindex < 0) { // test here
			hdput(p);
			p->hd_dentry = NULL;
			continue;
		}
		/* swap two hidden dentries, and loop again */
		q = dinfo->di_hdentry + new_bindex;
		tmp = *q;
		*q = *p;
		*p = tmp;
		if (tmp.hd_dentry) {
			bindex--;
			p--;
		}
	}

	// test here
	dinfo->di_bwh = -1;
	if (unlikely(bwh >= 0 && bwh <= sbend(sb) && sbr_whable(sb, bwh)))
		dinfo->di_bwh = bwh;
	dinfo->di_bdiropq = -1;
	if (unlikely(bdiropq >= 0 && bdiropq <= sbend(sb)
		     && sbr_whable(sb, bdiropq)))
		dinfo->di_bdiropq = bdiropq;
	parent_bend = dbend(parent);
	p = dinfo->di_hdentry;
	for (bindex = 0; bindex <= parent_bend; bindex++, p++)
		if (p->hd_dentry) {
			dinfo->di_bstart = bindex;
			break;
		}
	p = dinfo->di_hdentry + parent_bend;
	//for (bindex = parent_bend; bindex > dinfo->di_bstart; bindex--, p--)
	for (bindex = parent_bend; bindex >= 0; bindex--, p--)
		if (p->hd_dentry) {
			dinfo->di_bend = bindex;
			break;
		}

	npositive = 0;
	parent_bstart = dbstart(parent);
	if (type != S_IFDIR && dinfo->di_bstart == parent_bstart)
		goto out_dgen; /* success */

	npositive = au_lkup_dentry(dentry, parent_bstart, type, /*nd*/NULL);
	//if (LktrCond) npositive = -1;
	if (npositive < 0)
		goto out;
	if (unlikely(dinfo->di_bwh >= 0 && dinfo->di_bwh <= dinfo->di_bstart))
		d_drop(dentry);

 out_dgen:
	au_update_digen(dentry);
 out:
	dput(parent);
	AuTraceErr(npositive);
	return npositive;
}

static int h_d_revalidate(struct dentry *dentry, struct inode *inode,
			  struct nameidata *nd, int do_udba)
{
	int err, plus, locked, unhashed, is_root, h_plus, valid;
	struct nameidata fake_nd, *p;
	aufs_bindex_t bindex, btail, bstart, ibs, ibe;
	struct super_block *sb;
	struct inode *first, *h_inode, *h_cached_inode;
	umode_t mode, h_mode;
	struct dentry *h_dentry;
	int (*reval)(struct dentry *, struct nameidata *);
	struct qstr *name;

	LKTRTrace("%.*s, nd %d\n", AuDLNPair(dentry), !!nd);
	AuDebugOn(inode && au_digen(dentry) != au_iigen(inode));

	err = 0;
	sb = dentry->d_sb;
	plus = 0;
	mode = 0;
	first = NULL;
	ibs = -1;
	ibe = -1;
	unhashed = d_unhashed(dentry);
	is_root = IS_ROOT(dentry);
	name = &dentry->d_name;

	/*
	 * Theoretically, REVAL test should be unnecessary in case of INOTIFY.
	 * But inotify doesn't fire some necessary events,
	 *	IN_ATTRIB for atime/nlink/pageio
	 *	IN_DELETE for NFS dentry
	 * Let's do REVAL test too.
	 */
	if (do_udba && inode) {
		mode = (inode->i_mode & S_IFMT);
		plus = (inode->i_nlink > 0);
		first = au_h_iptr(inode);
		ibs = ibstart(inode);
		ibe = ibend(inode);
	}

	bstart = dbstart(dentry);
	btail = bstart;
	if (inode && S_ISDIR(inode->i_mode))
		btail = dbtaildir(dentry);
	locked = 0;
#ifndef CONFIG_AUFS_FAKE_DM
	if (nd && dentry != nd->dentry) {
		di_read_lock_parent(nd->dentry, 0);
		locked = 1;
	}
#endif
	for (bindex = bstart; bindex <= btail; bindex++) {
		h_dentry = au_h_dptr_i(dentry, bindex);
		if (unlikely(!h_dentry))
			continue;
		if (unlikely(do_udba
			     && !is_root
			     && (unhashed != d_unhashed(h_dentry)
//#if 1
				 || name->len != h_dentry->d_name.len
				 || memcmp(name->name, h_dentry->d_name.name,
					   name->len)
//#endif
				     ))) {
			LKTRTrace("unhash 0x%x 0x%x, %.*s %.*s\n",
				  unhashed, d_unhashed(h_dentry),
				  AuDLNPair(dentry), AuDLNPair(h_dentry));
			goto err;
		}

		reval = NULL;
		if (h_dentry->d_op)
			reval = h_dentry->d_op->d_revalidate;
		if (unlikely(reval)) {
			//LKTRLabel(hidden reval);
			if (nd) {
				memcpy(&fake_nd, nd, sizeof(*nd));
				err = au_fake_intent(&fake_nd,
						     sbr_perm(sb, bindex));
				if (unlikely(err))
					goto err;
			}
			p = au_fake_dm(&fake_nd, nd, sb, bindex);
			AuDebugOn(IS_ERR(p));
			AuDebugOn(nd && p != &fake_nd);
			LKTRTrace("b%d\n", bindex);

			/* it may return tri-state */
			valid = reval(h_dentry, p);
			if (unlikely(valid < 0))
				err = valid;
			else if (!valid)
				err = -EINVAL;
			else
				AuDebugOn(err);

			if (p) {
				int e;
				AuDebugOn(!nd);
				e = au_hin_after_reval(p, dentry, bindex,
						       nd->intent.open.file);
				au_update_fuse_h_inode(p->mnt, h_dentry);
				/*ignore*/
				if (unlikely(e && !err))
					err = e;
			} else
				au_update_fuse_h_inode(NULL, h_dentry);
			/*ignore*/
			au_fake_dm_release(p);
			if (unlikely(err)) {
				//AuDbg("here\n");
				/* do not goto err, to keep the errno */
				break;
			}
		}

		if (unlikely(!do_udba))
			continue;

		/* UDBA tests */
		h_inode = h_dentry->d_inode;
		if (unlikely(!!inode != !!h_inode)) {
			//AuDbg("here\n");
			goto err;
		}

		h_plus = plus;
		h_mode = mode;
		h_cached_inode = h_inode;
		if (h_inode) {
			h_mode = (h_inode->i_mode & S_IFMT);
			h_plus = (h_inode->i_nlink > 0);
			//AuDbgInode(inode);
			//AuDbgInode(h_inode);
		}
		if (inode && ibs <= bindex && bindex <= ibe)
			h_cached_inode = au_h_iptr_i(inode, bindex);

		LKTRTrace("{%d, 0%o, %d}, h{%d, 0%o, %d}\n",
			  plus, mode, !!h_cached_inode,
			  h_plus, h_mode, !!h_inode);
		if (unlikely(plus != h_plus
			     || mode != h_mode
			     || h_cached_inode != h_inode))
			goto err;
		continue;

	err:
		err = -EINVAL;
		break;
	}
#ifndef CONFIG_AUFS_FAKE_DM
	if (unlikely(locked))
		di_read_unlock(nd->dentry, 0);
#endif

	/*
	 * judging by timestamps is meaningless since some filesystem uses
	 * CURRENT_TIME_SEC instead of CURRENT_TIME.
	 */
	/*
	 * NFS may stop IN_DELETE because of DCACHE_NFSFS_RENAMED.
	 */

#if 0 // debug
	if (!err)
		au_update_from_fuse(inode);
	if (unlikely(!err && udba && first))
		au_cpup_attr_all(inode);
#endif

	AuTraceErr(err);
	return err;
}

static int simple_reval_dpath(struct dentry *dentry, au_gen_t sgen)
{
	int err;
	mode_t type;
	struct dentry *parent;
	struct inode *inode;

	LKTRTrace("%.*s, sgen %d\n", AuDLNPair(dentry), sgen);
	SiMustAnyLock(dentry->d_sb);
	DiMustWriteLock(dentry);
	inode = dentry->d_inode;
	AuDebugOn(!inode);

	if (au_digen(dentry) == sgen)
		return 0;

	parent = dget_parent(dentry);
	di_read_lock_parent(parent, AuLock_IR);
	AuDebugOn(au_digen(parent) != sgen);
#ifdef CONFIG_AUFS_DEBUG
	{
		int i, j;
		struct au_dcsub_pages dpages;
		struct au_dpage *dpage;
		struct dentry **dentries;

		err = au_dpages_init(&dpages, GFP_TEMPORARY);
		AuDebugOn(err);
		err = au_dcsub_pages_rev(&dpages, parent, /*do_include*/1, NULL,
					 NULL);
		AuDebugOn(err);
		for (i = dpages.ndpage - 1; !err && i >= 0; i--) {
			dpage = dpages.dpages + i;
			dentries = dpage->dentries;
			for (j = dpage->ndentry - 1; !err && j >= 0; j--)
				AuDebugOn(au_digen(dentries[j]) != sgen);
		}
		au_dpages_free(&dpages);
	}
#endif
	type = (inode->i_mode & S_IFMT);
	/* returns a number of positive dentries */
	err = au_refresh_hdentry(dentry, type);
	if (err >= 0)
		err = au_refresh_hinode(inode, dentry);
	di_read_unlock(parent, AuLock_IR);
	dput(parent);
	AuTraceErr(err);
	return err;
}

int au_reval_dpath(struct dentry *dentry, au_gen_t sgen)
{
	int err;
	struct dentry *d, *parent;
	struct inode *inode;

	LKTRTrace("%.*s, sgen %d\n", AuDLNPair(dentry), sgen);
	AuDebugOn(!dentry->d_inode);
	DiMustWriteLock(dentry);

	if (!stosi(dentry->d_sb)->si_failed_refresh_dirs)
		return simple_reval_dpath(dentry, sgen);

	/* slow loop, keep it simple and stupid */
	/* cf: au_cpup_dirs() */
	err = 0;
	parent = NULL;
	while (au_digen(dentry) != sgen) {
		d = dentry;
		while (1) {
			dput(parent);
			parent = dget_parent(d);
			if (au_digen(parent) == sgen)
				break;
			d = parent;
		}

		inode = d->d_inode;
		if (d != dentry) {
			//vfsub_i_lock(inode);
			di_write_lock_child(d);
		}

		/* someone might update our dentry while we were sleeping */
		if (au_digen(d) != sgen) {
			di_read_lock_parent(parent, AuLock_IR);
			/* returns a number of positive dentries */
			err = au_refresh_hdentry(d, inode->i_mode & S_IFMT);
			//err = -1;
			if (err >= 0)
				err = au_refresh_hinode(inode, d);
			//err = -1;
			di_read_unlock(parent, AuLock_IR);
		}

		if (d != dentry) {
			di_write_unlock(d);
			//vfsub_i_unlock(inode);
		}
		dput(parent);
		if (unlikely(err))
			break;
	}

	AuTraceErr(err);
	return err;
}

/*
 * THIS IS A BOOLEAN FUNCTION: returns 1 if valid, 0 otherwise.
 * nfsd passes NULL as nameidata.
 */
static int aufs_d_revalidate(struct dentry *dentry, struct nameidata *nd)
{
	int valid, err, do_udba;
	struct super_block *sb;
	au_gen_t sgen;
	struct inode *inode;
	struct nameidata tmp_nd, *ndp;
	struct aufs_sbinfo *sbinfo;
	//todo: no more lower nameidata, only here. re-use it.

	LKTRTrace("dentry %.*s\n", AuDLNPair(dentry));
	if (nd && nd->dentry)
		LKTRTrace("nd{%.*s, 0x%x}\n", AuDLNPair(nd->dentry), nd->flags);
	//dir case: AuDebugOn(dentry->d_parent != nd->dentry);
	//remove failure case:AuDebugOn(!IS_ROOT(dentry) && d_unhashed(dentry));
	AuDebugOn(!dentry->d_fsdata);
	//AuDbgDentry(dentry);

	err = -EINVAL;
	inode = dentry->d_inode;
	sb = dentry->d_sb;
	si_read_lock(sb, AuLock_FLUSH);

	sgen = au_sigen(sb);
	if (au_digen(dentry) == sgen)
		di_read_lock_child(dentry, !AuLock_IR);
	else {
		AuDebugOn(IS_ROOT(dentry));
#ifdef ForceInotify
		AuDbg("UDBA or digen, %.*s\n", AuDLNPair(dentry));
#endif
		//di_read_lock_child(dentry, AuLock_IR);err = -EINVAL; goto out;
		//vfsub_i_lock(inode);
		di_write_lock_child(dentry);
		if (inode)
			err = au_reval_dpath(dentry, sgen);
		//err = -1;
		di_downgrade_lock(dentry, AuLock_IR);
		//vfsub_i_unlock(inode);
		if (unlikely(err))
			goto out;
		ii_read_unlock(inode);
		AuDebugOn(au_iigen(inode) != sgen);
	}

	if (inode) {
		if (au_iigen(inode) == sgen)
			ii_read_lock_child(inode);
		else {
			AuDebugOn(IS_ROOT(dentry));
#ifdef ForceInotify
			AuDbg("UDBA or survived, %.*s\n", AuDLNPair(dentry));
			//au_debug_on();
			//AuDbgInode(inode);
			//au_debug_off();
#endif
			//ii_read_lock_child(inode);err = -EINVAL; goto out;
			ii_write_lock_child(inode);
			err = au_refresh_hinode(inode, dentry);
			ii_downgrade_lock(inode);
			if (unlikely(err))
				goto out;
			AuDebugOn(au_iigen(inode) != sgen);
		}
	}

#if 0 // fix it
	/* parent dir i_nlink is not updated in the case of setattr */
	if (S_ISDIR(inode->i_mode)) {
		vfsub_i_lock(inode);
		ii_write_lock(inode);
		au_cpup_attr_nlink(inode);
		ii_write_unlock(inode);
		vfsub_i_unlock(inode);
	}
#endif

	err = -EINVAL;
	sbinfo = stosi(sb);
	do_udba = (AuFlag(sbinfo, f_udba) != AuUdba_NONE);
	if (do_udba && inode && ibstart(inode) >= 0
	    && au_test_higen(inode, au_h_iptr(inode)))
		goto out;
	ndp = au_dup_nd(sbinfo, &tmp_nd, nd);
	err = h_d_revalidate(dentry, inode, ndp, do_udba);
	//err = -1;

 out:
#if 0
	AuDbgDentry(dentry);
	if (nd && (nd->flags & LOOKUP_OPEN)) {
		LKTRTrace("intent.open.file %p\n", nd->intent.open.file);
		if (nd->intent.open.file && nd->intent.open.file->f_dentry)
			AuDbgFile(nd->intent.open.file);
	}
#endif
	au_store_fmode_exec(nd, inode);

	if (inode)
		ii_read_unlock(inode);
	di_read_unlock(dentry, !AuLock_IR);
	si_read_unlock(sb);
	AuTraceErr(err);
	valid = !err;
	if (!valid)
		LKTRTrace("%.*s invalid\n", AuDLNPair(dentry));
	return valid;
}

static void aufs_d_release(struct dentry *dentry)
{
	struct aufs_dinfo *dinfo;
	aufs_bindex_t bend, bindex;

	LKTRTrace("%.*s\n", AuDLNPair(dentry));
	AuDebugOn(!d_unhashed(dentry));

	dinfo = dentry->d_fsdata;
	if (unlikely(!dinfo))
		return;

	/* dentry may not be revalidated */
	bindex = dinfo->di_bstart;
	if (bindex >= 0) {
		struct aufs_hdentry *p;
		bend = dinfo->di_bend;
		AuDebugOn(bend < bindex);
		p = dinfo->di_hdentry + bindex;
		while (bindex++ <= bend) {
			if (p->hd_dentry)
				hdput(p);
			p++;
		}
	}
	kfree(dinfo->di_hdentry);
	au_cache_free_dinfo(dinfo);
}

struct dentry_operations aufs_dop = {
	.d_revalidate	= aufs_d_revalidate,
	.d_release	= aufs_d_release,
	/* never use d_delete, especially in case of nfs server */
	//.d_delete	= aufs_d_delete
	//.d_iput		= aufs_d_iput
};

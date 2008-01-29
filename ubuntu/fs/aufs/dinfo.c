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

/* $Id: dinfo.c,v 1.37 2007/11/05 01:37:14 sfjro Exp $ */

#include "aufs.h"

int au_alloc_dinfo(struct dentry *dentry)
{
	struct aufs_dinfo *dinfo;
	struct super_block *sb;
	int nbr;

	LKTRTrace("%.*s\n", AuDLNPair(dentry));
	AuDebugOn(dentry->d_fsdata);

	dinfo = au_cache_alloc_dinfo();
	//if (LktrCond) {au_cache_free_dinfo(dinfo); dinfo = NULL;}
	if (dinfo) {
		sb = dentry->d_sb;
		nbr = sbend(sb) + 1;
		if (unlikely(nbr <= 0))
			nbr = 1;
		dinfo->di_hdentry = kcalloc(nbr, sizeof(*dinfo->di_hdentry),
					    GFP_KERNEL);
		//if (LktrCond)
		//{kfree(dinfo->di_hdentry); dinfo->di_hdentry = NULL;}
		if (dinfo->di_hdentry) {
			au_h_dentry_init_all(dinfo->di_hdentry, nbr);
			atomic_set(&dinfo->di_generation, au_sigen(sb));
			//smp_mb(); /* atomic_set */
			rw_init_wlock_nested(&dinfo->di_rwsem, AuLsc_DI_PARENT);
			dinfo->di_bstart = -1;
			dinfo->di_bend = -1;
			dinfo->di_bwh = -1;
			dinfo->di_bdiropq = -1;

			dentry->d_fsdata = dinfo;
			dentry->d_op = &aufs_dop;
			return 0; /* success */
		}
		au_cache_free_dinfo(dinfo);
	}
	AuTraceErr(-ENOMEM);
	return -ENOMEM;
}

struct aufs_dinfo *dtodi(struct dentry *dentry)
{
	struct aufs_dinfo *dinfo = dentry->d_fsdata;
	AuDebugOn(!dinfo
		 || !dinfo->di_hdentry
		 /* || stosi(dentry->d_sb)->si_bend < dinfo->di_bend */
		 || dinfo->di_bend < dinfo->di_bstart
		 /* dbwh can be outside of this range */
		 || (0 <= dinfo->di_bdiropq
		     && (dinfo->di_bdiropq < dinfo->di_bstart
			 /* || dinfo->di_bend < dinfo->di_bdiropq */))
		);
	return dinfo;
}

/* ---------------------------------------------------------------------- */

static void do_ii_write_lock(struct inode *inode, unsigned int lsc)
{
	switch (lsc) {
	case AuLsc_DI_CHILD:
		ii_write_lock_child(inode);
		break;
	case AuLsc_DI_CHILD2:
		ii_write_lock_child2(inode);
		break;
	case AuLsc_DI_CHILD3:
		ii_write_lock_child3(inode);
		break;
	case AuLsc_DI_PARENT:
		//AuDebugOn(!S_ISDIR(inode->i_mode));
		ii_write_lock_parent(inode);
		break;
	case AuLsc_DI_PARENT2:
		//AuDebugOn(!S_ISDIR(inode->i_mode));
		ii_write_lock_parent2(inode);
		break;
	case AuLsc_DI_PARENT3:
		//AuDebugOn(!S_ISDIR(inode->i_mode));
		ii_write_lock_parent3(inode);
		break;
	default:
		BUG();
	}
}

static void do_ii_read_lock(struct inode *inode, unsigned int lsc)
{
	switch (lsc) {
	case AuLsc_DI_CHILD:
		ii_read_lock_child(inode);
		break;
	case AuLsc_DI_CHILD2:
		ii_read_lock_child2(inode);
		break;
	case AuLsc_DI_CHILD3:
		ii_read_lock_child3(inode);
		break;
	case AuLsc_DI_PARENT:
		//AuDebugOn(!S_ISDIR(inode->i_mode));
		ii_read_lock_parent(inode);
		break;
	case AuLsc_DI_PARENT2:
		//AuDebugOn(!S_ISDIR(inode->i_mode));
		ii_read_lock_parent2(inode);
		break;
	case AuLsc_DI_PARENT3:
		//AuDebugOn(!S_ISDIR(inode->i_mode));
		ii_read_lock_parent3(inode);
		break;
	default:
		BUG();
	}
}

void di_read_lock(struct dentry *d, int flags, unsigned int lsc)
{
	SiMustAnyLock(d->d_sb);
	// todo: always nested?
	rw_read_lock_nested(&dtodi(d)->di_rwsem, lsc);
	if (d->d_inode) {
		if (flags & AuLock_IW)
			do_ii_write_lock(d->d_inode, lsc);
		else if (flags & AuLock_IR)
			do_ii_read_lock(d->d_inode, lsc);
	}
}

void di_read_unlock(struct dentry *d, int flags)
{
	SiMustAnyLock(d->d_sb);
	if (d->d_inode) {
		if (flags & AuLock_IW)
			ii_write_unlock(d->d_inode);
		else if (flags & AuLock_IR)
			ii_read_unlock(d->d_inode);
	}
	rw_read_unlock(&dtodi(d)->di_rwsem);
}

void di_downgrade_lock(struct dentry *d, int flags)
{
	SiMustAnyLock(d->d_sb);
	rw_dgrade_lock(&dtodi(d)->di_rwsem);
	if (d->d_inode && (flags & AuLock_IR))
		ii_downgrade_lock(d->d_inode);
}

void di_write_lock(struct dentry *d, unsigned int lsc)
{
	SiMustAnyLock(d->d_sb);
	// todo: always nested?
	rw_write_lock_nested(&dtodi(d)->di_rwsem, lsc);
	if (d->d_inode)
		do_ii_write_lock(d->d_inode, lsc);
}

void di_write_unlock(struct dentry *d)
{
	SiMustAnyLock(d->d_sb);
	if (d->d_inode)
		ii_write_unlock(d->d_inode);
	rw_write_unlock(&dtodi(d)->di_rwsem);
}

void di_write_lock2_child(struct dentry *d1, struct dentry *d2, int isdir)
{
	AuTraceEnter();
	AuDebugOn(d1 == d2
		  || d1->d_inode == d2->d_inode
		  || d1->d_sb != d2->d_sb);

	if (isdir && au_test_subdir(d1, d2)) {
		di_write_lock_child(d1);
		di_write_lock_child2(d2);
	} else {
		/* there should be no races */
		di_write_lock_child(d2);
		di_write_lock_child2(d1);
	}
}

void di_write_lock2_parent(struct dentry *d1, struct dentry *d2, int isdir)
{
	AuTraceEnter();
	AuDebugOn(d1 == d2
		  || d1->d_inode == d2->d_inode
		  || d1->d_sb != d2->d_sb);

	if (isdir && au_test_subdir(d1, d2)) {
		di_write_lock_parent(d1);
		di_write_lock_parent2(d2);
	} else {
		/* there should be no races */
		di_write_lock_parent(d2);
		di_write_lock_parent2(d1);
	}
}

void di_write_unlock2(struct dentry *d1, struct dentry *d2)
{
	di_write_unlock(d1);
	if (d1->d_inode == d2->d_inode)
		rw_write_unlock(&dtodi(d2)->di_rwsem);
	else
		di_write_unlock(d2);
}

/* ---------------------------------------------------------------------- */

aufs_bindex_t dbstart(struct dentry *dentry)
{
	DiMustAnyLock(dentry);
	return dtodi(dentry)->di_bstart;
}

aufs_bindex_t dbend(struct dentry *dentry)
{
	DiMustAnyLock(dentry);
	return dtodi(dentry)->di_bend;
}

aufs_bindex_t dbwh(struct dentry *dentry)
{
	DiMustAnyLock(dentry);
	return dtodi(dentry)->di_bwh;
}

aufs_bindex_t dbdiropq(struct dentry *dentry)
{
	DiMustAnyLock(dentry);
	AuDebugOn(dentry->d_inode
		  && dentry->d_inode->i_mode
		  && !S_ISDIR(dentry->d_inode->i_mode));
	return dtodi(dentry)->di_bdiropq;
}

struct dentry *au_h_dptr_i(struct dentry *dentry, aufs_bindex_t bindex)
{
	struct dentry *d;

	DiMustAnyLock(dentry);
	if (dbstart(dentry) < 0 || bindex < dbstart(dentry))
		return NULL;
	AuDebugOn(bindex < 0
		  /* || bindex > sbend(dentry->d_sb) */);
	d = dtodi(dentry)->di_hdentry[0 + bindex].hd_dentry;
	AuDebugOn(d && (atomic_read(&d->d_count) <= 0));
	return d;
}

struct dentry *au_h_dptr(struct dentry *dentry)
{
	return au_h_dptr_i(dentry, dbstart(dentry));
}

aufs_bindex_t dbtail(struct dentry *dentry)
{
	aufs_bindex_t bend, bwh;

	bend = dbend(dentry);
	if (0 <= bend) {
		bwh = dbwh(dentry);
		//AuDebugOn(bend < bwh);
		if (!bwh)
			return bwh;
		if (0 < bwh && bwh < bend)
			return bwh - 1;
	}
	return bend;
}

aufs_bindex_t dbtaildir(struct dentry *dentry)
{
	aufs_bindex_t bend, bopq;

	AuDebugOn(dentry->d_inode
		  && dentry->d_inode->i_mode
		  && !S_ISDIR(dentry->d_inode->i_mode));

	bend = dbtail(dentry);
	if (0 <= bend) {
		bopq = dbdiropq(dentry);
		AuDebugOn(bend < bopq);
		if (0 <= bopq && bopq < bend)
			bend = bopq;
	}
	return bend;
}

aufs_bindex_t dbtail_generic(struct dentry *dentry)
{
	struct inode *inode;

	inode = dentry->d_inode;
	if (inode && S_ISDIR(inode->i_mode))
		return dbtaildir(dentry);
	else
		return dbtail(dentry);
}

/* ---------------------------------------------------------------------- */

// hard/soft set
void set_dbstart(struct dentry *dentry, aufs_bindex_t bindex)
{
	DiMustWriteLock(dentry);
	AuDebugOn(sbend(dentry->d_sb) < bindex);
	/* */
	dtodi(dentry)->di_bstart = bindex;
}

void set_dbend(struct dentry *dentry, aufs_bindex_t bindex)
{
	DiMustWriteLock(dentry);
	AuDebugOn(sbend(dentry->d_sb) < bindex
		  || bindex < dbstart(dentry));
	dtodi(dentry)->di_bend = bindex;
}

void set_dbwh(struct dentry *dentry, aufs_bindex_t bindex)
{
	DiMustWriteLock(dentry);
	AuDebugOn(sbend(dentry->d_sb) < bindex);
	/* dbwh can be outside of bstart - bend range */
	dtodi(dentry)->di_bwh = bindex;
}

void set_dbdiropq(struct dentry *dentry, aufs_bindex_t bindex)
{
	DiMustWriteLock(dentry);
	AuDebugOn(sbend(dentry->d_sb) < bindex);
	AuDebugOn((bindex >= 0
		   && (bindex < dbstart(dentry) || dbend(dentry) < bindex))
		  || (dentry->d_inode
		      && dentry->d_inode->i_mode
		      && !S_ISDIR(dentry->d_inode->i_mode)));
	dtodi(dentry)->di_bdiropq = bindex;
}

static void hintent_put(struct aufs_hdentry *hd)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) \
	&& !defined(AuNoNfsBranch)
	struct aufs_hdintent *hdi, *tmp;
	struct file *hf;

	if (unlikely(hd->hd_intent_list)) {
		// no spin lock
		list_for_each_entry_safe(hdi, tmp, hd->hd_intent_list,
					 hdi_list) {
			LKTRTrace("hdi %p\n", hdi);
			hf = hdi->hdi_file[AuIntent_BRANCH];
			if (unlikely(hf))
				fput(hf);
			//list_del(&hdi->hdi_list);
			kfree(hdi);
		}
		kfree(hd->hd_intent_list);
	}
#endif
}

void hdput(struct aufs_hdentry *hd)
{
	hintent_put(hd);
	dput(hd->hd_dentry);
}

void set_h_dptr(struct dentry *dentry, aufs_bindex_t bindex,
		struct dentry *h_dentry)
{
	struct aufs_hdentry *hd = dtodi(dentry)->di_hdentry + bindex;
	DiMustWriteLock(dentry);
	AuDebugOn(bindex < dtodi(dentry)->di_bstart
		  || bindex > dtodi(dentry)->di_bend
		  || (h_dentry && atomic_read(&h_dentry->d_count) <= 0)
		  || (h_dentry && hd->hd_dentry)
		);
	if (hd->hd_dentry)
		hdput(hd);
	hd->hd_dentry = h_dentry;
}

/* ---------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) \
	&& !defined(AuNoNfsBranch)
static struct file *au_find_h_intent(struct aufs_hdentry *hd, struct file *file)
{
	struct file *h_file, *hf;
	struct aufs_hdintent *hdi, *tmp;

	LKTRTrace("%.*s\n", AuDLNPair(hd->hd_dentry));

	h_file = NULL;
	spin_lock(&hd->hd_lock);
	list_for_each_entry_safe(hdi, tmp, hd->hd_intent_list, hdi_list) {
		hf = hdi->hdi_file[AuIntent_BRANCH];
		if (hdi->hdi_file[AuIntent_AUFS] == file
		    && hf->f_dentry == hd->hd_dentry) {
			h_file = hf;
			list_del(&hdi->hdi_list);
			kfree(hdi);
			break;
		}
	}
	spin_unlock(&hd->hd_lock);

	return h_file;
}

struct file *au_h_intent(struct dentry *dentry, aufs_bindex_t bindex,
			 struct file *file)
{
	struct file *h_file;
	struct aufs_hdentry *hd = dtodi(dentry)->di_hdentry + bindex;

	LKTRTrace("%.*s, b%d, f %p\n", AuDLNPair(dentry), bindex, file);
	DiMustAnyLock(dentry);
	AuDebugOn(bindex < dtodi(dentry)->di_bstart
		  || bindex > dtodi(dentry)->di_bend);

	h_file = NULL;
	if (!hd->hd_intent_list || !file)
		return h_file; /* success */

	//AuDebugOn(au_test_wkq(current));
	h_file = au_find_h_intent(hd, file);
	//AuDbgFile(h_file);
	return h_file;
}

int au_set_h_intent(struct dentry *dentry, aufs_bindex_t bindex,
		    struct file *file, struct file *h_file)
{
	int err;
	struct aufs_hdentry *hd = dtodi(dentry)->di_hdentry + bindex;
	struct aufs_hdintent *hdi;
	struct file *hf;

	LKTRTrace("%.*s, b%d, f %p\n", AuDLNPair(dentry), bindex, file);
	/* d_revalidate() holds read_lock */
	//DiMustWriteLock(dentry);
	AuDebugOn(bindex < dtodi(dentry)->di_bstart
		  || bindex > dtodi(dentry)->di_bend
		  || !file
		  || !h_file
		  /* || au_test_wkq(current) */);

	err = -ENOMEM;
	if (hd->hd_intent_list) {
		while (1) {
			hf = au_find_h_intent(hd, file);
			if (!hf)
				break;
#if 0 //def CONFIG_AUFS_DEBUG
			au_debug_on();
			DbgDentry(dentry);
			DbgFile(hf);
			au_debug_off();
#endif
			fput(hf);
			AuWarn("freed hfile %.*s b%d left\n",
			       AuDLNPair(dentry), bindex);
		}
	} else {
		spin_lock(&hd->hd_lock);
		if (!hd->hd_intent_list) {
			hd->hd_intent_list
				= kmalloc(sizeof(*hd->hd_intent_list),
					  GFP_ATOMIC);
			if (unlikely(!hd->hd_intent_list)) {
				spin_unlock(&hd->hd_lock);
				goto out;
			}
			INIT_LIST_HEAD(hd->hd_intent_list);
		}
		spin_unlock(&hd->hd_lock);
	}

	hdi = kmalloc(sizeof(*hdi), GFP_TEMPORARY);
	if (unlikely(!hdi))
		goto out;

	err = 0;
	//hdi->hdi_pid = current->pid;
	hdi->hdi_file[AuIntent_AUFS] = file;
	hdi->hdi_file[AuIntent_BRANCH] = h_file;
	spin_lock(&hd->hd_lock);
	list_add(&hdi->hdi_list, hd->hd_intent_list);
	spin_unlock(&hd->hd_lock);
	//AuDbgDentry(dentry);
	//AuDbgFile(h_file);

 out:
	AuTraceErr(err);
	return err;
}
#endif /* !defined(AuNoNfsBranch) */

/* ---------------------------------------------------------------------- */

void au_update_digen(struct dentry *dentry)
{
	//DiMustWriteLock(dentry);
	AuDebugOn(!dentry->d_sb);
	atomic_set(&dtodi(dentry)->di_generation, au_sigen(dentry->d_sb));
	//smp_mb(); /* atomic_set */
}

void au_update_dbrange(struct dentry *dentry, int do_put_zero)
{
	struct aufs_dinfo *dinfo;
	aufs_bindex_t bindex;
	struct dentry *h_d;

	LKTRTrace("%.*s, %d\n", AuDLNPair(dentry), do_put_zero);
	DiMustWriteLock(dentry);

	dinfo = dtodi(dentry);
	if (unlikely(!dinfo) || dinfo->di_bstart < 0)
		return;

	if (do_put_zero) {
		for (bindex = dinfo->di_bstart; bindex <= dinfo->di_bend;
		     bindex++) {
			h_d = dinfo->di_hdentry[0 + bindex].hd_dentry;
			if (h_d && !h_d->d_inode)
				set_h_dptr(dentry, bindex, NULL);
		}
	}

	dinfo->di_bstart = -1;
	while (++dinfo->di_bstart <= dinfo->di_bend)
		if (dinfo->di_hdentry[0 + dinfo->di_bstart].hd_dentry)
			break;
	if (dinfo->di_bstart > dinfo->di_bend) {
		dinfo->di_bstart = -1;
		dinfo->di_bend = -1;
		return;
	}

	dinfo->di_bend++;
	while (0 <= --dinfo->di_bend)
		if (dinfo->di_hdentry[0 + dinfo->di_bend].hd_dentry)
			break;
	AuDebugOn(dinfo->di_bstart > dinfo->di_bend || dinfo->di_bend < 0);
}

void au_update_dbstart(struct dentry *dentry)
{
	aufs_bindex_t bindex, bstart = dbstart(dentry), bend = dbend(dentry);
	struct dentry *hidden_dentry;

	LKTRTrace("%.*s\n", AuDLNPair(dentry));
	DiMustWriteLock(dentry);

	for (bindex = bstart; bindex <= bend; bindex++) {
		hidden_dentry = au_h_dptr_i(dentry, bindex);
		if (!hidden_dentry)
			continue;
		if (hidden_dentry->d_inode) {
			set_dbstart(dentry, bindex);
			return;
		}
		set_h_dptr(dentry, bindex, NULL);
	}
	//set_dbstart(dentry, -1);
	//set_dbend(dentry, -1);
}

void au_update_dbend(struct dentry *dentry)
{
	aufs_bindex_t bindex, bstart = dbstart(dentry), bend = dbend(dentry);
	struct dentry *h_dentry;

	DiMustWriteLock(dentry);
	for (bindex = bend; bindex <= bstart; bindex--) {
		h_dentry = au_h_dptr_i(dentry, bindex);
		if (!h_dentry)
			continue;
		if (h_dentry->d_inode) {
			set_dbend(dentry, bindex);
			return;
		}
		set_h_dptr(dentry, bindex, NULL);
	}
	//set_dbstart(dentry, -1);
	//set_dbend(dentry, -1);
}

int au_find_dbindex(struct dentry *dentry, struct dentry *hidden_dentry)
{
	aufs_bindex_t bindex, bend;

	bend = dbend(dentry);
	for (bindex = dbstart(dentry); bindex <= bend; bindex++)
		if (au_h_dptr_i(dentry, bindex) == hidden_dentry)
			return bindex;
	return -1;
}

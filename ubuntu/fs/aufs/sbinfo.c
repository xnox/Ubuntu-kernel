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

/* $Id: sbinfo.c,v 1.44 2007/11/12 01:43:10 sfjro Exp $ */

#include "aufs.h"

struct aufs_sbinfo *stosi(struct super_block *sb)
{
	struct aufs_sbinfo *sbinfo;
	sbinfo = sb->s_fs_info;
	//AuDebugOn(sbinfo->si_bend < 0);
	return sbinfo;
}

aufs_bindex_t sbend(struct super_block *sb)
{
	SiMustAnyLock(sb);
	return stosi(sb)->si_bend;
}

struct aufs_branch *stobr(struct super_block *sb, aufs_bindex_t bindex)
{
	struct aufs_branch *br;

	SiMustAnyLock(sb);
	AuDebugOn(bindex < 0 || sbend(sb) < bindex);
	br = stosi(sb)->si_branch[0 + bindex];
	AuDebugOn(!br);
	return br;
}

au_gen_t au_sigen(struct super_block *sb)
{
	SiMustAnyLock(sb);
	return stosi(sb)->si_generation;
}

au_gen_t au_sigen_inc(struct super_block *sb)
{
	au_gen_t gen;

	SiMustWriteLock(sb);
	gen = ++stosi(sb)->si_generation;
	au_update_digen(sb->s_root);
	au_update_iigen(sb->s_root->d_inode);
	sb->s_root->d_inode->i_version++;
	return gen;
}

int find_bindex(struct super_block *sb, struct aufs_branch *br)
{
	aufs_bindex_t bindex, bend;

	bend = sbend(sb);
	for (bindex = 0; bindex <= bend; bindex++)
		if (stobr(sb, bindex) == br)
			return bindex;
	return -1;
}

/* ---------------------------------------------------------------------- */

/* dentry and super_block lock. call at entry point */
void aufs_read_lock(struct dentry *dentry, int flags)
{
	si_read_lock(dentry->d_sb, flags);
	if (flags & AuLock_DW)
		di_write_lock_child(dentry);
	else
		di_read_lock_child(dentry, flags);
}

void aufs_read_unlock(struct dentry *dentry, int flags)
{
	if (flags & AuLock_DW)
		di_write_unlock(dentry);
	else
		di_read_unlock(dentry, flags);
	si_read_unlock(dentry->d_sb);
}

void aufs_write_lock(struct dentry *dentry)
{
	si_write_lock(dentry->d_sb);
	di_write_lock_child(dentry);
}

void aufs_write_unlock(struct dentry *dentry)
{
	di_write_unlock(dentry);
	si_write_unlock(dentry->d_sb);
}

void aufs_read_and_write_lock2(struct dentry *d1, struct dentry *d2, int flags)
{
	AuDebugOn(d1 == d2 || d1->d_sb != d2->d_sb);
	si_read_lock(d1->d_sb, flags);
	di_write_lock2_child(d1, d2, flags);
}

void aufs_read_and_write_unlock2(struct dentry *d1, struct dentry *d2)
{
	AuDebugOn(d1 == d2 || d1->d_sb != d2->d_sb);
	di_write_unlock2(d1, d2);
	si_read_unlock(d1->d_sb);
}

/* ---------------------------------------------------------------------- */

aufs_bindex_t new_br_id(struct super_block *sb)
{
	aufs_bindex_t br_id;
	struct aufs_sbinfo *sbinfo;

	AuTraceEnter();
	SiMustWriteLock(sb);

	sbinfo = stosi(sb);
	while (1) {
		br_id = ++sbinfo->si_last_br_id;
		if (br_id && find_brindex(sb, br_id) < 0)
			return br_id;
	}
}

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_SYSAUFS
static int make_xino(struct seq_file *seq, struct sysaufs_args *args,
		     int *do_size)
{
	int err, dlgt;
	struct super_block *sb = args->sb;
	aufs_bindex_t bindex, bend;
	struct file *xf;
	struct kstat st;
	struct aufs_sbinfo *sbinfo;

	AuTraceEnter();
	AuDebugOn(args->index != SysaufsSb_XINO);
	SiMustReadLock(sb);

	err = 0;
	*do_size = 0;
	sbinfo = stosi(sb);
	if (unlikely(AuFlag(sbinfo, f_xino) == AuXino_NONE)) {
#ifdef CONFIG_AUFS_DEBUG
		AuDebugOn(sbinfo->si_xib);
		bend = sbend(sb);
		for (bindex = 0; !err && bindex <= bend; bindex++)
			AuDebugOn(stobr(sb, bindex)->br_xino);
#endif
		return err;
	}

	dlgt = au_need_dlgt(sb);
	xf = sbinfo->si_xib;
	err = vfsub_getattr(xf->f_vfsmnt, xf->f_dentry, &st, dlgt);
	if (!err)
		err = seq_printf(seq, "%Lux%lu %Ld\n",
				 st.blocks, st.blksize, (long long)st.size);
	else
		err = seq_printf(seq, "err %d\n", err);

	bend = sbend(sb);
	for (bindex = 0; !err && bindex <= bend; bindex++) {
		xf = stobr(sb, bindex)->br_xino;
		if (!xf)
			continue;
		err = seq_printf(seq, "%d: ", bindex);
		if (!err)
			err = vfsub_getattr(xf->f_vfsmnt, xf->f_dentry, &st,
					    dlgt);
		if (!err)
			err = seq_printf(seq, "%d, %Lux%lu %Ld\n",
					 file_count(xf), st.blocks, st.blksize,
					 (long long)st.size);
		else
			err = seq_printf(seq, "err %d\n", err);
	}
	AuTraceErr(err);
	return err;
}

sysaufs_op au_si_ops[] = {
	[SysaufsSb_XINO] = make_xino
};
#endif /* CONFIG_AUFS_SYSAUFS */

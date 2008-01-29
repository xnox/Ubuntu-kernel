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

/* $Id: opts.h,v 1.25 2007/12/10 01:19:34 sfjro Exp $ */

#ifndef __AUFS_OPTS_H__
#define __AUFS_OPTS_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/version.h>
#include <linux/aufs_type.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
typedef const char *au_parser_pattern_t;
#else
typedef char *au_parser_pattern_t;
#endif

/* ---------------------------------------------------------------------- */
/* mount flags */

/* external inode number bitmap and translation table */
enum {
	AuXino_NONE,
	AuXino_XINO,
	AuXino_XIB
};

/* users direct branch access */
enum {
	AuUdba_NONE,
	AuUdba_REVAL,
	AuUdba_INOTIFY
};

/* copyup on open */
enum {
	AuCoo_NONE,
	AuCoo_LEAF,
	AuCoo_ALL
};

/* policies to select one among multiple writable branches */
enum {
	AuWbrCreate_TDP,	/* top down parent */
	AuWbrCreate_RR,		/* round robin */
	AuWbrCreate_MFS,	/* most free space */
	AuWbrCreate_MFSV,	/* mfs with seconds */
	AuWbrCreate_MFSRR,	/* mfs then rr */
	AuWbrCreate_MFSRRV,	/* mfs then rr with seconds */
	AuWbrCreate_PMFS,	/* parent and mfs */
	AuWbrCreate_PMFSV,	/* parent and mfs with seconds */
	AuWbrCreate_Last
};

enum {
	AuWbrCopyup_TDP,	/* top down parent */
	AuWbrCopyup_BUP,	/* bottom up parent */
	AuWbrCopyup_BU		/* bottom up */
};

/* revert it to bit-shift? */
struct au_opts_flags {
	unsigned int	f_xino:2;
	unsigned int	f_trunc_xino:1;
	unsigned int	f_udba:2;
	unsigned int	f_dlgt:1;
	unsigned int	f_plink:1;
	unsigned int	f_warn_perm:1;
	unsigned int	f_coo:2;
	unsigned int	f_always_diropq:1;
	unsigned int	f_refrof:1;
	unsigned int	f_verbose:1;
	unsigned int	f_wbr_copyup:2;
	unsigned int	f_wbr_create:3;
};

/* ---------------------------------------------------------------------- */

struct au_opt_add {
	aufs_bindex_t		bindex;
	char			*path;
	int			perm;
	struct nameidata	nd;
};

struct au_opt_del {
	char		*path;
	struct dentry	*h_root;
};

struct au_opt_mod {
	char		*path;
	int		perm;
	struct dentry	*h_root;
};

struct au_opt_xino {
	char		*path;
	struct file	*file;
};

struct au_opt_xino_itrunc {
	aufs_bindex_t	bindex;
};

struct au_opt_xino_trunc_v {
	u64		upper;
	int		step;
};

struct au_opt_wbr_create {
	int wbr_create;
	int mfs_second;
	u64 mfsrr_watermark;
};

struct au_opt {
	int type;
	union {
		struct au_opt_xino	xino;
		struct au_opt_xino_itrunc xino_itrunc;
		struct au_opt_add	add;
		struct au_opt_del	del;
		struct au_opt_mod	mod;
		int			dirwh;
		int			rdcache;
		int			deblk;
		int			nhash;
		int			udba;
		int			coo;
		struct au_opt_wbr_create wbr_create;
		int			wbr_copyup;
	};
};

struct au_opts {
	struct au_opt	*opt;
	int		max_opt;

	struct au_opts_flags given;
	struct {
		unsigned int remount:1;

		unsigned int refresh_dir:1;
		unsigned int refresh_nondir:1;
		unsigned int trunc_xib:1;
	};
};

/* ---------------------------------------------------------------------- */

void au_opts_flags_def(struct au_opts_flags *flags);
au_parser_pattern_t au_optstr_br_perm(int brperm);
au_parser_pattern_t au_optstr_udba(int udba);
au_parser_pattern_t au_optstr_coo(int coo);
au_parser_pattern_t au_optstr_wbr_copyup(int wbr_copyup);
au_parser_pattern_t au_optstr_wbr_create(int wbr_create);

void au_opts_free(struct au_opts *opts);
int au_opts_parse(struct super_block *sb, char *str, struct au_opts *opts);
int au_opts_mount(struct super_block *sb, struct au_opts *opts);
int au_opts_remount(struct super_block *sb, struct au_opts *opts);

#endif /* __KERNEL__ */
#endif /* __AUFS_OPTS_H__ */

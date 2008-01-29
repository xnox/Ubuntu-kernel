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

/* $Id: opts.c,v 1.55 2007/12/10 01:19:25 sfjro Exp $ */

#include <linux/types.h> /* a distribution requires */
#include <linux/parser.h>
#include "aufs.h"

void au_opts_flags_def(struct au_opts_flags *flags)
{
	// self-check at build time.
	flags->f_wbr_create = AuWbrCreate_Last - 1;

	flags->f_xino = AuXino_XINO;
	flags->f_trunc_xino = 0;
	flags->f_udba = AuUdba_REVAL;
	flags->f_dlgt = 0;
	flags->f_plink = 1;
	flags->f_warn_perm = 1;
	flags->f_coo = AuCoo_NONE;
	flags->f_always_diropq = 0;
	flags->f_refrof = 0;
	flags->f_verbose = 0;
	flags->f_wbr_copyup = AuWbrCopyup_TDP;
	flags->f_wbr_create = AuWbrCreate_TDP;

#ifdef CONFIG_AUFS_COMPAT
	flags->f_always_diropq = 1;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
	flags->f_plink = 0;
#endif
}

/* ---------------------------------------------------------------------- */

enum {
	Opt_br,
	Opt_add, Opt_del, Opt_mod, Opt_reorder, Opt_append, Opt_prepend,
	Opt_idel, Opt_imod, Opt_ireorder,
	Opt_dirwh, Opt_rdcache, Opt_deblk, Opt_nhash, Opt_rendir,
	Opt_xino, Opt_zxino, Opt_noxino,
	Opt_trunc_xino, Opt_trunc_xino_v, Opt_notrunc_xino,
	Opt_trunc_xino_path, Opt_itrunc_xino,
	Opt_xinodir, Opt_xinonames, Opt_ixinonames,
	Opt_trunc_xib, Opt_notrunc_xib,
	Opt_plink, Opt_noplink, Opt_list_plink, Opt_clean_plink,
	Opt_udba,
	//Opt_lock, Opt_unlock,
	Opt_cmd, Opt_cmd_args,
	Opt_diropq_a, Opt_diropq_w,
	Opt_warn_perm, Opt_nowarn_perm,
	Opt_wbr_copyup, Opt_wbr_create,
	Opt_coo,
	Opt_dlgt, Opt_nodlgt,
	Opt_refrof, Opt_norefrof,
	Opt_verbose, Opt_noverbose,
	Opt_tail, Opt_ignore, Opt_err
};

static match_table_t options = {
	{Opt_br, "br=%s"},
	{Opt_br, "br:%s"},

	{Opt_add, "add=%d:%s"},
	{Opt_add, "add:%d:%s"},
	{Opt_add, "ins=%d:%s"},
	{Opt_add, "ins:%d:%s"},
	{Opt_append, "append=%s"},
	{Opt_append, "append:%s"},
	{Opt_prepend, "prepend=%s"},
	{Opt_prepend, "prepend:%s"},

	{Opt_del, "del=%s"},
	{Opt_del, "del:%s"},
	//{Opt_idel, "idel:%d"},
	{Opt_mod, "mod=%s"},
	{Opt_mod, "mod:%s"},
	{Opt_imod, "imod:%d:%s"},

	{Opt_dirwh, "dirwh=%d"},
	{Opt_dirwh, "dirwh:%d"},

	{Opt_xino, "xino=%s"},
	{Opt_xino, "xino:%s"},
	{Opt_xinodir, "xinodir=%s"},
	{Opt_xinodir, "xinodir:%s"},
	{Opt_noxino, "noxino"},
	{Opt_trunc_xino, "trunc_xino"},
	{Opt_trunc_xino_v, "trunc_xino_v=%d:%d"},
	{Opt_notrunc_xino, "notrunc_xino"},
	{Opt_trunc_xino_path, "trunc_xino=%s"},
	{Opt_trunc_xino_path, "trunc_xino:%s"},
	{Opt_itrunc_xino, "itrunc_xino=%d"},
	{Opt_itrunc_xino, "itrunc_xino:%d"},
	//{Opt_zxino, "zxino=%s"},
	{Opt_trunc_xib, "trunc_xib"},
	{Opt_notrunc_xib, "notrunc_xib"},

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 16)
	{Opt_plink, "plink"},
	{Opt_noplink, "noplink"},
#ifdef CONFIG_AUFS_DEBUG
	{Opt_list_plink, "list_plink"},
#endif
	{Opt_clean_plink, "clean_plink"},
#endif

	{Opt_udba, "udba=%s"},

	{Opt_diropq_a, "diropq=always"},
	{Opt_diropq_a, "diropq=a"},
	{Opt_diropq_w, "diropq=whiteouted"},
	{Opt_diropq_w, "diropq=w"},

	{Opt_warn_perm, "warn_perm"},
	{Opt_nowarn_perm, "nowarn_perm"},

#ifdef CONFIG_AUFS_DLGT
	{Opt_dlgt, "dlgt"},
	{Opt_nodlgt, "nodlgt"},
#endif

	{Opt_rendir, "rendir=%d"},
	{Opt_rendir, "rendir:%d"},

	{Opt_refrof, "refrof"},
	{Opt_norefrof, "norefrof"},

	{Opt_verbose, "verbose"},
	{Opt_verbose, "v"},
	{Opt_noverbose, "noverbose"},
	{Opt_noverbose, "quiet"},
	{Opt_noverbose, "q"},
	{Opt_noverbose, "silent"},

	{Opt_rdcache, "rdcache=%d"},
	{Opt_rdcache, "rdcache:%d"},

	{Opt_coo, "coo=%s"},

	{Opt_wbr_create, "create=%s"},
	{Opt_wbr_create, "create:%s"},
	{Opt_wbr_create, "create_policy=%s"},
	{Opt_wbr_create, "create_policy:%s"},
	{Opt_wbr_copyup, "cpup=%s"},
	{Opt_wbr_copyup, "cpup:%s"},
	{Opt_wbr_copyup, "copyup=%s"},
	{Opt_wbr_copyup, "copyup:%s"},
	{Opt_wbr_copyup, "copyup_policy=%s"},
	{Opt_wbr_copyup, "copyup_policy:%s"},

#if 0 // rfu
	{Opt_deblk, "deblk=%d"},
	{Opt_deblk, "deblk:%d"},
	{Opt_nhash, "nhash=%d"},
	{Opt_nhash, "nhash:%d"},
#endif

	{Opt_br, "dirs=%s"},
	{Opt_ignore, "debug=%d"},
	{Opt_ignore, "delete=whiteout"},
	{Opt_ignore, "delete=all"},
	{Opt_ignore, "imap=%s"},

	{Opt_err, NULL}
};

/* ---------------------------------------------------------------------- */

static au_parser_pattern_t au_parser_pattern(int val, struct match_token *token)
{
	while (token->pattern) {
		if (token->token == val)
			return token->pattern;
		token++;
	}
	BUG();
	return "??";
}

/* ---------------------------------------------------------------------- */

#define RW		"rw"
#define RO		"ro"
#define WH		"wh"
#define RR		"rr"
#define NoLinkWH	"nolwh"

static match_table_t brperms = {
	{AuBr_RR, RR},
	{AuBr_RO, RO},
	{AuBr_RW, RW},

	{AuBr_RRWH, RR "+" WH},
	{AuBr_ROWH, RO "+" WH},
	{AuBr_RWNoLinkWH, RW "+" NoLinkWH},

	{AuBr_ROWH, "nfsro"},
	{AuBr_RO, NULL}
};

static int br_perm_val(char *perm)
{
	int val;
	substring_t args[MAX_OPT_ARGS];

	AuDebugOn(!perm || !*perm);
	LKTRTrace("perm %s\n", perm);
	val = match_token(perm, brperms, args);
	AuTraceErr(val);
	return val;
}

au_parser_pattern_t au_optstr_br_perm(int brperm)
{
	return au_parser_pattern(brperm, brperms);
}

/* ---------------------------------------------------------------------- */

static match_table_t udbalevel = {
	{AuUdba_REVAL, "reval"},
#ifdef CONFIG_AUFS_HINOTIFY
	{AuUdba_INOTIFY, "inotify"},
#endif
	{AuUdba_NONE, "none"},
	{-1, NULL}
};

static int udba_val(char *str)
{
	substring_t args[MAX_OPT_ARGS];
	return match_token(str, udbalevel, args);
}

au_parser_pattern_t au_optstr_udba(int udba)
{
	return au_parser_pattern(udba, udbalevel);
}

/* ---------------------------------------------------------------------- */

static match_table_t coolevel = {
	{AuCoo_LEAF, "leaf"},
	{AuCoo_ALL, "all"},
	{AuCoo_NONE, "none"},
	{-1, NULL}
};

static int coo_val(char *str)
{
	substring_t args[MAX_OPT_ARGS];
	return match_token(str, coolevel, args);
}

au_parser_pattern_t au_optstr_coo(int coo)
{
	return au_parser_pattern(coo, coolevel);
}

/* ---------------------------------------------------------------------- */

static match_table_t au_wbr_create_policy = {
	{AuWbrCreate_TDP, "tdp"},
	{AuWbrCreate_TDP, "top-down-parent"},
	{AuWbrCreate_RR, "rr"},
	{AuWbrCreate_RR, "round-robin"},
	{AuWbrCreate_MFS, "mfs"},
	{AuWbrCreate_MFS, "most-free-space"},
	{AuWbrCreate_MFSV, "mfs:%d"},
	{AuWbrCreate_MFSV, "most-free-space:%d"},

	{AuWbrCreate_MFSRR, "mfsrr:%d"},
	{AuWbrCreate_MFSRRV, "mfsrr:%d:%d"},
	{AuWbrCreate_PMFS, "pmfs"},
	{AuWbrCreate_PMFSV, "pmfs:%d"},

	{-1, NULL}
};

/* cf. linux/lib/parser.c */
static int au_match_ull(substring_t *s, unsigned long long *result, int base)
{
	char *endp;
	char *buf;
	int ret;

	buf = kmalloc(s->to - s->from + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	memcpy(buf, s->from, s->to - s->from);
	buf[s->to - s->from] = '\0';
	*result = simple_strtoull(buf, &endp, base);
	ret = 0;
	if (endp == buf)
		ret = -EINVAL;
	kfree(buf);
	return ret;
}

static int au_wbr_mfs_wmark(substring_t *arg, char *str,
			    struct au_opt_wbr_create *create)
{
	int err;
	u64 ull;

	err = 0;
	if (!au_match_ull(arg, &ull, 0))
		create->mfsrr_watermark = ull;
	else {
		AuErr("bad integer in %s\n", str);
		err = -EINVAL;
	}

	AuTraceErr(err);
	return err;
}

static int au_wbr_mfs_sec(substring_t *arg, char *str,
			  struct au_opt_wbr_create *create)
{
	int n, err;

	err = 0;
	if (!match_int(arg, &n) && 0 <= n)
		create->mfs_second = n;
	else {
		AuErr("bad integer in %s\n", str);
		err = -EINVAL;
	}

	AuTraceErr(err);
	return err;
}

static int au_wbr_create_val(char *str, struct au_opt_wbr_create *create)
{
	int err, e;
	substring_t args[MAX_OPT_ARGS];

	err = match_token(str, au_wbr_create_policy, args);
	create->wbr_create = err;
	switch (err) {
	case AuWbrCreate_MFSRRV:
		e = au_wbr_mfs_wmark(&args[0], str, create);
		if (!e)
			e = au_wbr_mfs_sec(&args[1], str, create);
		if (unlikely(e))
			err = e;
		break;
	case AuWbrCreate_MFSRR:
		e = au_wbr_mfs_wmark(&args[0], str, create);
		if (unlikely(e)) {
			err = e;
			break;
		}
		/*FALLTHROUGH*/
	case AuWbrCreate_MFS:
	case AuWbrCreate_PMFS:
		create->mfs_second = AUFS_MFS_SECOND_DEF;
		break;
	case AuWbrCreate_MFSV:
	case AuWbrCreate_PMFSV:
		e = au_wbr_mfs_sec(&args[0], str, create);
		if (unlikely(e))
			err = e;
		break;
	}

	return err;
}

au_parser_pattern_t au_optstr_wbr_create(int wbr_create)
{
	return au_parser_pattern(wbr_create, au_wbr_create_policy);
}

static match_table_t au_wbr_copyup_policy = {
	{AuWbrCopyup_TDP, "tdp"},
	{AuWbrCopyup_TDP, "top-down-parent"},
	{AuWbrCopyup_BUP, "bup"},
	{AuWbrCopyup_BUP, "bottom-up-parent"},
	{AuWbrCopyup_BU, "bu"},
	{AuWbrCopyup_BU, "bottom-up"},
	{-1, NULL}
};

static int au_wbr_copyup_val(char *str)
{
	substring_t args[MAX_OPT_ARGS];
	return match_token(str, au_wbr_copyup_policy, args);
}

au_parser_pattern_t au_optstr_wbr_copyup(int wbr_copyup)
{
	return au_parser_pattern(wbr_copyup, au_wbr_copyup_policy);
}

/* ---------------------------------------------------------------------- */

static const int lkup_dirflags = LOOKUP_FOLLOW | LOOKUP_DIRECTORY;

static void dump_opts(struct au_opts *opts)
{
#ifdef CONFIG_AUFS_DEBUG
	/* reduce stack space */
	union {
		struct au_opt_add *add;
		struct au_opt_del *del;
		struct au_opt_mod *mod;
		struct au_opt_xino *xino;
		struct au_opt_xino_itrunc *xino_itrunc;
		struct au_opt_wbr_create *create;
	} u;
	struct au_opt *opt;

	AuTraceEnter();

	opt = opts->opt;
	while (/* opt < opts_tail && */ opt->type != Opt_tail) {
		switch (opt->type) {
		case Opt_add:
			u.add = &opt->add;
			LKTRTrace("add {b%d, %s, 0x%x, %p}\n",
				  u.add->bindex, u.add->path, u.add->perm,
				  u.add->nd.dentry);
			break;
		case Opt_del:
		case Opt_idel:
			u.del = &opt->del;
			LKTRTrace("del {%s, %p}\n", u.del->path, u.del->h_root);
			break;
		case Opt_mod:
		case Opt_imod:
			u.mod = &opt->mod;
			LKTRTrace("mod {%s, 0x%x, %p}\n",
				  u.mod->path, u.mod->perm, u.mod->h_root);
			break;
		case Opt_append:
			u.add = &opt->add;
			LKTRTrace("append {b%d, %s, 0x%x, %p}\n",
				  u.add->bindex, u.add->path, u.add->perm,
				  u.add->nd.dentry);
			break;
		case Opt_prepend:
			u.add = &opt->add;
			LKTRTrace("prepend {b%d, %s, 0x%x, %p}\n",
				  u.add->bindex, u.add->path, u.add->perm,
				  u.add->nd.dentry);
			break;
		case Opt_dirwh:
			LKTRTrace("dirwh %d\n", opt->dirwh);
			break;
		case Opt_rdcache:
			LKTRTrace("rdcache %d\n", opt->rdcache);
			break;
		case Opt_xino:
			u.xino = &opt->xino;
			LKTRTrace("xino {%s %.*s}\n",
				  u.xino->path,
				  AuDLNPair(u.xino->file->f_dentry));
			break;
		case Opt_trunc_xino:
			LKTRLabel(trunc_xino);
			break;
		case Opt_notrunc_xino:
			LKTRLabel(notrunc_xino);
			break;
		case Opt_trunc_xino_path:
		case Opt_itrunc_xino:
			u.xino_itrunc = &opt->xino_itrunc;
			LKTRTrace("trunc_xino %d\n", u.xino_itrunc->bindex);
			break;

		case Opt_noxino:
			LKTRLabel(noxino);
			break;
		case Opt_trunc_xib:
			LKTRLabel(trunc_xib);
			break;
		case Opt_notrunc_xib:
			LKTRLabel(notrunc_xib);
			break;
		case Opt_plink:
			LKTRLabel(plink);
			break;
		case Opt_noplink:
			LKTRLabel(noplink);
			break;
		case Opt_list_plink:
			LKTRLabel(list_plink);
			break;
		case Opt_clean_plink:
			LKTRLabel(clean_plink);
			break;
		case Opt_udba:
			LKTRTrace("udba %d, %s\n",
				  opt->udba, au_optstr_udba(opt->udba));
			break;
		case Opt_diropq_a:
			LKTRLabel(diropq_a);
			break;
		case Opt_diropq_w:
			LKTRLabel(diropq_w);
			break;
		case Opt_warn_perm:
			LKTRLabel(warn_perm);
			break;
		case Opt_nowarn_perm:
			LKTRLabel(nowarn_perm);
			break;
		case Opt_dlgt:
			LKTRLabel(dlgt);
			break;
		case Opt_nodlgt:
			LKTRLabel(nodlgt);
			break;
		case Opt_refrof:
			LKTRLabel(refrof);
			break;
		case Opt_norefrof:
			LKTRLabel(norefrof);
			break;
		case Opt_verbose:
			LKTRLabel(verbose);
			break;
		case Opt_noverbose:
			LKTRLabel(noverbose);
			break;
		case Opt_coo:
			LKTRTrace("coo %d, %s\n",
				  opt->coo, au_optstr_coo(opt->coo));
			break;
		case Opt_wbr_create:
			u.create = &opt->wbr_create;
			LKTRTrace("create %d, %s\n", u.create->wbr_create,
				  au_optstr_wbr_create(u.create->wbr_create));
			switch (u.create->wbr_create) {
			case AuWbrCreate_MFSV:
			case AuWbrCreate_PMFSV:
				LKTRTrace("%d sec\n", u.create->mfs_second);
				break;
			case AuWbrCreate_MFSRR:
				LKTRTrace("%Lu watermark\n",
					  u.create->mfsrr_watermark);
				break;
			case AuWbrCreate_MFSRRV:
				LKTRTrace("%Lu watermark, %d sec\n",
					  u.create->mfsrr_watermark,
					  u.create->mfs_second);
				break;
			}
			break;
		case Opt_wbr_copyup:
			LKTRTrace("copyup %d, %s\n", opt->wbr_copyup,
				  au_optstr_wbr_copyup(opt->wbr_copyup));
			break;
		default:
			BUG();
		}
		opt++;
	}
#endif
}

void au_opts_free(struct au_opts *opts)
{
	struct au_opt *opt;

	AuTraceEnter();

	opt = opts->opt;
	while (opt->type != Opt_tail) {
		switch (opt->type) {
		case Opt_add:
		case Opt_append:
		case Opt_prepend:
			path_release(&opt->add.nd);
			break;
		case Opt_del:
		case Opt_idel:
			dput(opt->del.h_root);
			break;
		case Opt_mod:
		case Opt_imod:
			dput(opt->mod.h_root);
			break;
		case Opt_xino:
			fput(opt->xino.file);
			break;
		}
		opt++;
	}
}

static int opt_add(struct au_opt *opt, char *opt_str, struct super_block *sb,
		   aufs_bindex_t bindex)
{
	int err;
	struct au_opt_add *add = &opt->add;
	char *p;

	LKTRTrace("%s, b%d\n", opt_str, bindex);

	add->bindex = bindex;
	add->perm = AuBr_Last;
	add->path = opt_str;
	p = strchr(opt_str, '=');
	if (unlikely(p)) {
		*p++ = 0;
		if (*p)
			add->perm = br_perm_val(p);
	}

	/* LSM may detect it */
	/* do not superio. */
	err = vfsub_path_lookup(add->path, lkup_dirflags, &add->nd);
	//err = -1;
	if (!err) {
		if (!p /* && add->perm == AuBr_Last */) {
			add->perm = AuBr_RO;
			if (au_test_def_rr(add->nd.dentry->d_sb))
				add->perm = AuBr_RR;
			if (!bindex && !(sb->s_flags & MS_RDONLY))
				add->perm = AuBr_RW;
#ifdef CONFIG_AUFS_COMPAT
			add->perm = AuBr_RW;
#endif
		}
		opt->type = Opt_add;
		goto out;
	}
	AuErr("lookup failed %s (%d)\n", add->path, err);
	err = -EINVAL;

 out:
	AuTraceErr(err);
	return err;
}

/* called without aufs lock */
int au_opts_parse(struct super_block *sb, char *str, struct au_opts *opts)
{
	int err, n, token, skipped;
	struct dentry *root;
	struct au_opt *opt, *opt_tail;
	char *opt_str, *p;
	substring_t args[MAX_OPT_ARGS];
	aufs_bindex_t bindex, bend;
	struct nameidata nd;
	union {
		struct au_opt_del *del;
		struct au_opt_mod *mod;
		struct au_opt_xino *xino;
		struct au_opt_xino_itrunc *xino_itrunc;
		struct au_opt_wbr_create *create;
	} u;
	struct file *file;

	LKTRTrace("%s, nopts %d\n", str, opts->max_opt);

	root = sb->s_root;
	err = 0;
	bindex = 0;
	opt = opts->opt;
	opt_tail = opt + opts->max_opt - 1;
	opt->type = Opt_tail;
	while (!err && (opt_str = strsep(&str, ",")) && *opt_str) {
		err = -EINVAL;
		token = match_token(opt_str, options, args);
		LKTRTrace("%s, token %d, args[0]{%p, %p}\n",
			  opt_str, token, args[0].from, args[0].to);

		skipped = 0;
		switch (token) {
		case Opt_br:
			err = 0;
			while (!err && (opt_str = strsep(&args[0].from, ":"))
			       && *opt_str) {
				err = opt_add(opt, opt_str, sb, bindex++);
				//if (LktrCond) err = -1;
				if (unlikely(!err && ++opt > opt_tail)) {
					err = -E2BIG;
					break;
				}
				opt->type = Opt_tail;
				skipped = 1;
			}
			break;
		case Opt_add:
			if (unlikely(match_int(&args[0], &n))) {
				AuErr("bad integer in %s\n", opt_str);
				break;
			}
			bindex = n;
			err = opt_add(opt, args[1].from, sb, bindex);
			break;
		case Opt_append:
			err = opt_add(opt, args[0].from, sb, /*dummy bindex*/1);
			if (!err)
				opt->type = token;
			break;
		case Opt_prepend:
			err = opt_add(opt, args[0].from, sb, /*bindex*/0);
			if (!err)
				opt->type = token;
			break;
		case Opt_del:
			u.del = &opt->del;
			u.del->path = args[0].from;
			LKTRTrace("del path %s\n", u.del->path);
			/* LSM may detect it */
			/* do not superio. */
			err = vfsub_path_lookup(u.del->path, lkup_dirflags,
						&nd);
			if (unlikely(err)) {
				AuErr("lookup failed %s (%d)\n",
				    u.del->path, err);
				break;
			}
			u.del->h_root = dget(nd.dentry);
			path_release(&nd);
			opt->type = token;
			break;
#if 0 // rfu
		case Opt_idel:
			u.del = &opt->del;
			u.del->path = "(indexed)";
			if (unlikely(match_int(&args[0], &n))) {
				AuErr("bad integer in %s\n", opt_str);
				break;
			}
			bindex = n;
			aufs_read_lock(root, AuLock_FLUSH);
			if (bindex < 0 || sbend(sb) < bindex) {
				AuErr("out of bounds, %d\n", bindex);
				aufs_read_unlock(root, !AuLock_IR);
				break;
			}
			err = 0;
			u.del->h_root = dget(au_h_dptr_i(root, bindex));
			opt->type = token;
			aufs_read_unlock(root, !AuLock_IR);
			break;
#endif
		case Opt_mod:
			u.mod = &opt->mod;
			u.mod->path = args[0].from;
			p = strchr(u.mod->path, '=');
			if (unlikely(!p)) {
				AuErr("no permssion %s\n", opt_str);
				break;
			}
			*p++ = 0;
			u.mod->perm = br_perm_val(p);
			LKTRTrace("mod path %s, perm 0x%x, %s\n",
				  u.mod->path, u.mod->perm, p);
			/* LSM may detect it */
			/* do not superio. */
			err = vfsub_path_lookup(u.mod->path, lkup_dirflags,
						&nd);
			if (unlikely(err)) {
				AuErr("lookup failed %s (%d)\n",
				    u.mod->path, err);
				break;
			}
			u.mod->h_root = dget(nd.dentry);
			path_release(&nd);
			opt->type = token;
			break;
#ifdef IMOD
		case Opt_imod:
			u.mod = &opt->mod;
			u.mod->path = "(indexed)";
			if (unlikely(match_int(&args[0], &n))) {
				AuErr("bad integer in %s\n", opt_str);
				break;
			}
			bindex = n;
			aufs_read_lock(root, AuLock_FLUSH);
			if (bindex < 0 || sbend(sb) < bindex) {
				AuErr("out of bounds, %d\n", bindex);
				aufs_read_unlock(root, !AuLock_IR);
				break;
			}
			u.mod->perm = br_perm_val(args[1].from);
			LKTRTrace("mod path %s, perm 0x%x, %s\n",
				  u.mod->path, u.mod->perm, args[1].from);
			err = 0;
			u.mod->h_root = dget(au_h_dptr_i(root, bindex));
			opt->type = token;
			aufs_read_unlock(root, !AuLock_IR);
			break;
#endif
		case Opt_xino:
			u.xino = &opt->xino;
			file = xino_create(sb, args[0].from, /*silent*/0,
					   /*parent*/NULL);
			err = PTR_ERR(file);
			if (IS_ERR(file))
				break;
			err = -EINVAL;
			if (unlikely(file->f_dentry->d_sb == sb)) {
				fput(file);
				AuErr("%s must be outside\n", args[0].from);
				break;
			}
			err = 0;
			u.xino->file = file;
			u.xino->path = args[0].from;
			opt->type = token;
			break;

		case Opt_trunc_xino_path:
			u.xino_itrunc = &opt->xino_itrunc;
			p = args[0].from;
			LKTRTrace("trunc_xino path %s\n", p);
			/* LSM may detect it */
			/* do not superio. */
			err = vfsub_path_lookup(p, lkup_dirflags, &nd);
			if (unlikely(err)) {
				AuErr("lookup failed %s (%d)\n", p , err);
				break;
			}
			u.xino_itrunc->bindex = -1;
			aufs_read_lock(root, AuLock_FLUSH);
			bend = sbend(sb);
			for (bindex = 0; bindex <= bend; bindex++) {
				if (au_h_dptr_i(root, bindex) == nd.dentry) {
					u.xino_itrunc->bindex = bindex;
					break;
				}
			}
			aufs_read_unlock(root, !AuLock_IR);
			path_release(&nd);
			if (unlikely(u.xino_itrunc->bindex < 0)) {
				AuErr("no such branch %s\n", p);
				err = -EINVAL;
				break;
			}
			opt->type = token;
			break;

		case Opt_itrunc_xino:
			u.xino_itrunc = &opt->xino_itrunc;
			if (unlikely(match_int(&args[0], &n))) {
				AuErr("bad integer in %s\n", opt_str);
				break;
			}
			u.xino_itrunc->bindex = n;
			aufs_read_lock(root, AuLock_FLUSH);
			if (n < 0 || sbend(sb) < n) {
				AuErr("out of bounds, %d\n", n);
				aufs_read_unlock(root, !AuLock_IR);
				break;
			}
			aufs_read_unlock(root, !AuLock_IR);
			err = 0;
			opt->type = token;
			break;

		case Opt_dirwh:
			if (unlikely(match_int(&args[0], &opt->dirwh)))
				break;
			err = 0;
			opt->type = token;
			break;

		case Opt_rdcache:
			if (unlikely(match_int(&args[0], &opt->rdcache)))
				break;
			err = 0;
			opt->type = token;
			break;

		case Opt_trunc_xino:
		case Opt_notrunc_xino:
		case Opt_noxino:
		case Opt_trunc_xib:
		case Opt_notrunc_xib:
		case Opt_plink:
		case Opt_noplink:
		case Opt_list_plink:
		case Opt_clean_plink:
		case Opt_diropq_a:
		case Opt_diropq_w:
		case Opt_warn_perm:
		case Opt_nowarn_perm:
		case Opt_dlgt:
		case Opt_nodlgt:
		case Opt_refrof:
		case Opt_norefrof:
		case Opt_verbose:
		case Opt_noverbose:
			err = 0;
			opt->type = token;
			break;

		case Opt_udba:
			opt->udba = udba_val(args[0].from);
			if (opt->udba >= 0) {
				err = 0;
				opt->type = token;
			}
			break;

		case Opt_wbr_create:
			u.create = &opt->wbr_create;
			u.create->wbr_create
				= au_wbr_create_val(args[0].from, u.create);
			if (u.create->wbr_create >= 0) {
				err = 0;
				opt->type = token;
			}
			break;
		case Opt_wbr_copyup:
			opt->wbr_copyup = au_wbr_copyup_val(args[0].from);
			if (opt->wbr_copyup >= 0) {
				err = 0;
				opt->type = token;
			}
			break;

		case Opt_coo:
			opt->coo = coo_val(args[0].from);
			if (opt->coo >= 0) {
				err = 0;
				opt->type = token;
			}
			break;

		case Opt_ignore:
#ifndef CONFIG_AUFS_COMPAT
			AuWarn("ignored %s\n", opt_str);
#endif
			skipped = 1;
			err = 0;
			break;
		case Opt_err:
			AuErr("unknown option %s\n", opt_str);
			break;
		}

		if (!err && !skipped) {
			if (unlikely(++opt > opt_tail)) {
				err = -E2BIG;
				opt--;
				opt->type = Opt_tail;
				break;
			}
			opt->type = Opt_tail;
		}
	}

	dump_opts(opts);
	if (unlikely(err))
		au_opts_free(opts);
	AuTraceErr(err);
	return err;
}

/*
 * returns,
 * plus: processed without an error
 * zero: unprocessed
 */
static int au_opt_simple(struct super_block *sb, struct au_opt *opt,
			 struct au_opts *opts)
{
	int err;
	struct aufs_sbinfo *sbinfo;
	struct au_opts_flags *given;
	struct au_opt_wbr_create *create;

	AuTraceEnter();

	err = 1; /* handled */
	sbinfo = stosi(sb);
	given = &opts->given;
	switch (opt->type) {
	case Opt_udba:
		AuFlagSet(sbinfo, f_udba, opt->udba);
		given->f_udba = 1;
		break;

	case Opt_plink:
		AuFlagSet(sbinfo, f_plink, 1);
		given->f_plink = 1;
		break;
	case Opt_noplink:
		if (AuFlag(sbinfo, f_plink))
			au_put_plink(sb);
		AuFlagSet(sbinfo, f_plink, 0);
		given->f_plink = 1;
		break;
	case Opt_list_plink:
		if (AuFlag(sbinfo, f_plink))
			au_list_plink(sb);
		break;
	case Opt_clean_plink:
		if (AuFlag(sbinfo, f_plink))
			au_put_plink(sb);
		break;

	case Opt_diropq_a:
		AuFlagSet(sbinfo, f_always_diropq, 1);
		given->f_always_diropq = 1;
		break;
	case Opt_diropq_w:
		AuFlagSet(sbinfo, f_always_diropq, 0);
		given->f_always_diropq = 1;
		break;

	case Opt_dlgt:
		AuFlagSet(sbinfo, f_dlgt, 1);
		given->f_dlgt = 1;
		break;
	case Opt_nodlgt:
		AuFlagSet(sbinfo, f_dlgt, 0);
		given->f_dlgt = 1;
		break;

	case Opt_warn_perm:
		AuFlagSet(sbinfo, f_warn_perm, 1);
		given->f_warn_perm = 1;
		break;
	case Opt_nowarn_perm:
		AuFlagSet(sbinfo, f_warn_perm, 0);
		given->f_warn_perm = 1;
		break;

	case Opt_refrof:
		AuFlagSet(sbinfo, f_refrof, 1);
		given->f_refrof = 1;
		break;
	case Opt_norefrof:
		//coo_set(sb, AuFlag_COO_LEAF);
		AuFlagSet(sbinfo, f_refrof, 0);
		given->f_refrof = 1;
		break;

	case Opt_verbose:
		AuFlagSet(sbinfo, f_verbose, 1);
		given->f_verbose = 1;
		break;
	case Opt_noverbose:
		AuFlagSet(sbinfo, f_verbose, 0);
		given->f_verbose = 1;
		break;

	case Opt_wbr_create:
		create = &opt->wbr_create;
		if (sbinfo->si_wbr_create_ops->fin) {
			err = sbinfo->si_wbr_create_ops->fin(sb);
			if (!err)
				err = 1;
		}
		AuFlagSet(sbinfo, f_wbr_create, create->wbr_create);
		sbinfo->si_wbr_create_ops
			= au_wbr_create_ops + create->wbr_create;
		switch (create->wbr_create) {
		case AuWbrCreate_MFSRRV:
		case AuWbrCreate_MFSRR:
			sbinfo->si_wbr_mfs.mfsrr_watermark
				= create->mfsrr_watermark;
			/*FALLTHROUGH*/
		case AuWbrCreate_MFS:
		case AuWbrCreate_MFSV:
		case AuWbrCreate_PMFS:
		case AuWbrCreate_PMFSV:
			sbinfo->si_wbr_mfs.mfs_expire = create->mfs_second * HZ;
			break;
		}
		if (sbinfo->si_wbr_create_ops->init)
			sbinfo->si_wbr_create_ops->init(sb); /* ignore */
		given->f_wbr_create = 1;
		break;
	case Opt_wbr_copyup:
		AuFlagSet(sbinfo, f_wbr_copyup, opt->wbr_copyup);
		sbinfo->si_wbr_copyup_ops = au_wbr_copyup_ops + opt->wbr_copyup;
		given->f_wbr_copyup = 1;
		break;

	case Opt_coo:
		AuFlagSet(sbinfo, f_coo, opt->coo);
		given->f_coo = 1;
		break;

	case Opt_dirwh:
		sbinfo->si_dirwh = opt->dirwh;
		break;

	case Opt_rdcache:
		sbinfo->si_rdcache = opt->rdcache * HZ;
		break;

	case Opt_trunc_xino:
		AuFlagSet(sbinfo, f_trunc_xino, 1);
		given->f_trunc_xino = 1;
		break;
	case Opt_notrunc_xino:
		AuFlagSet(sbinfo, f_trunc_xino, 0);
		given->f_trunc_xino = 1;
		break;

	case Opt_trunc_xino_path:
	case Opt_itrunc_xino:
		err = xino_trunc(sb, opt->xino_itrunc.bindex);
		if (!err)
			err = 1;
		break;

	case Opt_trunc_xib:
		opts->trunc_xib = 1;
		break;
	case Opt_notrunc_xib:
		opts->trunc_xib = 0;
		break;

	default:
		err = 0;
		break;
	}

	AuTraceErr(err);
	return err;
}

/*
 * returns tri-state.
 * plus: processed without an error
 * zero: unprocessed
 * minus: error
 */
static int au_opt_br(struct super_block *sb, struct au_opt *opt,
		     struct au_opts *opts)
{
	int err, do_refresh;

	AuTraceEnter();

	err = 0;
	switch (opt->type) {
	case Opt_append:
		opt->add.bindex = sbend(sb) + 1;
		if (unlikely(opt->add.bindex < 0))
			opt->add.bindex = 0;
		goto add;
	case Opt_prepend:
		opt->add.bindex = 0;
	add:
	case Opt_add:
		err = br_add(sb, &opt->add, opts->remount);
		if (!err) {
			err = 1;
			opts->refresh_dir = 1;
			if (unlikely(br_whable(opt->add.perm)))
				opts->refresh_nondir = 1;
		}
		break;

	case Opt_del:
	case Opt_idel:
		err = br_del(sb, &opt->del, opts->remount);
		if (!err) {
			err = 1;
			opts->trunc_xib = 1;
			opts->refresh_dir = 1;
			opts->refresh_nondir = 1;
		}
		break;

	case Opt_mod:
	case Opt_imod:
		err = br_mod(sb, &opt->mod, opts->remount, &do_refresh);
		if (!err) {
			err = 1;
			if (unlikely(do_refresh)) {
				opts->refresh_dir = 1;
				opts->refresh_nondir = 1;
			}
		}
		break;
	}

	AuTraceErr(err);
	return err;
}

static int au_opt_xino(struct super_block *sb, struct au_opt *opt,
		       struct au_opt_xino **opt_xino, struct au_opts *opts)
{
	int err;

	AuTraceEnter();

	err = 0;
	switch (opt->type) {
	case Opt_xino:
		err = xino_set(sb, &opt->xino, opts->remount);
		if (!err)
			*opt_xino = &opt->xino;
		break;
	case Opt_noxino:
		xino_clr(sb);
		*opt_xino = (void *)-1;
		break;
	}

	AuTraceErr(err);
	return err;
}

static int verify_opts(struct super_block *sb, struct au_opts_flags *pending,
		       int remount)
{
	int err;
	aufs_bindex_t bindex, bend;
	struct aufs_branch *br;
	struct dentry *root;
	struct inode *dir;
	unsigned int do_plink;
	struct aufs_sbinfo *sbinfo;

	AuTraceEnter();

	if (unlikely(!(sb->s_flags & MS_RDONLY)
		     && !br_writable(sbr_perm(sb, 0))))
		AuWarn("first branch should be rw\n");

	sbinfo = stosi(sb);
	if (unlikely((au_flag_test_udba_inotify(sb)
		      || pending->f_udba == AuUdba_INOTIFY)
		     && AuFlag(sbinfo, f_xino) == AuXino_NONE))
		AuWarn("udba=inotify requires xino\n");

	err = 0;
	root = sb->s_root;
	dir = sb->s_root->d_inode;
	do_plink = AuFlag(sbinfo, f_plink);
	bend = sbend(sb);
	for (bindex = 0; !err && bindex <= bend; bindex++) {
		struct inode *h_dir;
		int skip;

		skip = 0;
		h_dir = au_h_iptr_i(dir, bindex);
		br = stobr(sb, bindex);
		br_wh_read_lock(br);
		switch (br->br_perm) {
		case AuBr_RR:
		case AuBr_RO:
		case AuBr_RRWH:
		case AuBr_ROWH:
			skip = (!br->br_wh && !br->br_plink);
			break;

		case AuBr_RWNoLinkWH:
			skip = !br->br_wh;
			if (skip) {
				if (do_plink)
					skip = !!br->br_plink;
				else
					skip = !br->br_plink;
			}
			break;

		case AuBr_RW:
			skip = !!br->br_wh;
			if (skip) {
				if (do_plink)
					skip = !!br->br_plink;
				else
					skip = !br->br_plink;
			}
			break;

		default:
			BUG();
		}
		br_wh_read_unlock(br);

		if (skip)
			continue;

		hdir_lock(h_dir, dir, bindex);
		br_wh_write_lock(br);
		err = init_wh(au_h_dptr_i(root, bindex), br,
			      au_nfsmnt(sb, bindex), sb);
		br_wh_write_unlock(br);
		hdir_unlock(h_dir, dir, bindex);
	}

	AuTraceErr(err);
	return err;
}

int au_opts_mount(struct super_block *sb, struct au_opts *opts)
{
	int err;
	struct inode *dir;
	struct au_opt *opt;
	struct au_opt_xino *opt_xino;
	aufs_bindex_t bend;
	struct aufs_sbinfo *sbinfo;
	struct au_opts_flags tmp;

	AuTraceEnter();
	SiMustWriteLock(sb);
	DiMustWriteLock(sb->s_root);
	dir = sb->s_root->d_inode;
	IiMustWriteLock(dir);

	err = 0;
	opt_xino = NULL;
	opt = opts->opt;
	while (err >= 0 && opt->type != Opt_tail)
		err = au_opt_simple(sb, opt++, opts);
	if (err > 0)
		err = 0;
	else if (unlikely(err < 0))
		goto out;

	/* disable xino, hinotify, dlgt temporary */
	sbinfo = stosi(sb);
	tmp = sbinfo->au_si_flags;
	AuFlagSet(sbinfo, f_xino, AuXino_NONE);
	AuFlagSet(sbinfo, f_dlgt, 0);
	AuFlagSet(sbinfo, f_udba, AuUdba_REVAL);

	opt = opts->opt;
	while (err >= 0 && opt->type != Opt_tail)
		err = au_opt_br(sb, opt++, opts);
	if (err > 0)
		err = 0;
	else if (unlikely(err < 0))
		goto out;

	bend = sbend(sb);
	if (unlikely(bend < 0)) {
		err = -EINVAL;
		AuErr("no branches\n");
		goto out;
	}

	AuFlagSet(sbinfo, f_xino, tmp.f_xino);
	opt = opts->opt;
	while (!err && opt->type != Opt_tail)
		err = au_opt_xino(sb, opt++, &opt_xino, opts);
	if (unlikely(err))
		goto out;

	//todo: test this error case.
	err = verify_opts(sb, &tmp, /*remount*/0);
	if (unlikely(err))
		goto out;

	/* enable xino */
	if (tmp.f_xino != AuXino_NONE && !opt_xino) {
		struct au_opt_xino xino;

		xino.file = xino_def(sb);
		err = PTR_ERR(xino.file);
		if (IS_ERR(xino.file))
			goto out;

		err = xino_set(sb, &xino, /*remount*/0);
		fput(xino.file);
		if (unlikely(err))
			goto out;
	}

	/* restore hinotify */
	AuFlagSet(sbinfo, f_udba, tmp.f_udba);
	if (tmp.f_udba == AuUdba_INOTIFY)
		au_reset_hinotify(dir, au_hi_flags(dir, 1) & ~AUFS_HI_XINO);

	/* restore dlgt */
	AuFlagSet(sbinfo, f_dlgt, tmp.f_dlgt);

 out:
	AuTraceErr(err);
	return err;
}

int au_opts_remount(struct super_block *sb, struct au_opts *opts)
{
	int err, rerr;
	struct inode *dir;
	struct au_opt_xino *opt_xino;
	struct au_opt *opt;
	unsigned int dlgt;
	struct aufs_sbinfo *sbinfo;

	AuTraceEnter();
	SiMustWriteLock(sb);
	DiMustWriteLock(sb->s_root);
	dir = sb->s_root->d_inode;
	IiMustWriteLock(dir);
	//AuDebugOn(au_flag_test_udba_inotify(sb));

	err = 0;
	sbinfo = stosi(sb);
	dlgt = au_need_dlgt(sb);
	opt_xino = NULL;
	opt = opts->opt;
	while (err >= 0 && opt->type != Opt_tail) {
		err = au_opt_simple(sb, opt, opts);

		/* disable it temporary */
		dlgt = au_need_dlgt(sb);
		AuFlagSet(sbinfo, f_dlgt, 0);

		if (!err)
			err = au_opt_br(sb, opt, opts);
		if (!err)
			err = au_opt_xino(sb, opt, &opt_xino, opts);

		/* restore it */
		AuFlagSet(sbinfo, f_dlgt, dlgt);
		opt++;
	}
	if (err > 0)
		err = 0;
	AuTraceErr(err);

	/* go on even err */

	//todo: test this error case.
	AuFlagSet(sbinfo, f_dlgt, 0);
	rerr = verify_opts(sb, &sbinfo->au_si_flags, /*remount*/1);
	AuFlagSet(sbinfo, f_dlgt, dlgt);
	if (unlikely(rerr && !err))
		err = rerr;

	if (unlikely(opts->trunc_xib)) {
		rerr = xib_trunc(sb);
		if (unlikely(rerr && !err))
			err = rerr;
	}

	/* they are handled by the caller */
	if (!opts->refresh_dir)
		opts->refresh_dir
			= !!(opts->given.f_udba
			     || AuFlag(sbinfo, f_xino) != AuXino_NONE);

	LKTRTrace("status {%d, %d}\n",
		  opts->refresh_dir, opts->refresh_nondir);
	AuTraceErr(err);
	return err;
}

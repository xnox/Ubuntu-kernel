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

/* $Id: module.c,v 1.27 2007/11/26 01:32:50 sfjro Exp $ */

//#include <linux/init.h>
#include <linux/module.h>
#include "aufs.h"

/* ---------------------------------------------------------------------- */

/*
 * aufs caches
 */
struct kmem_cache *aufs_cachep[AuCache_Last];
static int __init create_cache(void)
{
	void *p;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
#define Args(type, sz)	(type), (sz), 0, SLAB_RECLAIM_ACCOUNT, NULL
#else
#define Args(type, sz)	(type), (sz), 0, SLAB_RECLAIM_ACCOUNT, NULL, NULL
#endif
#define CacheX(type, extra) \
	kmem_cache_create(Args(#type, sizeof(struct type) + extra))
#define Cache(type)		CacheX(type, 0)

	p = NULL;
	aufs_cachep[AuCache_DINFO] = Cache(aufs_dinfo);
	if (aufs_cachep[AuCache_DINFO])
		aufs_cachep[AuCache_ICNTNR] = Cache(aufs_icntnr);
	if (aufs_cachep[AuCache_ICNTNR])
		aufs_cachep[AuCache_FINFO] = Cache(aufs_finfo);
	//aufs_cachep[AuCache_FINFO] = NULL;
	if (aufs_cachep[AuCache_FINFO])
		aufs_cachep[AuCache_VDIR] = Cache(aufs_vdir);
	if (aufs_cachep[AuCache_VDIR]) {
		aufs_cachep[AuCache_DEHSTR] = Cache(aufs_dehstr);
		p = aufs_cachep[AuCache_DEHSTR];
	}

#ifdef CONFIG_AUFS_HINOTIFY
	AuDebugOn(!au_hin_nignore);
	if (p) {
		aufs_cachep[AuCache_HINOTIFY]
			= CacheX(aufs_hinotify,
				 sizeof(atomic_t) * au_hin_nignore);
		p = aufs_cachep[AuCache_HINOTIFY];
	}
#endif

	if (p)
		return 0;
	return -ENOMEM;

#undef CacheX
#undef Cache
#undef Args
}

static void destroy_cache(void)
{
	int i;
	for (i = 0; i < AuCache_Last; i++)
		if (aufs_cachep[i])
			kmem_cache_destroy(aufs_cachep[i]);
}

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_HINOTIFY
/* the size of an array for ignore counter */
int au_hin_nignore;
#endif

char au_esc_chars[0x20 + 3]; /* 0x01-0x20, backslash, del, and NULL */
int au_dir_roflags;

#ifdef DbgDlgt
#include <linux/security.h>
#include "dbg_dlgt.c"
#else
#define dbg_dlgt_init()	0
#define dbg_dlgt_fin()	do {} while (0)
#endif

/*
 * functions for module interface.
 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Junjiro Okajima");
MODULE_DESCRIPTION(AUFS_NAME " -- Another unionfs");
MODULE_VERSION(AUFS_VERSION);

/* it should be 'byte', but param_set_byte() prints it by "%c" */
short aufs_nwkq = AUFS_NWKQ_DEF;
MODULE_PARM_DESC(nwkq, "the number of workqueue thread, " AUFS_WKQ_NAME);
module_param_named(nwkq, aufs_nwkq, short, S_IRUGO);

int sysaufs_brs;
MODULE_PARM_DESC(brs, "use <sysfs>/fs/aufs/brs");
module_param_named(brs, sysaufs_brs, int, S_IRUGO);

char *aufs_sysrq_key = "a";
#ifdef CONFIG_MAGIC_SYSRQ
MODULE_PARM_DESC(sysrq, "MagicSysRq key for " AUFS_NAME);
module_param_named(sysrq, aufs_sysrq_key, charp, S_IRUGO);
#endif

static int __init aufs_init(void)
{
	int err, i;
	char *p;

#ifdef CONFIG_AUFS_DEBUG
	{
		aufs_bindex_t bindex = -1;
		AuDebugOn(bindex >= 0);
	}
	{
		struct aufs_destr destr;
		destr.len = -1;
		AuDebugOn(destr.len < NAME_MAX);
	}

#ifdef CONFIG_4KSTACKS
	AuWarn("CONFIG_4KSTACKS is defined.\n");
#endif
#if 0 // verbose debug
	{
		union {
			struct aufs_branch *br;
			struct aufs_dinfo *di;
			struct aufs_finfo *fi;
			struct aufs_iinfo *ii;
			struct aufs_hinode *hi;
			struct aufs_sbinfo *si;
			struct aufs_destr *destr;
			struct aufs_de *de;
			struct aufs_wh *wh;
			struct aufs_vdir *vd;
		} u;

		pr_info("br{"
			"xino %d, "
			"id %d, perm %d, mnt %d, count %d, "
			"wh_sem %d, wh %d, run %d, plink %d, gen %d} %d\n",
			offsetof(typeof(*u.br), br_xino),
			offsetof(typeof(*u.br), br_id),
			offsetof(typeof(*u.br), br_perm),
			offsetof(typeof(*u.br), br_mnt),
			offsetof(typeof(*u.br), br_count),
			offsetof(typeof(*u.br), br_wh_rwsem),
			offsetof(typeof(*u.br), br_wh),
			offsetof(typeof(*u.br), br_wh_running),
			offsetof(typeof(*u.br), br_plink),
			offsetof(typeof(*u.br), br_generation),
			sizeof(*u.br));
		pr_info("di{gen %d, rwsem %d, bstart %d, bend %d, bwh %d, "
			"bdiropq %d, hdentry %d} %d\n",
			offsetof(typeof(*u.di), di_generation),
			offsetof(typeof(*u.di), di_rwsem),
			offsetof(typeof(*u.di), di_bstart),
			offsetof(typeof(*u.di), di_bend),
			offsetof(typeof(*u.di), di_bwh),
			offsetof(typeof(*u.di), di_bdiropq),
			offsetof(typeof(*u.di), di_hdentry),
			sizeof(*u.di));
		pr_info("fi{gen %d, rwsem %d, hfile %d, bstart %d, bend %d, "
			"h_vm_ops %d, vdir_cach %d} %d\n",
			offsetof(typeof(*u.fi), fi_generation),
			offsetof(typeof(*u.fi), fi_rwsem),
			offsetof(typeof(*u.fi), fi_hfile),
			offsetof(typeof(*u.fi), fi_bstart),
			offsetof(typeof(*u.fi), fi_bend),
			offsetof(typeof(*u.fi), fi_h_vm_ops),
			offsetof(typeof(*u.fi), fi_vdir_cache),
			sizeof(*u.fi));
		pr_info("ii{gen %d, hsb %d, "
			"rwsem %d, bstart %d, bend %d, hinode %d, vdir %d} "
			"%d\n",
			offsetof(typeof(*u.ii), ii_generation),
			offsetof(typeof(*u.ii), ii_hsb1),
			offsetof(typeof(*u.ii), ii_rwsem),
			offsetof(typeof(*u.ii), ii_bstart),
			offsetof(typeof(*u.ii), ii_bend),
			offsetof(typeof(*u.ii), ii_hinode),
			offsetof(typeof(*u.ii), ii_vdir),
			sizeof(*u.ii));
		pr_info("hi{inode %d, id %d, notify %d, wh %d} %d\n",
			offsetof(typeof(*u.hi), hi_inode),
			offsetof(typeof(*u.hi), hi_id),
			offsetof(typeof(*u.hi), hi_notify),
			offsetof(typeof(*u.hi), hi_whdentry),
			sizeof(*u.hi));
		pr_info("si{nwt %d, rwsem %d, gen %d, "
			"bend %d, last id %d, br %d, "
			"flags %d, "
			"xread %d, xwrite %d, xib %d, xmtx %d, buf %d, "
			"xlast %d, xnext %d, "
			"rdcache %d, "
			"dirwh %d, "
			"pl_lock %d, pl %d, "
			"l %d, mnt %d, "
			"sys %d, "
			"} %d\n",
			offsetof(typeof(*u.si), si_nowait),
			offsetof(typeof(*u.si), si_rwsem),
			offsetof(typeof(*u.si), si_generation),
			offsetof(typeof(*u.si), si_bend),
			offsetof(typeof(*u.si), si_last_br_id),
			offsetof(typeof(*u.si), si_branch),
			offsetof(typeof(*u.si), au_si_flags),
			offsetof(typeof(*u.si), si_xread),
			offsetof(typeof(*u.si), si_xwrite),
			offsetof(typeof(*u.si), si_xib),
			offsetof(typeof(*u.si), si_xib_mtx),
			offsetof(typeof(*u.si), si_xib_buf),
			offsetof(typeof(*u.si), si_xib_last_pindex),
			offsetof(typeof(*u.si), si_xib_next_bit),
			offsetof(typeof(*u.si), si_rdcache),
			offsetof(typeof(*u.si), si_dirwh),
			offsetof(typeof(*u.si), si_plink_lock),
			offsetof(typeof(*u.si), si_plink),
			offsetof(typeof(*u.si), si_list),
			offsetof(typeof(*u.si), si_mnt),
			offsetof(typeof(*u.si), si_sysaufs),
			sizeof(*u.si));
		pr_info("destr{len %d, name %d} %d\n",
			offsetof(typeof(*u.destr), len),
			offsetof(typeof(*u.destr), name),
			sizeof(*u.destr));
		pr_info("de{ino %d, type %d, str %d} %d\n",
			offsetof(typeof(*u.de), de_ino),
			offsetof(typeof(*u.de), de_type),
			offsetof(typeof(*u.de), de_str),
			sizeof(*u.de));
		pr_info("wh{hash %d, bindex %d, str %d} %d\n",
			offsetof(typeof(*u.wh), wh_hash),
			offsetof(typeof(*u.wh), wh_bindex),
			offsetof(typeof(*u.wh), wh_str),
			sizeof(*u.wh));
		pr_info("vd{deblk %d, nblk %d, last %d, ver %d, jiffy %d} %d\n",
			offsetof(typeof(*u.vd), vd_deblk),
			offsetof(typeof(*u.vd), vd_nblk),
			offsetof(typeof(*u.vd), vd_last),
			offsetof(typeof(*u.vd), vd_version),
			offsetof(typeof(*u.vd), vd_jiffy),
			sizeof(*u.vd));
	}
#endif
#endif /* CONFIG_AUFS_DEBUG */

	p = au_esc_chars;
	for (i = 1; i <= ' '; i++)
		*p++ = i;
	*p++ = '\\';
	*p++ = '\x7f';
	*p = 0;

	au_dir_roflags = au_file_roflags(O_DIRECTORY | O_LARGEFILE);
#ifndef CONFIG_AUFS_SYSAUFS
	sysaufs_brs = 0;
#endif

	err = -EINVAL;
	if (unlikely(aufs_nwkq <= 0))
		goto out;

	err = sysaufs_init();
	if (unlikely(err))
		goto out;
	err = au_wkq_init();
	if (unlikely(err))
		goto out_sysaufs;
	err = au_inotify_init();
	if (unlikely(err))
		goto out_wkq;
	err = au_sysrq_init();
	if (unlikely(err))
		goto out_inotify;

	err = create_cache();
	if (unlikely(err))
		goto out_sysrq;

	err = dbg_dlgt_init();
	if (unlikely(err))
		goto out_cache;

	err = register_filesystem(&aufs_fs_type);
	if (unlikely(err))
		goto out_dlgt;
	pr_info(AUFS_NAME " " AUFS_VERSION "\n");
	return 0; /* success */

 out_dlgt:
	dbg_dlgt_fin();
 out_cache:
	destroy_cache();
 out_sysrq:
	au_sysrq_fin();
 out_inotify:
	au_inotify_fin();
 out_wkq:
	au_wkq_fin();
 out_sysaufs:
	sysaufs_fin();
 out:
	AuTraceErr(err);
	return err;
}

static void __exit aufs_exit(void)
{
	unregister_filesystem(&aufs_fs_type);
	dbg_dlgt_fin();
	destroy_cache();

	au_sysrq_fin();
	au_inotify_fin();
	au_wkq_fin();
	sysaufs_fin();
}

module_init(aufs_init);
module_exit(aufs_exit);

/* ---------------------------------------------------------------------- */

/* fake Kconfig */
#if 1

#ifdef CONFIG_AUFS_HINOTIFY
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#error CONFIG_AUFS_HINOTIFY is supported in linux-2.6.18 and later.
#endif
#ifndef CONFIG_INOTIFY
#error enable CONFIG_INOTIFY to use CONFIG_AUFS_HINOTIFY.
#endif
#endif /* CONFIG_AUFS_HINOTIFY */

#if AUFS_BRANCH_MAX > 511 && PAGE_SIZE > 4096
#warning pagesize is larger than 4kb, \
	CONFIG_AUFS_BRANCH_MAX_511 or smaller is recommended.
#endif

#ifdef CONFIG_AUFS_SYSAUFS
#ifndef CONFIG_SYSFS
#error CONFIG_AUFS_SYSAUFS requires CONFIG_SYSFS.
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#error CONFIG_AUFS_SYSAUFS requires linux-2.6.18 and later.
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23) \
	&& defined(CONFIG_AUFS_SYSFS_GET_DENTRY_PATCH)
#warning CONFIG_AUFS_SYSFS_GET_DENTRY_PATCH is supported linux-2.6.23 and later.
#endif
#elif defined(CONFIG_AUFS_SYSFS_GET_DENTRY_PATCH)
#warning CONFIG_AUFS_SYSFS_GET_DENTRY_PATCH requires CONFIG_AUFS_SYSAUFS.
#endif /* CONFIG_AUFS_SYSAUFS */

#ifdef CONFIG_AUFS_EXPORT
#if !defined(CONFIG_EXPORTFS) && !defined(CONFIG_EXPORTFS_MODULE)
#error CONFIG_AUFS_EXPORT requires CONFIG_EXPORTFS
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#error CONFIG_AUFS_EXPORT requires linux-2.6.18 and later.
#endif
#if defined(CONFIG_EXPORTFS_MODULE) && defined(CONFIG_AUFS)
#error need CONFIG_EXPORTFS = y to link aufs statically with CONFIG_AUFS_EXPORT
#endif
#endif /* CONFIG_AUFS_EXPORT */

#ifdef CONFIG_AUFS_SEC_PERM_PATCH
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
#warning CONFIG_AUFS_SEC_PERM_PATCH does not support before linux-2.6.24.
#endif
#ifndef CONFIG_SECURITY
#warning CONFIG_AUFS_SEC_PERM_PATCH is unnecessary since CONFIG_SECURITY is disabled.
#endif
#ifdef CONFIG_AUFS
#warning CONFIG_AUFS_SEC_PERM_PATCH is unnecessary since CONFIG_AUFS is not a module.
#endif
#endif

#ifdef CONFIG_AUFS_PUT_FILP_PATCH
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
#warning CONFIG_AUFS_PUT_FILP_PATCH does not support before linux-2.6.19.
#endif
#if !defined(CONFIG_NFS_FS) && !defined(CONFIG_NFS_FS_MODULE)
#warning CONFIG_AUFS_PUT_FILP_PATCH is unnecessary since CONFIG_NFS_FS is disabled.
#endif
#ifdef CONFIG_AUFS
#warning CONFIG_AUFS_PUT_FILP_PATCH is unnecessary since CONFIG_AUFS is not a module.
#endif
#ifdef CONFIG_AUFS_FAKE_DM
#error CONFIG_AUFS_FAKE_DM must be disabled for CONFIG_AUFS_PUT_FILP_PATCH.
#endif
#endif /* CONFIG_AUFS_PUT_FILP_PATCH */

#ifdef CONFIG_AUFS_LHASH_PATCH
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
#error CONFIG_AUFS_LHASH_PATCH does not support before linux-2.6.19.
#endif
#if !defined(CONFIG_NFS_FS) && !defined(CONFIG_NFS_FS_MODULE)
#warning CONFIG_AUFS_LHASH_PATCH is unnecessary since CONFIG_NFS_FS is disabled.
#endif
#ifdef CONFIG_AUFS_FAKE_DM
#error CONFIG_AUFS_FAKE_DM must be disabled for CONFIG_AUFS_LHASH_PATCH.
#endif
#endif

#if defined(CONFIG_AUFS_KSIZE_PATCH) \
	&& LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#warning CONFIG_AUFS_KSIZE_PATCH is unnecessary for linux-2.6.22 and later.
#endif

#ifdef CONFIG_AUFS_WORKAROUND_FUSE
#if !defined(CONFIG_FUSE_FS) && !defined(CONFIG_FUSE_FS_MODULE)
#warning CONFIG_AUFS_WORKAROUND_FUSE is enabled while FUSE is disabled.
#endif
#endif

#if defined(CONFIG_AUFS_SPLICE_PATCH) \
	&& LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
#error CONFIG_AUFS_SPLICE_PATCH is supported linux-2.6.23 and later.
#endif

#ifdef CONFIG_DEBUG_PROVE_LOCKING
#if MAX_LOCKDEP_SUBCLASSES < AuLsc_I_End
#warning lockdep will not work since aufs uses deeper locks.
#endif
#endif

#ifdef CONFIG_AUFS_COMPAT
#warning CONFIG_AUFS_COMPAT will be removed in the near future.
#endif

#endif

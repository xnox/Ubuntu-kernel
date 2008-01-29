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

/* $Id: sysaufs.c,v 1.24 2008/01/21 04:56:54 sfjro Exp $ */

#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/sysfs.h>
#include <linux/version.h>
#ifdef CONFIG_AUFS_SYSFS_GET_DENTRY_PATCH
#include <linux/fs.h>
#include <../fs/sysfs/sysfs.h>
#endif
#include "aufs.h"

/* ---------------------------------------------------------------------- */

/* super_blocks list is not exported */
static DEFINE_MUTEX(aufs_sbilist_mtx);
static LIST_HEAD(aufs_sbilist);

/* ---------------------------------------------------------------------- */

typedef ssize_t (*rwfunc_t)(struct kobject *kobj, char *buf, loff_t offset,
			    size_t sz, struct sysaufs_args *args);
static ssize_t sysaufs_read(struct kobject *kobj, char *buf, loff_t offset,
			    size_t sz, struct sysaufs_args *args);
static ssize_t sysaufs_free_write(struct kobject *kobj, char *buf, loff_t
				  offset, size_t sz, struct sysaufs_args *args);

#define GFuncBody(_index, func) \
{ \
	struct sysaufs_args args = { \
		.index	= (_index), \
		.mtx	= &aufs_sbilist_mtx, \
		.sb	= NULL \
	}; \
	return func(kobj, buf, offset, sz, &args); \
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
#define GFunc(name, _index, func) \
static ssize_t name(struct kobject *kobj, struct bin_attribute *attr, \
	char *buf, loff_t offset, size_t sz) \
GFuncBody(_index, func)
#else
#define GFunc(name, _index, func) \
static ssize_t name(struct kobject *kobj, char *buf, loff_t offset, size_t sz) \
GFuncBody(_index, func)
#endif

#define GFuncs(name, _index) \
	GFunc(read_##name, _index, sysaufs_read) \
	GFunc(write_##name, _index, sysaufs_free_write)

static struct super_block *find_sb_lock(struct kobject *kobj)
{
	struct super_block *sb;
	struct aufs_sbinfo *sbinfo;

	AuTraceEnter();
	MtxMustLock(&aufs_sbilist_mtx);

	sb = NULL;
	list_for_each_entry(sbinfo, &aufs_sbilist, si_list) {
		if (&au_subsys_to_kset(sbinfo->si_sysaufs.subsys).kobj != kobj)
			continue;
		sb = sbinfo->si_mnt->mnt_sb;
		si_read_lock(sb, !AuLock_FLUSH);
		break;
	}
	return sb;
}

static ssize_t sb_func(struct kobject *kobj, char *buf, loff_t offset,
		       size_t sz, struct sysaufs_args *args, rwfunc_t func)
{
	ssize_t err;

	err = -ENOENT;
	mutex_lock(&aufs_sbilist_mtx);
	args->sb = find_sb_lock(kobj);
	if (args->sb) {
		err = func(kobj, buf, offset, sz, args);
		si_read_unlock(args->sb);
	}
	mutex_unlock(&aufs_sbilist_mtx);
	return err;
}

#define SbFuncBody(_index, func) \
{ \
	struct sysaufs_args args = { \
		.index	= (_index), \
		.mtx	= NULL \
	}; \
	return sb_func(kobj, buf, offset, sz, &args, func); \
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
#define SbFunc(name, _index, func) \
static ssize_t name(struct kobject *kobj, struct bin_attribute *attr, \
	char *buf, loff_t offset, size_t sz) \
SbFuncBody(_index, func)
#else
#define SbFunc(name, _index, func) \
static ssize_t name(struct kobject *kobj, char *buf, loff_t offset, size_t sz) \
SbFuncBody(_index, func)
#endif

#define SbFuncs(name, index) \
	SbFunc(read_##name, index, sysaufs_read) \
	SbFunc(write_##name, index, sysaufs_free_write)

static decl_subsys(aufs, NULL, NULL);
enum { Brs, Stat, Config, _Last };
static struct sysaufs_entry g_array[_Last];
/*
 * read_brs, write_brs,
 * read_stat, write_stat,
 * read_config, write_config
 */
GFuncs(brs, Brs);
GFuncs(stat, Stat);
GFuncs(config, Config);

/*
 * read_xino, write_xino
 */
SbFuncs(xino, SysaufsSb_XINO);

#define SetEntry(e, _name, init_size, _ops) \
	do { \
		(e)->attr.attr.name = #_name; \
		(e)->attr.attr.owner = THIS_MODULE; \
		(e)->attr.attr.mode = S_IRUGO | S_IWUSR; \
		(e)->attr.read = read_##_name; \
		(e)->attr.write = write_##_name; \
		(e)->allocated = init_size; \
		(e)->err = -1; \
		(e)->ops = _ops; \
	} while (0)

#define Priv(e)		(e)->attr.private
#define Allocated(e)	(e)->allocated
#define Len(e)		(e)->attr.size
#define Name(e)		attr_name((e)->attr)

/* ---------------------------------------------------------------------- */

static void free_entry(struct sysaufs_entry *e)
{
	MtxMustLock(&aufs_sbilist_mtx);
	AuDebugOn(!Priv(e));

	if (Allocated(e) > 0)
		kfree(Priv(e));
	else
		free_pages((unsigned long)Priv(e), -Allocated(e));
	Priv(e) = NULL;
	Len(e) = 0;
}

static void free_entries(void)
{
	static int a[] = { Brs, -1 };
	int *p = a;

	MtxMustLock(&aufs_sbilist_mtx);

	while (*p >= 0) {
		if (Priv(g_array + *p))
			free_entry(g_array + *p);
		p++;
	}
}

static int alloc_entry(struct sysaufs_entry *e)
{
	MtxMustLock(&aufs_sbilist_mtx);
	AuDebugOn(Priv(e));

	if (Allocated(e) > 0)
		Priv(e) = kmalloc(Allocated(e), GFP_TEMPORARY);
	else
		Priv(e) = (void *)__get_free_pages(GFP_TEMPORARY,
						   -Allocated(e));
	if (Priv(e))
		return 0;
	return -ENOMEM;
}

/* ---------------------------------------------------------------------- */

static void unreg(au_subsys_t *subsys, struct sysaufs_entry *a, int n,
		  au_subsys_t *parent)
{
	int i;

	AuTraceEnter();

	for (i = 0; i < n; i++, a++)
		if (!a->err) {
			sysfs_remove_bin_file
				(&au_subsys_to_kset(*subsys).kobj, &a->attr);
			if (Priv(a))
				free_entry(a);
		}

	subsystem_unregister(subsys);
	subsys_put(parent);
}

static int reg(au_subsys_t *subsys, struct sysaufs_entry *a, int n,
	       au_subsys_t *parent)
{
	int err, i;

	AuTraceEnter();

	subsys_get(parent);
	kobj_set_kset_s(&au_subsys_to_kset(*subsys), *parent);
	err = subsystem_register(subsys);
	if (unlikely(err))
		goto out;

	for (i = 0; !err && i < n; i++) {
		a[i].err = sysfs_create_bin_file
			(&au_subsys_to_kset(*subsys).kobj, &a[i].attr);
		err = a[i].err;
	}
	if (unlikely(err))
		unreg(subsys, a, n, parent);

 out:
	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

#define SbSetEntry(index, name, init_size) \
	SetEntry(sa->array + index, name, init_size, au_si_ops);

void sysaufs_add(struct aufs_sbinfo *sbinfo)
{
	int err;
	struct sysaufs_sbinfo *sa = &sbinfo->si_sysaufs;

	AuTraceEnter();

	mutex_lock(&aufs_sbilist_mtx);
	list_add_tail(&sbinfo->si_list, &aufs_sbilist);
	free_entries();

	memset(sa, 0, sizeof(*sa));
	SbSetEntry(SysaufsSb_XINO, xino, 128);
	err = kobject_set_name(&au_subsys_to_kset(sa->subsys).kobj, "%p",
			       sbinfo->si_mnt->mnt_sb);
	if (!err)
		err = reg(&sa->subsys, sa->array, SysaufsSb_Last, &aufs_subsys);
	if (unlikely(err))
		AuWarn("failed adding sysfs (%d)\n", err);

	mutex_unlock(&aufs_sbilist_mtx);
}

void sysaufs_del(struct aufs_sbinfo *sbinfo)
{
	struct sysaufs_sbinfo *sa = &sbinfo->si_sysaufs;

	AuTraceEnter();

	mutex_lock(&aufs_sbilist_mtx);
	unreg(&sa->subsys, sa->array, SysaufsSb_Last, &aufs_subsys);
	list_del(&sbinfo->si_list);
	free_entries();
	mutex_unlock(&aufs_sbilist_mtx);
}

void sysaufs_notify_remount(void)
{
	mutex_lock(&aufs_sbilist_mtx);
	free_entries();
	mutex_unlock(&aufs_sbilist_mtx);
}

/* ---------------------------------------------------------------------- */

static int make_brs(struct seq_file *seq, struct sysaufs_args *args,
		    int *do_size)
{
	int err;
	struct aufs_sbinfo *sbinfo;

	AuTraceEnter();
	MtxMustLock(&aufs_sbilist_mtx);
	AuDebugOn(args->index != Brs);

	err = 0;
	list_for_each_entry(sbinfo, &aufs_sbilist, si_list) {
		struct super_block *sb;
		struct dentry *root;
		struct vfsmount *mnt;

		sb = sbinfo->si_mnt->mnt_sb;
		root = sb->s_root;
		aufs_read_lock(root, !AuLock_IR);
		mnt = sbinfo->si_mnt;
		err = seq_escape
			(seq, mnt->mnt_devname ? mnt->mnt_devname : "none",
			 au_esc_chars);
		if (!err)
			err = seq_putc(seq, ' ');
		if (!err)
			err = seq_path(seq, mnt, root, au_esc_chars);
		if (err > 0)
			err = seq_printf(seq, " %p br:", sb);
		if (!err)
			err = au_show_brs(seq, sb);
		aufs_read_unlock(root, !AuLock_IR);
		if (!err)
			err = seq_putc(seq, '\n');
		else
			break;
	}

	AuTraceErr(err);
	return err;
}

static int make_config(struct seq_file *seq, struct sysaufs_args *args,
		       int *do_size)
{
	int err;

	AuTraceEnter();
	AuDebugOn(args->index != Config);

#ifdef CONFIG_AUFS
	err = seq_puts(seq, "CONFIG_AUFS=y\n");
#else
	err = seq_puts(seq, "CONFIG_AUFS=m\n");
#endif

#define puts(m, v) do { \
	if (!err) \
		err = seq_puts(seq, "CONFIG_AUFS_" #m "=" #v "\n"); \
} while (0)
#define puts_bool(m)	puts(m, y)
#define puts_mod(m)	puts(m, m)

#ifdef CONFIG_AUFS_FAKE_DM
	puts_bool(FAKE_DM);
#endif
#ifdef CONFIG_AUFS_BRANCH_MAX_127
	puts_bool(BRANCH_MAX_127);
#elif defined(CONFIG_AUFS_BRANCH_MAX_511)
	puts_bool(BRANCH_MAX_511);
#elif defined(CONFIG_AUFS_BRANCH_MAX_1023)
	puts_bool(BRANCH_MAX_1023);
#elif defined(CONFIG_AUFS_BRANCH_MAX_32767)
	puts_bool(BRANCH_MAX_32767);
#endif
	puts_bool(SYSAUFS);
#ifdef CONFIG_AUFS_HINOTIFY
	puts_bool(HINOTIFY);
#endif
#ifdef CONFIG_AUFS_EXPORT
	puts_bool(EXPORT);
#endif
#ifdef CONFIG_AUFS_ROBR
	puts_bool(ROBR);
#endif
#ifdef CONFIG_AUFS_DLGT
	puts_bool(DLGT);
#endif
#ifdef CONFIG_AUFS_RR_SQUASHFS
	puts_bool(RR_SQUASHFS);
#endif
#ifdef CONFIG_AUFS_SEC_PERM_PATCH
	puts_bool(SEC_PERM_PATCH);
#endif
#ifdef CONFIG_AUFS_SPLICE_PATCH
	puts_bool(SPLICE_PATCH);
#endif
#ifdef CONFIG_AUFS_PUT_FILP_PATCH
	puts_bool(PUT_FILP_PATCH);
#endif
#ifdef CONFIG_AUFS_LHASH_PATCH
	puts_bool(LHASH_PATCH);
#endif
#ifdef CONFIG_AUFS_DENY_WRITE_ACCESS_PATCH
	puts_bool(DENY_WRITE_ACCESS_PATCH);
#endif
#ifdef CONFIG_AUFS_SYSFS_GET_DENTRY_PATCH
	puts_bool(SYSFS_GET_DENTRY_PATCH);
#endif
#ifdef CONFIG_AUFS_KSIZE_PATCH
	puts_bool(KSIZE_PATCH);
#endif
#ifdef CONFIG_AUFS_WORKAROUND_FUSE
	puts_bool(WORKAROUND_FUSE);
#endif
#ifdef CONFIG_AUFS_DEBUG
	puts_bool(DEBUG);
#endif
#ifdef CONFIG_AUFS_COMPAT
	puts_bool(COMPAT);
#endif
#ifdef DbgUdbaRace
	if (!err)
		err = seq_printf(seq, "DbgUdbaRace=%d\n", DbgUdbaRace);
#endif

#undef puts_bool
#undef puts

	AuTraceErr(err);
	return err;
}

static int make_stat(struct seq_file *seq, struct sysaufs_args *args,
		     int *do_size)
{
	int err, i;

	AuTraceEnter();
	AuDebugOn(args->index != Stat);

	*do_size = 0;
	err = seq_puts(seq, "wkq max_busy:");
	for (i = 0; !err && i < aufs_nwkq; i++)
		err = seq_printf(seq, " %u", au_wkq[i].max_busy);
	if (!err)
		err = seq_printf(seq, ", %u(generic)\n",
				 au_wkq[aufs_nwkq].max_busy);
	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static int make(struct sysaufs_entry *e, struct sysaufs_args *args,
		int *do_size)
{
	int err;
	struct seq_file *seq;

	AuTraceEnter();
	AuDebugOn(Priv(e));
	MtxMustLock(&aufs_sbilist_mtx);

	err = -ENOMEM;
	seq = kzalloc(sizeof(*seq), GFP_TEMPORARY);
	if (unlikely(!seq))
		goto out;

	Len(e) = 0;
	while (1) {
		err = alloc_entry(e);
		if (unlikely(err))
			break;

		//mutex_init(&seq.lock);
		seq->buf = Priv(e);
		seq->count = 0;
		seq->size = Allocated(e);
		if (unlikely(Allocated(e) <= 0))
			seq->size = PAGE_SIZE << -Allocated(e);

		err = e->ops[args->index](seq, args, do_size);
		if (!err) {
			Len(e) = seq->count;
			break; /* success */
		}

		free_entry(e);
		if (Allocated(e) > 0) {
			Allocated(e) <<= 1;
			if (unlikely(Allocated(e) >= (int)PAGE_SIZE))
				Allocated(e) = 0;
		} else
			Allocated(e)--;
	}
	kfree(seq);

 out:
	AuTraceErr(err);
	return err;
}

/* why does sysfs pass my parent kobject? */
static struct dentry *find_me(struct kobject *kobj, struct sysaufs_entry *e)
{
#if 1
	struct dentry *dentry, *parent;
	const char *name = Name(e);
	const unsigned int len = strlen(name);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
	parent = kobj->dentry;
#elif defined(CONFIG_AUFS_SYSFS_GET_DENTRY_PATCH)
	parent = sysfs_get_dentry(kobj->sd);
#else
	return NULL;
#endif

	spin_lock(&dcache_lock);
	list_for_each_entry(dentry, &parent->d_subdirs, D_CHILD) {
		if (len == dentry->d_name.len
		    && !strcmp(dentry->d_name.name, name)) {
			spin_unlock(&dcache_lock);
			return dentry;
		}
	}
	spin_unlock(&dcache_lock);
#endif
	return NULL;
}

static ssize_t sysaufs_read(struct kobject *kobj, char *buf, loff_t offset,
			    size_t sz, struct sysaufs_args *args)
{
	ssize_t err;
	loff_t len;
	struct dentry *d;
	struct sysaufs_entry *e;
	int do_size;

	LKTRTrace("{%d, %p}, offset %Ld, sz %lu\n",
		  args->index, args->sb, offset, (unsigned long)sz);

	if (unlikely(!sz))
		return 0;

	err = 0;
	d = NULL;
	e = g_array + args->index;
	if (args->sb)
		e = stosi(args->sb)->si_sysaufs.array + args->index;

	do_size = 1;
	if (args->mtx)
		mutex_lock(args->mtx);
	if (unlikely(!Priv(e))) {
		err = make(e, args, &do_size);
		AuDebugOn(Len(e) > INT_MAX);
		if (do_size) {
			d = find_me(kobj, e);
			if (d)
				i_size_write(d->d_inode, Len(e));
		}
	}

	if (!err) {
		len = Len(e) - offset;
		err = len;
		LKTRTrace("%Ld\n", len);
		if (len > 0) {
			if (len > sz)
				err = sz;
			memcpy(buf, Priv(e) + offset, err);
		}

		if (!do_size)
			free_entry(e);
	}
	if (args->mtx)
		mutex_unlock(args->mtx);

	AuTraceErr(err);
	return err;
}

static ssize_t sysaufs_free_write(struct kobject *kobj, char *buf,
				  loff_t offset, size_t sz,
				  struct sysaufs_args *args)
{
	struct dentry *d;
	int allocated, len;
	struct sysaufs_entry *e;

	LKTRTrace("{%d, %p}\n", args->index, args->sb);

	e = g_array + args->index;
	if (args->sb)
		e = stosi(args->sb)->si_sysaufs.array + args->index;

	if (args->mtx)
		mutex_lock(args->mtx);
	if (Priv(e)) {
		allocated = Allocated(e);
		if (unlikely(allocated <= 0))
			allocated = PAGE_SIZE << -allocated;
		allocated >>= 1;
		len = Len(e);

		free_entry(e);
		if (unlikely(len <= allocated)) {
			if (Allocated(e) >= 0)
				Allocated(e) = allocated;
			else
				Allocated(e)++;
		}

		d = find_me(kobj, e);
		if (d && i_size_read(d->d_inode))
			i_size_write(d->d_inode, 0);
	}
	if (args->mtx)
		mutex_unlock(args->mtx);

	return sz;
}

static sysaufs_op g_ops[] = {
	[Brs]		= make_brs,
	[Stat]		= make_stat,
	[Config]	= make_config
};

/* ---------------------------------------------------------------------- */

#define GSetEntry(index, name, init_size) \
	SetEntry(g_array + index, name, init_size, g_ops)

int __init sysaufs_init(void)
{
	int err;

	GSetEntry(Brs, brs, 128);
	GSetEntry(Stat, stat, 32);
	GSetEntry(Config, config, 256);
	err = reg(&aufs_subsys, g_array, _Last, &fs_subsys);
	AuTraceErr(err);
	return err;
}

void sysaufs_fin(void)
{
	mutex_lock(&aufs_sbilist_mtx);
	unreg(&aufs_subsys, g_array, _Last, &fs_subsys);
	mutex_unlock(&aufs_sbilist_mtx);
}

/* ---------------------------------------------------------------------- */

void au_each_sb(each_sb_cb_t cb, int do_lock)
{
	struct aufs_sbinfo *sbinfo;

	if (do_lock)
		mutex_lock(&aufs_sbilist_mtx);
	list_for_each_entry(sbinfo, &aufs_sbilist, si_list)
		cb(sbinfo->si_mnt->mnt_sb);
	if (do_lock)
		mutex_unlock(&aufs_sbilist_mtx);
}

/* ---------------------------------------------------------------------- */

#ifdef DbgDlgt
int au_test_branch(struct super_block *h_sb)
{
	int found = 0;
	struct aufs_sbinfo *sbinfo;

	mutex_lock(&aufs_sbilist_mtx);
	list_for_each_entry(sbinfo, &aufs_sbilist, si_list) {
		aufs_bindex_t bindex, bend;
		struct super_block *sb;

		sb = sbinfo->si_mnt->mnt_sb;
		si_read_lock(sb);
		bend = sbend(sb);
		for (bindex = 0; !found && bindex <= bend; bindex++)
			found = (h_sb == sbr_sb(sb, bindex));
		si_read_unlock(sb);
	}
	mutex_unlock(&aufs_sbilist_mtx);
	return found;
}
#endif

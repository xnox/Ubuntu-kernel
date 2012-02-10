/*
 *  linux/fs/proc/net.c
 *
 *  Copyright (C) 2007
 *
 *  Author: Eric Biederman <ebiederm@xmission.com>
 *
 *  proc net directory handling functions
 */

#include <asm/uaccess.h>

#include <linux/errno.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/smp_lock.h>
#include <linux/mount.h>
#include <linux/nsproxy.h>
#include <net/net_namespace.h>

#include "internal.h"


struct proc_dir_entry *proc_net_fops_create(struct net *net,
	const char *name, mode_t mode, const struct file_operations *fops)
{
	struct proc_dir_entry *res;

	res = create_proc_entry(name, mode, get_exec_env()->_proc_net);
	if (res)
		res->proc_fops = fops;
	return res;
}
EXPORT_SYMBOL_GPL(proc_net_fops_create);

void proc_net_remove(struct net *net, const char *name)
{
	remove_proc_entry(name, get_exec_env()->_proc_net);
}
EXPORT_SYMBOL_GPL(proc_net_remove);

struct net *get_proc_net(const struct inode *inode)
{
	return maybe_get_net(PDE_NET(PDE(inode)));
}
EXPORT_SYMBOL_GPL(get_proc_net);

static struct proc_dir_entry *proc_net_shadow(struct task_struct *task,
						struct proc_dir_entry *de)
{
	return task->nsproxy->net_ns->proc_net;
}

int __init proc_net_init(void)
{
	struct proc_dir_entry *pde;

	pde = proc_mkdir("net", NULL);
	pde->shadow_proc = proc_net_shadow;
	init_net.proc_net = pde;
	ve0._proc_net = pde;
	pde->data = &init_net;

	pde = proc_mkdir("stat", pde);
	init_net.proc_net_stat = pde;
	ve0._proc_net_stat = pde;
	pde->data = &init_net;

	return 0;
}

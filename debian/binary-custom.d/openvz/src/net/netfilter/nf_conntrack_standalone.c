/* (C) 1999-2001 Paul `Rusty' Russell
 * (C) 2002-2004 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/module.h>
#include <linux/nsproxy.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/percpu.h>
#include <linux/netdevice.h>
#include <net/net_namespace.h>
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif
#include <linux/nfcalls.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>

MODULE_LICENSE("GPL");

int ip_conntrack_disable_ve0 = 0;
module_param(ip_conntrack_disable_ve0, int, 0440);
EXPORT_SYMBOL(ip_conntrack_disable_ve0);

#ifdef CONFIG_PROC_FS
int
print_tuple(struct seq_file *s, const struct nf_conntrack_tuple *tuple,
	    struct nf_conntrack_l3proto *l3proto,
	    struct nf_conntrack_l4proto *l4proto)
{
	return l3proto->print_tuple(s, tuple) || l4proto->print_tuple(s, tuple);
}
EXPORT_SYMBOL_GPL(print_tuple);

#ifdef CONFIG_NF_CT_ACCT
static unsigned int
seq_print_counters(struct seq_file *s,
		   const struct ip_conntrack_counter *counter)
{
	return seq_printf(s, "packets=%llu bytes=%llu ",
			  (unsigned long long)counter->packets,
			  (unsigned long long)counter->bytes);
}
#else
#define seq_print_counters(x, y)	0
#endif

struct ct_iter_state {
	unsigned int bucket;
};

static struct hlist_node *ct_get_first(struct seq_file *seq)
{
	struct ct_iter_state *st = seq->private;

	for (st->bucket = 0;
	     st->bucket < nf_conntrack_htable_size;
	     st->bucket++) {
		if (!hlist_empty(&ve_nf_conntrack_hash[st->bucket]))
			return ve_nf_conntrack_hash[st->bucket].first;
	}
	return NULL;
}

static struct hlist_node *ct_get_next(struct seq_file *seq,
				      struct hlist_node *head)
{
	struct ct_iter_state *st = seq->private;

	head = head->next;
	while (head == NULL) {
		if (++st->bucket >= nf_conntrack_htable_size)
			return NULL;
		head = ve_nf_conntrack_hash[st->bucket].first;
	}
	return head;
}

static struct hlist_node *ct_get_idx(struct seq_file *seq, loff_t pos)
{
	struct hlist_node *head = ct_get_first(seq);

	if (head)
		while (pos && (head = ct_get_next(seq, head)))
			pos--;
	return pos ? NULL : head;
}

static void *ct_seq_start(struct seq_file *seq, loff_t *pos)
{
	read_lock_bh(&nf_conntrack_lock);
	return ct_get_idx(seq, *pos);
}

static void *ct_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;
	return ct_get_next(s, v);
}

static void ct_seq_stop(struct seq_file *s, void *v)
{
	read_unlock_bh(&nf_conntrack_lock);
}

/* return 0 on success, 1 in case of error */
static int ct_seq_show(struct seq_file *s, void *v)
{
	const struct nf_conntrack_tuple_hash *hash = v;
	const struct nf_conn *conntrack = nf_ct_tuplehash_to_ctrack(hash);
	struct nf_conntrack_l3proto *l3proto;
	struct nf_conntrack_l4proto *l4proto;

	NF_CT_ASSERT(conntrack);

	/* we only want to print DIR_ORIGINAL */
	if (NF_CT_DIRECTION(hash))
		return 0;

	l3proto = __nf_ct_l3proto_find(conntrack->tuplehash[IP_CT_DIR_ORIGINAL]
				       .tuple.src.l3num);

	NF_CT_ASSERT(l3proto);
	l4proto = __nf_ct_l4proto_find(conntrack->tuplehash[IP_CT_DIR_ORIGINAL]
				   .tuple.src.l3num,
				   conntrack->tuplehash[IP_CT_DIR_ORIGINAL]
				   .tuple.dst.protonum);
	NF_CT_ASSERT(l4proto);

	if (seq_printf(s, "%-8s %u %-8s %u %ld ",
		       l3proto->name,
		       conntrack->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.l3num,
		       l4proto->name,
		       conntrack->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.protonum,
		       timer_pending(&conntrack->timeout)
		       ? (long)(conntrack->timeout.expires - jiffies)/HZ : 0) != 0)
		return -ENOSPC;

	if (l3proto->print_conntrack(s, conntrack))
		return -ENOSPC;

	if (l4proto->print_conntrack(s, conntrack))
		return -ENOSPC;

	if (print_tuple(s, &conntrack->tuplehash[IP_CT_DIR_ORIGINAL].tuple,
			l3proto, l4proto))
		return -ENOSPC;

	if (seq_print_counters(s, &conntrack->counters[IP_CT_DIR_ORIGINAL]))
		return -ENOSPC;

	if (!(test_bit(IPS_SEEN_REPLY_BIT, &conntrack->status)))
		if (seq_printf(s, "[UNREPLIED] "))
			return -ENOSPC;

	if (print_tuple(s, &conntrack->tuplehash[IP_CT_DIR_REPLY].tuple,
			l3proto, l4proto))
		return -ENOSPC;

	if (seq_print_counters(s, &conntrack->counters[IP_CT_DIR_REPLY]))
		return -ENOSPC;

	if (test_bit(IPS_ASSURED_BIT, &conntrack->status))
		if (seq_printf(s, "[ASSURED] "))
			return -ENOSPC;

#if defined(CONFIG_NF_CONNTRACK_MARK)
	if (seq_printf(s, "mark=%u ", conntrack->mark))
		return -ENOSPC;
#endif

#ifdef CONFIG_NF_CONNTRACK_SECMARK
	if (seq_printf(s, "secmark=%u ", conntrack->secmark))
		return -ENOSPC;
#endif

	if (seq_printf(s, "use=%u\n", atomic_read(&conntrack->ct_general.use)))
		return -ENOSPC;

	return 0;
}

static const struct seq_operations ct_seq_ops = {
	.start = ct_seq_start,
	.next  = ct_seq_next,
	.stop  = ct_seq_stop,
	.show  = ct_seq_show
};

static int ct_open(struct inode *inode, struct file *file)
{
	return seq_open_private(file, &ct_seq_ops,
			sizeof(struct ct_iter_state));
}

static const struct file_operations ct_file_ops = {
	.owner   = THIS_MODULE,
	.open    = ct_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release_private,
};

static void *ct_cpu_seq_start(struct seq_file *seq, loff_t *pos)
{
	int cpu;

	if (*pos == 0)
		return SEQ_START_TOKEN;

	for (cpu = *pos-1; cpu < NR_CPUS; ++cpu) {
		if (!cpu_possible(cpu))
			continue;
		*pos = cpu + 1;
		return &per_cpu(nf_conntrack_stat, cpu);
	}

	return NULL;
}

static void *ct_cpu_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	int cpu;

	for (cpu = *pos; cpu < NR_CPUS; ++cpu) {
		if (!cpu_possible(cpu))
			continue;
		*pos = cpu + 1;
		return &per_cpu(nf_conntrack_stat, cpu);
	}

	return NULL;
}

static void ct_cpu_seq_stop(struct seq_file *seq, void *v)
{
}

static int ct_cpu_seq_show(struct seq_file *seq, void *v)
{
	unsigned int nr_conntracks = atomic_read(&ve_nf_conntrack_count);
	struct ip_conntrack_stat *st = v;

	if (v == SEQ_START_TOKEN) {
		seq_printf(seq, "entries  searched found new invalid ignore delete delete_list insert insert_failed drop early_drop icmp_error  expect_new expect_create expect_delete\n");
		return 0;
	}

	seq_printf(seq, "%08x  %08x %08x %08x %08x %08x %08x %08x "
			"%08x %08x %08x %08x %08x  %08x %08x %08x \n",
		   nr_conntracks,
		   st->searched,
		   st->found,
		   st->new,
		   st->invalid,
		   st->ignore,
		   st->delete,
		   st->delete_list,
		   st->insert,
		   st->insert_failed,
		   st->drop,
		   st->early_drop,
		   st->error,

		   st->expect_new,
		   st->expect_create,
		   st->expect_delete
		);
	return 0;
}

static const struct seq_operations ct_cpu_seq_ops = {
	.start	= ct_cpu_seq_start,
	.next	= ct_cpu_seq_next,
	.stop	= ct_cpu_seq_stop,
	.show	= ct_cpu_seq_show,
};

static int ct_cpu_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ct_cpu_seq_ops);
}

static const struct file_operations ct_cpu_seq_fops = {
	.owner	 = THIS_MODULE,
	.open	 = ct_cpu_seq_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = seq_release_private,
};

static int nf_conntrack_init_ve_proc(struct ve_struct *ve)
{
	struct net *net = ve->ve_netns;
	struct proc_dir_entry *proc, *proc_stat;
	int create_proc_net_stat_nf_conntrack = 1;

	proc = proc_net_fops_create(net, "nf_conntrack", 0440, &ct_file_ops);
	if (!proc)
		goto out;
#ifdef CONFIG_VE_IPTABLES
	create_proc_net_stat_nf_conntrack = ve_is_super(get_exec_env());
#endif
	if (create_proc_net_stat_nf_conntrack) {
		proc_stat = create_proc_entry("nf_conntrack", S_IRUGO,
						net->proc_net_stat);
		if (!proc_stat)
			goto out_rm_nf_conntrack_expect;
		proc_stat->proc_fops = &ct_cpu_seq_fops;
		proc_stat->owner = THIS_MODULE;
	}
	return 0;
out_rm_nf_conntrack_expect:
	proc_net_remove(net, "nf_conntrack");
out:
	return -ENOMEM;
}

static void nf_conntrack_fini_ve_proc(struct ve_struct *ve)
{
	struct net *net = ve->ve_netns;
	int remove_proc_net_stat_nf_conntrack = 1;

#ifdef CONFIG_VE_IPTABLES
	remove_proc_net_stat_nf_conntrack = ve_is_super(get_exec_env());
#endif
	if (remove_proc_net_stat_nf_conntrack)
		remove_proc_entry("nf_conntrack", net->proc_net_stat);
	proc_net_remove(net, "nf_conntrack");
}
#else
static inline int nf_conntrack_init_ve_proc(struct ve_struct *ve)
{
	return 0;
}

static inline void nf_conntrack_fini_ve_proc(struct ve_struct *ve)
{
}
#endif /* CONFIG_PROC_FS */

/* Sysctl support */

int nf_conntrack_checksum __read_mostly = 1;
EXPORT_SYMBOL_GPL(nf_conntrack_checksum);

#ifdef CONFIG_SYSCTL
/* Log invalid packets of a given protocol */
static int log_invalid_proto_min = 0;
static int log_invalid_proto_max = 255;

static struct ctl_table_header *nf_ct_sysctl_header;

static ctl_table nf_ct_sysctl_table[] = {
	{
		.ctl_name	= NET_NF_CONNTRACK_MAX,
		.procname	= "nf_conntrack_max",
		.data		= &nf_conntrack_max,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec,
	},
	{
		.ctl_name	= NET_NF_CONNTRACK_COUNT,
		.procname	= "nf_conntrack_count",
		.data		= &nf_conntrack_count,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= &proc_dointvec,
	},
	{
		.ctl_name       = NET_NF_CONNTRACK_BUCKETS,
		.procname       = "nf_conntrack_buckets",
		.data           = &nf_conntrack_htable_size,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0444,
		.proc_handler   = &proc_dointvec,
	},
	{
		.ctl_name	= NET_NF_CONNTRACK_CHECKSUM,
		.procname	= "nf_conntrack_checksum",
		.data		= &nf_conntrack_checksum,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec,
	},
	{
		.ctl_name	= NET_NF_CONNTRACK_LOG_INVALID,
		.procname	= "nf_conntrack_log_invalid",
		.data		= &nf_ct_log_invalid,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec_minmax,
		.strategy	= &sysctl_intvec,
		.extra1		= &log_invalid_proto_min,
		.extra2		= &log_invalid_proto_max,
	},
	{
		.ctl_name	= CTL_UNNUMBERED,
		.procname	= "nf_conntrack_expect_max",
		.data		= &nf_ct_expect_max,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec,
	},
	{ .ctl_name = 0 }
};

#define NET_NF_CONNTRACK_MAX 2089

static ctl_table nf_ct_netfilter_table[] = {
	{
		.ctl_name	= NET_NETFILTER,
		.procname	= "netfilter",
		.mode		= 0555,
		.child		= nf_ct_sysctl_table,
	},
	{
		.ctl_name	= NET_NF_CONNTRACK_MAX,
		.procname	= "nf_conntrack_max",
		.data		= &nf_conntrack_max,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec,
	},
	{ .ctl_name = 0 }
};

static ctl_table nf_ct_net_table[] = {
	{
		.ctl_name	= CTL_NET,
		.procname	= "net",
		.mode		= 0555,
		.child		= nf_ct_netfilter_table,
	},
	{ .ctl_name = 0 }
};
EXPORT_SYMBOL_GPL(nf_ct_log_invalid);
#endif /* CONFIG_SYSCTL */

#if defined(CONFIG_SYSCTL) && defined(CONFIG_VE_IPTABLES)
static int nf_conntrack_init_ve_sysctl(struct ve_struct *ve)
{
	ve_nf_ct_net_table = nf_ct_net_table;
	ve_nf_ct_netfilter_table = nf_ct_netfilter_table;
	ve_nf_ct_sysctl_table = nf_ct_sysctl_table;

	if (!ve_is_super(ve)) {
		ve_nf_ct_net_table = clone_sysctl_template(nf_ct_net_table);
		if (ve_nf_ct_net_table == NULL)
			goto out;
	}

	ve_nf_ct_netfilter_table = ve_nf_ct_net_table[0].child;
	ve_nf_ct_netfilter_table[1].data = &ve_nf_conntrack_max;
	ve_nf_ct_sysctl_table = ve_nf_ct_netfilter_table[0].child;
	ve_nf_ct_sysctl_table[0].data = &ve_nf_conntrack_max;
	ve_nf_ct_sysctl_table[1].data = &ve_nf_conntrack_count;
	ve_nf_ct_sysctl_table[3].data = &ve_nf_conntrack_checksum;
	ve_nf_ct_sysctl_table[4].data = &ve_nf_ct_log_invalid;
	ve_nf_ct_sysctl_table[5].data = &ve_nf_ct_expect_max;

	ve_nf_ct_sysctl_header = register_sysctl_table(ve_nf_ct_net_table);
	if (!ve_nf_ct_sysctl_header)
		goto out_unclone;

	return 0;

out_unclone:
	if (!ve_is_super(ve))
		free_sysctl_clone(ve_nf_ct_net_table);
out:
	return -ENOMEM;
}

static void nf_conntrack_fini_ve_sysctl(struct ve_struct *ve)
{
	unregister_sysctl_table(ve_nf_ct_sysctl_header);
	if (!ve_is_super(ve))
		free_sysctl_clone(ve_nf_ct_net_table);
}
#else
static inline int nf_conntrack_init_ve_sysctl(struct ve_struct *ve)
{
	return 0;
}

static inline void nf_conntrack_fini_ve_sysctl(struct ve_struct *ve)
{
}
#endif

int nf_conntrack_init_ve(void)
{
	struct ve_struct *ve = get_exec_env();
	int err;

	err = nf_conntrack_init();
	if (err)
		goto out;

	if (ve_is_super(ve) && ip_conntrack_disable_ve0)
		return 0;

	ve_nf_conntrack_checksum = nf_conntrack_checksum;

	err = nf_conntrack_init_ve_sysctl(ve);
	if (err < 0)
		goto out_generic;

	err = nf_conntrack_init_ve_proc(ve);
	if (err < 0)
		goto out_sysctl;

	return 0;

out_sysctl:
	nf_conntrack_fini_ve_proc(ve);
out_generic:
	nf_conntrack_cleanup();
out:
	return err;
}

void nf_conntrack_cleanup_ve(void)
{
	struct ve_struct *ve = get_exec_env();

	if (ve_is_super(ve) && ip_conntrack_disable_ve0)
		goto cleanup;
	nf_conntrack_fini_ve_proc(ve);
	nf_conntrack_fini_ve_sysctl(ve);
cleanup:
	nf_conntrack_cleanup();
}
EXPORT_SYMBOL(nf_conntrack_cleanup_ve);

static int __init nf_conntrack_standalone_init(void)
{
#ifdef CONFIG_VE_IPTABLES
	KSYMRESOLVE(nf_conntrack_init_ve);
	KSYMRESOLVE(nf_conntrack_cleanup_ve);
	KSYMMODRESOLVE(nf_conntrack);
#endif
	return nf_conntrack_init_ve();
}

static void __exit nf_conntrack_standalone_fini(void)
{
#ifdef CONFIG_VE_IPTABLES
	KSYMMODUNRESOLVE(nf_conntrack);
	KSYMUNRESOLVE(nf_conntrack_init_ve);
	KSYMUNRESOLVE(nf_conntrack_cleanup_ve);
#endif
	nf_conntrack_cleanup_ve();
}

module_init(nf_conntrack_standalone_init);
module_exit(nf_conntrack_standalone_fini);

/* Some modules need us, but don't depend directly on any symbol.
   They should call this. */
void need_conntrack(void)
{
}
EXPORT_SYMBOL_GPL(need_conntrack);

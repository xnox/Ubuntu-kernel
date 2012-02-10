/*
 * This is the 1999 rewrite of IP Firewalling, aiming for kernel 2.3.x.
 *
 * Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 * Copyright (C) 2000-2004 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/nfcalls.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <net/ip.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Netfilter Core Team <coreteam@netfilter.org>");
MODULE_DESCRIPTION("iptables filter table");

#ifdef CONFIG_VE_IPTABLES
#include <linux/sched.h>
#define ve_packet_filter	(get_exec_env()->_ve_ipt_filter_pf)
#else
#define ve_packet_filter	&packet_filter
#endif

#define FILTER_VALID_HOOKS ((1 << NF_IP_LOCAL_IN) | (1 << NF_IP_FORWARD) | (1 << NF_IP_LOCAL_OUT))

static struct
{
	struct ipt_replace repl;
	struct ipt_standard entries[3];
	struct ipt_error term;
} initial_table = {
	.repl = {
		.name = "filter",
		.valid_hooks = FILTER_VALID_HOOKS,
		.num_entries = 4,
		.size = sizeof(struct ipt_standard) * 3 + sizeof(struct ipt_error),
		.hook_entry = {
			[NF_IP_LOCAL_IN] = 0,
			[NF_IP_FORWARD] = sizeof(struct ipt_standard),
			[NF_IP_LOCAL_OUT] = sizeof(struct ipt_standard) * 2,
		},
		.underflow = {
			[NF_IP_LOCAL_IN] = 0,
			[NF_IP_FORWARD] = sizeof(struct ipt_standard),
			[NF_IP_LOCAL_OUT] = sizeof(struct ipt_standard) * 2,
		},
	},
	.entries = {
		IPT_STANDARD_INIT(NF_ACCEPT),	/* LOCAL_IN */
		IPT_STANDARD_INIT(NF_ACCEPT),	/* FORWARD */
		IPT_STANDARD_INIT(NF_ACCEPT),	/* LOCAL_OUT */
	},
	.term = IPT_ERROR_INIT,			/* ERROR */
};

static struct xt_table packet_filter = {
	.name		= "filter",
	.valid_hooks	= FILTER_VALID_HOOKS,
	.lock		= RW_LOCK_UNLOCKED,
	.me		= THIS_MODULE,
	.af		= AF_INET,
};

/* The work comes in here from netfilter.c. */
static unsigned int
ipt_hook(unsigned int hook,
	 struct sk_buff *skb,
	 const struct net_device *in,
	 const struct net_device *out,
	 int (*okfn)(struct sk_buff *))
{
	return ipt_do_table(skb, hook, in, out, ve_packet_filter);
}

static unsigned int
ipt_local_out_hook(unsigned int hook,
		   struct sk_buff *skb,
		   const struct net_device *in,
		   const struct net_device *out,
		   int (*okfn)(struct sk_buff *))
{
	/* root is playing with raw sockets. */
	if (skb->len < sizeof(struct iphdr) ||
	    ip_hdrlen(skb) < sizeof(struct iphdr)) {
		if (net_ratelimit())
			printk("iptable_filter: ignoring short SOCK_RAW "
			       "packet.\n");
		return NF_ACCEPT;
	}

	return ipt_do_table(skb, hook, in, out, ve_packet_filter);
}

static struct nf_hook_ops ipt_ops[] = {
	{
		.hook		= ipt_hook,
		.owner		= THIS_MODULE,
		.pf		= PF_INET,
		.hooknum	= NF_IP_LOCAL_IN,
		.priority	= NF_IP_PRI_FILTER,
	},
	{
		.hook		= ipt_hook,
		.owner		= THIS_MODULE,
		.pf		= PF_INET,
		.hooknum	= NF_IP_FORWARD,
		.priority	= NF_IP_PRI_FILTER,
	},
	{
		.hook		= ipt_local_out_hook,
		.owner		= THIS_MODULE,
		.pf		= PF_INET,
		.hooknum	= NF_IP_LOCAL_OUT,
		.priority	= NF_IP_PRI_FILTER,
	},
};

/* Default to forward because I got too much mail already. */
static int forward = NF_ACCEPT;
module_param(forward, bool, 0000);

int init_iptable_filter(void)
{
	int ret;
	struct ipt_table *tmp_filter;

	/* Register table */
	tmp_filter = ipt_register_table(&packet_filter,
			&initial_table.repl);
	if (IS_ERR(tmp_filter))
		return PTR_ERR(tmp_filter);
#ifdef CONFIG_VE_IPTABLES
	ve_packet_filter = tmp_filter;
#endif

	/* Register hooks */
	ret = nf_register_hooks(ipt_ops, ARRAY_SIZE(ipt_ops));
	if (ret < 0)
		goto cleanup_table;

	return ret;

 cleanup_table:
	ipt_unregister_table(ve_packet_filter);
#ifdef CONFIG_VE_IPTABLES
	ve_packet_filter = NULL;
#endif
	return ret;
}

void fini_iptable_filter(void)
{
	nf_unregister_hooks(ipt_ops, ARRAY_SIZE(ipt_ops));
	ipt_unregister_table(ve_packet_filter);
#ifdef CONFIG_VE_IPTABLES
	ve_packet_filter = NULL;
#endif
}

static int __init iptable_filter_init(void)
{
	int err;

	if (forward < 0 || forward > NF_MAX_VERDICT) {
		printk("iptables forward must be 0 or 1\n");
		return -EINVAL;
	}

	/* Entry 1 is the FORWARD hook */
	initial_table.entries[1].target.verdict = -forward - 1;

	err = init_iptable_filter();
	if (err < 0)
		return err;

	KSYMRESOLVE(init_iptable_filter);
	KSYMRESOLVE(fini_iptable_filter);
	KSYMMODRESOLVE(iptable_filter);
	return 0;
}

static void __exit iptable_filter_fini(void)
{
	KSYMMODUNRESOLVE(iptable_filter);
	KSYMUNRESOLVE(init_iptable_filter);
	KSYMUNRESOLVE(fini_iptable_filter);
	fini_iptable_filter();
}

module_init(iptable_filter_init);
module_exit(iptable_filter_fini);

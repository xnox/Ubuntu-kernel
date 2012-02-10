/*
 *  include/linux/ve.h
 *
 *  Copyright (C) 2005  SWsoft
 *  All rights reserved.
 *  
 *  Licensing governed by "linux/COPYING.SWsoft" file.
 *
 */

#ifndef _LINUX_VE_H
#define _LINUX_VE_H

#include <linux/types.h>
#include <linux/capability.h>
#include <linux/sysctl.h>
#include <linux/net.h>
#include <linux/vzstat.h>
#include <linux/kobject.h>
#include <linux/pid.h>
#include <linux/socket.h>
#include <net/inet_frag.h>

#ifdef VZMON_DEBUG
#  define VZTRACE(fmt,args...) \
	printk(KERN_DEBUG fmt, ##args)
#else
#  define VZTRACE(fmt,args...)
#endif /* VZMON_DEBUG */

struct tty_driver;
struct devpts_config;
struct task_struct;
struct new_utsname;
struct file_system_type;
struct icmp_mib;
struct ip_mib;
struct tcp_mib;
struct udp_mib;
struct linux_mib;
struct fib_info;
struct fib_rule;
struct veip_struct;
struct ve_monitor;
struct nsproxy;
struct ve_sit_tunnels;

#if defined(CONFIG_VE) && defined(CONFIG_INET)
struct fib_table;
#ifdef CONFIG_VE_IPTABLES
struct xt_table;
struct nf_conn;

#define FRAG6Q_HASHSZ   64

struct ve_nf_conntrack {
	struct hlist_head		*_bysource;
	struct nf_nat_protocol		**_nf_nat_protos;
	int				_nf_nat_vmalloced;
	struct xt_table			*_nf_nat_table;
	struct nf_conntrack_l3proto	*_nf_nat_l3proto;
	atomic_t			_nf_conntrack_count;
	int				_nf_conntrack_max;
	struct hlist_head		*_nf_conntrack_hash;
	int				_nf_conntrack_checksum;
	int				_nf_conntrack_vmalloc;
	struct hlist_head		_unconfirmed;
	struct hlist_head		*_nf_ct_expect_hash;
	unsigned int			_nf_ct_expect_vmalloc;
	unsigned int			_nf_ct_expect_count;
	unsigned int			_nf_ct_expect_max;
	struct hlist_head		*_nf_ct_helper_hash;
	unsigned int			_nf_ct_helper_vmalloc;
	struct inet_frags		_nf_frags6;
	struct inet_frags_ctl		_nf_frags6_ctl;
#ifdef CONFIG_SYSCTL
	/* l4 stuff: */
	unsigned long			_nf_ct_icmp_timeout;
	unsigned long			_nf_ct_icmpv6_timeout;
	unsigned int			_nf_ct_udp_timeout;
	unsigned int			_nf_ct_udp_timeout_stream;
	unsigned int			_nf_ct_generic_timeout;
	unsigned int			_nf_ct_log_invalid;
	unsigned int			_nf_ct_tcp_timeout_max_retrans;
	int				_nf_ct_tcp_be_liberal;
	int				_nf_ct_tcp_loose;
	int				_nf_ct_tcp_max_retrans;
	unsigned int			_nf_ct_tcp_timeouts[10];
	struct ctl_table_header		*_icmp_sysctl_header;
	unsigned int			_tcp_sysctl_table_users;
	struct ctl_table_header		*_tcp_sysctl_header;
	unsigned int			_udp_sysctl_table_users;
	struct ctl_table_header		*_udp_sysctl_header;
	struct ctl_table_header		*_icmpv6_sysctl_header;
	struct ctl_table_header		*_generic_sysctl_header;
#ifdef CONFIG_NF_CONNTRACK_PROC_COMPAT
	struct ctl_table_header		*_icmp_compat_sysctl_header;
	struct ctl_table_header		*_tcp_compat_sysctl_header;
	struct ctl_table_header		*_udp_compat_sysctl_header;
	struct ctl_table_header		*_generic_compat_sysctl_header;
#endif
	/* l4 protocols sysctl tables: */
	struct nf_conntrack_l4proto	*_nf_conntrack_l4proto_icmp;
	struct nf_conntrack_l4proto	*_nf_conntrack_l4proto_tcp4;
	struct nf_conntrack_l4proto	*_nf_conntrack_l4proto_icmpv6;
	struct nf_conntrack_l4proto	*_nf_conntrack_l4proto_tcp6;
	struct nf_conntrack_l4proto	*_nf_conntrack_l4proto_udp4;
	struct nf_conntrack_l4proto	*_nf_conntrack_l4proto_udp6;
	struct nf_conntrack_l4proto	*_nf_conntrack_l4proto_generic;
	struct nf_conntrack_l4proto	**_nf_ct_protos[PF_MAX];
	/* l3 protocols sysctl tables: */
	struct nf_conntrack_l3proto	*_nf_conntrack_l3proto_ipv4;
	struct nf_conntrack_l3proto	*_nf_conntrack_l3proto_ipv6;
	struct nf_conntrack_l3proto	*_nf_ct_l3protos[AF_MAX];
	/* sysctl standalone stuff: */
	struct ctl_table_header		*_nf_ct_sysctl_header;
	ctl_table			*_nf_ct_sysctl_table;
	ctl_table			*_nf_ct_netfilter_table;
	ctl_table			*_nf_ct_net_table;
	ctl_table			*_ip_ct_net_table;
	ctl_table			*_ip_ct_netfilter_table;
	struct ctl_table_header		*_ip_ct_sysctl_header;
	int				_nf_ct_log_invalid_proto_min;
	int				_nf_ct_log_invalid_proto_max;
#endif /* CONFIG_SYSCTL */
};
#endif
#endif

struct ve_cpu_stats {
	cycles_t	idle_time;
	cycles_t	iowait_time;
	cycles_t	strt_idle_time;
	cycles_t	used_time;
	seqcount_t	stat_lock;
	int		nr_running;
	int		nr_unint;
	int		nr_iowait;
	cputime64_t	user;
	cputime64_t	nice;
	cputime64_t	system;
} ____cacheline_aligned;

struct ve_ipt_recent;
struct ve_xt_hashlimit;

struct ve_struct {
	struct list_head	ve_list;

	envid_t			veid;
	struct list_head	vetask_lh;
	/* capability bounding set */
	kernel_cap_t		ve_cap_bset;
	atomic_t		pcounter;
	/* ref counter to ve from ipc */
	atomic_t		counter;
	unsigned int		class_id;
	struct rw_semaphore	op_sem;
	int			is_running;
	int			is_locked;
	atomic_t		suspend;
	/* see vzcalluser.h for VE_FEATURE_XXX definitions */
	__u64			features;

/* VE's root */
	struct vfsmount 	*fs_rootmnt;
	struct dentry 		*fs_root;

/* sysctl */
	struct list_head	sysctl_lh;
	struct ctl_table_header *uts_header;
	struct file_system_type *proc_fstype;
	struct vfsmount		*proc_mnt;
	struct proc_dir_entry	*proc_root;
	struct proc_dir_entry	*proc_sys_root;
	struct proc_dir_entry	*_proc_net;
	struct proc_dir_entry	*_proc_net_stat;
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	struct proc_dir_entry	*_proc_net_devsnmp6;
#endif

/* BSD pty's */
#ifdef CONFIG_LEGACY_PTYS
	struct tty_driver       *pty_driver;
	struct tty_driver       *pty_slave_driver;
#endif
#ifdef CONFIG_UNIX98_PTYS
	struct tty_driver	*ptm_driver;
	struct tty_driver	*pts_driver;
	struct idr		*allocated_ptys;
	struct file_system_type *devpts_fstype;
	struct vfsmount		*devpts_mnt;
	struct dentry		*devpts_root;
	struct devpts_config	*devpts_config;
#endif

	struct ve_nfs_context	*nfs_context;

	struct file_system_type *shmem_fstype;
	struct vfsmount		*shmem_mnt;
#ifdef CONFIG_SYSFS
	struct file_system_type *sysfs_fstype;
	struct vfsmount		*sysfs_mnt;
	struct super_block	*sysfs_sb;
	struct sysfs_dirent	*_sysfs_root;
#endif
#ifndef CONFIG_SYSFS_DEPRECATED
	struct kobject		*_virtual_dir;
#endif
	struct kset		*class_subsys;
	struct kset		*class_obj_subsys;
	struct kset		*devices_subsys;
	struct class		*tty_class;
	struct class		*mem_class;

#ifdef CONFIG_NET
	struct class		*net_class;
#ifdef CONFIG_INET
	struct ipv4_devconf	*_ipv4_devconf;
	struct ipv4_devconf	*_ipv4_devconf_dflt;
	struct ctl_table_header	*forward_header;
	struct ctl_table	*forward_table;
 	unsigned long		rt_flush_required;
	struct neigh_table	*ve_arp_tbl;
	struct ve_sit_tunnels	*_sit_tunnels;
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	struct ipv6_devconf	*_ipv6_devconf;
	struct ipv6_devconf	*_ipv6_devconf_dflt;
	struct neigh_table	*ve_nd_tbl;
#endif
#endif
#endif
#if defined(CONFIG_VE_NETDEV) || defined (CONFIG_VE_NETDEV_MODULE)
	struct veip_struct	*veip;
	struct net_device	*_venet_dev;
#endif

/* per VE CPU stats*/
	struct timespec		start_timespec;
	u64			start_jiffies;	/* Deprecated */
	cycles_t 		start_cycles;
	unsigned long		avenrun[3];	/* loadavg data */

	cycles_t 		cpu_used_ve;
	struct kstat_lat_pcpu_struct	sched_lat_ve;

#ifdef CONFIG_INET
	struct hlist_head	*_fib_info_hash;
	struct hlist_head	*_fib_info_laddrhash;
	int			_fib_hash_size;
	int			_fib_info_cnt;

	struct fib_rule		*_local_rule;
	struct list_head	_fib_rules;
	/* XXX: why a magic constant? */
#ifdef CONFIG_IP_MULTIPLE_TABLES
	struct hlist_head 	_fib_table_hash[256];
#else
	struct hlist_head 	_fib_table_hash[1];
	struct fib_table	*_main_table;
	struct fib_table	*_local_table;
#endif
	struct icmp_mib		*_icmp_statistics[2];
	struct icmpmsg_mib	*_icmpmsg_statistics[2];
	struct ipstats_mib	*_ip_statistics[2];
	struct tcp_mib		*_tcp_statistics[2];
	struct udp_mib		*_udp_statistics[2];
	struct udp_mib		*_udplite_statistics[2];
	struct linux_mib	*_net_statistics[2];
	struct venet_stat       *stat;
#ifdef CONFIG_VE_IPTABLES
/* core/netfilter.c virtualization */
	void			*_nf_hooks;
	struct xt_table		*_ve_ipt_filter_pf; /* packet_filter struct */
	struct xt_table		*_ve_ip6t_filter_pf;
	struct xt_table		*_ipt_mangle_table;
	struct xt_table		*_ip6t_mangle_table;
	struct list_head	_xt_tables[NPROTO];

	__u64			_iptables_modules;
	struct ve_nf_conntrack	*_nf_conntrack;
	struct ve_ipt_recent	*_ipt_recent;
	struct ve_xt_hashlimit	*_xt_hashlimit;
#endif /* CONFIG_VE_IPTABLES */

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
#ifdef CONFIG_IPV6_MULTIPLE_TABLES
	struct hlist_head	_fib6_table_hash[256];
	struct fib6_table	*_fib6_local_table;
#else
	struct hlist_head	_fib6_table_hash[1];
#endif
	struct fib6_table	*_fib6_table;
	struct ipstats_mib	*_ipv6_statistics[2];
	struct icmpv6_mib	*_icmpv6_statistics[2];
	struct icmpv6msg_mib	*_icmpv6msg_statistics[2];
	struct udp_mib		*_udp_stats_in6[2];
	struct udp_mib		*_udplite_stats_in6[2];
#endif
#endif
	wait_queue_head_t	*_log_wait;
	unsigned long		*_log_start;
	unsigned long		*_log_end;
	unsigned long		*_logged_chars;
	char			*log_buf;
#define VE_DEFAULT_LOG_BUF_LEN	4096

	struct ve_cpu_stats	*cpu_stats;
	unsigned long		down_at;
	struct list_head	cleanup_list;
#if defined(CONFIG_FUSE_FS) || defined(CONFIG_FUSE_FS_MODULE)
	struct list_head	_fuse_conn_list;
	struct super_block	*_fuse_control_sb;

	struct file_system_type	*fuse_fs_type;
	struct file_system_type	*fuse_ctl_fs_type;
#endif
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
	struct proc_dir_entry	*_proc_vlan_dir;
	struct proc_dir_entry	*_proc_vlan_conf;
#endif
	unsigned long		jiffies_fixup;
	unsigned char		disable_net;
	struct ve_monitor	*monitor;
	struct proc_dir_entry	*monitor_proc;
	unsigned long		meminfo_val;

#if defined(CONFIG_NFS_FS) || defined(CONFIG_NFS_FS_MODULE) \
	|| defined(CONFIG_NFSD) || defined(CONFIG_NFSD_MODULE)
	unsigned int		_nlmsvc_users;
	pid_t			_nlmsvc_pid;
	int			_nlmsvc_grace_period;
	unsigned long		_nlmsvc_timeout;
#endif

#if defined(CONFIG_BINFMT_MISC) || defined(CONFIG_BINFMT_MISC_MODULE)
	struct file_system_type	*bm_fs_type;
	struct vfsmount		*bm_mnt;
	int			bm_enabled;
	int			bm_entry_count;
	struct list_head	bm_entries;
#endif

	struct nsproxy		*ve_ns;
	struct net		*ve_netns;
#ifdef CONFIG_GRKERNSEC
	struct {
		int		lock;
#ifdef CONFIG_GRKERNSEC_TPE
		int		enable_tpe;
		int		tpe_gid;
#ifdef CONFIG_GRKERNSEC_TPE_ALL
		int		enable_tpe_all;
#endif
#endif /*CONFIG_GRKERNSEC_TPE */
	} grsec;
#endif /* CONFIG_GRKERNSEC */
};

#define VE_CPU_STATS(ve, cpu)	(per_cpu_ptr((ve)->cpu_stats, cpu))

extern int nr_ve;

#ifdef CONFIG_VE

void do_update_load_avg_ve(void);
void do_env_free(struct ve_struct *ptr);

static inline struct ve_struct *get_ve(struct ve_struct *ptr)
{
	if (ptr != NULL)
		atomic_inc(&ptr->counter);
	return ptr;
}

static inline void put_ve(struct ve_struct *ptr)
{
	if (ptr && atomic_dec_and_test(&ptr->counter)) {
		if (atomic_read(&ptr->pcounter) > 0)
			BUG();
		if (ptr->is_running)
			BUG();
		do_env_free(ptr);
	}
}

static inline void pget_ve(struct ve_struct *ptr)
{
	atomic_inc(&ptr->pcounter);
}

void ve_cleanup_schedule(struct ve_struct *);
static inline void pput_ve(struct ve_struct *ptr)
{
	if (unlikely(atomic_dec_and_test(&ptr->pcounter)))
		ve_cleanup_schedule(ptr);
}

extern spinlock_t ve_cleanup_lock;
extern struct list_head ve_cleanup_list;
extern struct task_struct *ve_cleanup_thread;

extern unsigned long long ve_relative_clock(struct timespec * ts);

#ifdef CONFIG_FAIRSCHED
#define ve_cpu_online_map(ve, mask) fairsched_cpu_online_map(ve->veid, mask)
#else
#define ve_cpu_online_map(ve, mask) do { *(mask) = cpu_online_map; } while (0)
#endif
#else	/* CONFIG_VE */
#define ve_utsname	system_utsname
#define get_ve(ve)	(NULL)
#define put_ve(ve)	do { } while (0)
#define pget_ve(ve)	do { } while (0)
#define pput_ve(ve)	do { } while (0)
#endif	/* CONFIG_VE */

#endif /* _LINUX_VE_H */

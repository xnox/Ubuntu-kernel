#ifndef LINUX_26_COMPAT_H
#define LINUX_26_COMPAT_H

#include <linux/autoconf.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/netpoll.h>
#include <linux/rtnetlink.h>
#include <linux/audit.h>
#include <linux/workqueue.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/genetlink.h>
#include <linux/scatterlist.h>
#include <linux/usb.h>
#include <linux/hw_random.h>
#include <linux/leds.h>
#include <linux/pm_qos_params.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/rfkill.h>

#include <net/arp.h>
#include <net/neighbour.h>
#include <net/pkt_sched.h>

#include <linux/compat_autoconf.h>

#include <asm/io.h>

/* Compat work for 2.6.21 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22))

/* reuse ax25_ptr */
#define ieee80211_ptr ax25_ptr

#ifdef CONFIG_AX25
#error Compat reuses the AX.25 pointer so that may not be enabled!
#endif

static inline unsigned char *skb_mac_header(const struct sk_buff *skb)
{
	return skb->mac.raw;
}

static inline void skb_set_mac_header(struct sk_buff *skb, int offset)
{
	skb->mac.raw = skb->data + offset;
}

static inline void skb_reset_mac_header(struct sk_buff *skb)
{
	skb->mac.raw = skb->data;
}

static inline void skb_reset_network_header(struct sk_buff *skb)
{
	skb->nh.raw = skb->data;
}

static inline void skb_set_network_header(struct sk_buff *skb, int offset)
{
	skb->nh.raw = skb->data + offset;
}

static inline void skb_set_transport_header(struct sk_buff *skb, int offset)
{
	skb->h.raw = skb->data + offset;
}

static inline unsigned char *skb_transport_header(struct sk_buff *skb)
{
	return skb->h.raw;
}

static inline unsigned char *skb_network_header(const struct sk_buff *skb)
{
	return skb->nh.raw;
}

static inline unsigned char *skb_tail_pointer(const struct sk_buff *skb)
{
	return skb->tail;
}

static inline struct iphdr *ip_hdr(const struct sk_buff *skb)
{
	return (struct iphdr *)skb_network_header(skb);
}

static inline void skb_copy_from_linear_data(const struct sk_buff *skb,
					     void *to,
					     const unsigned int len)
{
	memcpy(to, skb->data, len);
}

static inline void skb_copy_from_linear_data_offset(const struct sk_buff *skb,
						    const int offset, void *to,
						    const unsigned int len)
{
	memcpy(to, skb->data + offset, len);
}

#define __maybe_unused	__attribute__((unused))

#define uninitialized_var(x) x = x

/* This will lead to very weird behaviour... */
#define NLA_BINARY NLA_STRING

static inline int pci_set_mwi(struct pci_dev *dev)
{
	return -ENOSYS;
}

static inline void pci_clear_mwi(struct pci_dev *dev)
{
}

#define list_first_entry(ptr, type, member) \
        list_entry((ptr)->next, type, member)

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)) */

/* Compat work for < 2.6.23 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))

/*
 * Tell gcc if a function is cold. The compiler will assume any path
 * directly leading to the call is unlikely.
 */

#if !(__GNUC__ == 4 && __GNUC_MINOR__ < 3)
/* Mark functions as cold. gcc will assume any path leading to a call
 * to them will be unlikely.  This means a lot of manual unlikely()s
 * are unnecessary now for any paths leading to the usual suspects
 * like BUG(), printk(), panic() etc. [but let's keep them for now for
 * older compilers]
 *
 * Early snapshots of gcc 4.3 don't support this and we can't detect this
 * in the preprocessor, but we can live with this because they're unreleased.
 * Maketime probing would be overkill here.
 *
 * gcc also has a __attribute__((__hot__)) to move hot functions into
 * a special section, but I don't see any sense in this right now in
 * the kernel context */
#define __cold                  __attribute__((__cold__))
#endif /* gcc 4.3 check */

#ifndef __cold
#define __cold
#endif

/* Added as of 2.6.23 in include/linux/netdevice.h */
#define alloc_netdev_mq(sizeof_priv, name, setup, queue) \
	alloc_netdev(sizeof_priv, name, setup)
#define NETIF_F_MULTI_QUEUE 16384

/* Added as of 2.6.23 on include/linux/netdevice.h */
static inline int netif_is_multiqueue(const struct net_device *dev)
{
	return (!!(NETIF_F_MULTI_QUEUE & dev->features));
}

/* 2.6.23 fixed a bug in tcf_destroy_chain and the parameter changed */
static inline void tcf_destroy_chain_compat(struct tcf_proto **fl)
{
	struct tcf_proto *tp;

	while ((tp = *fl) != NULL) {
		*fl = tp->next;
		tp->ops->destroy(tp);
		module_put(tp->ops->owner);
		kfree(tp);
	}
}

#else

#define tcf_destroy_chain_compat tcf_destroy_chain

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)) */

/* Compat work for 2.6.21, 2.6.22 and 2.6.23 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))

#if (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,23)) /* Local check */
/* Added as of 2.6.24 in include/linux/skbuff.h.
 *
 * Although 2.6.23 does support for CONFIG_NETDEVICES_MULTIQUEUE
 * this helper was not added until 2.6.24. This implementation
 * is exactly as it is on newer kernels.
 *
 * For older kernels we use the an internal mac80211 hack.
 * For details see changes to include/net/mac80211.h through
 * compat.diff and compat/mq_compat.h */
static inline u16 skb_get_queue_mapping(struct sk_buff *skb)
{
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	return skb->queue_mapping;
#else
	return 0;
#endif
}
#endif /* Local 2.6.23 check */

/* On older kernels we handle this a bit differently, so we yield to that
 * code for its implementation in mq_compat.h as we want to make
 * use of the internal mac80211 __ieee80211_queue_stopped() which itself
 * uses internal mac80211 data structure hacks. */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)) /* Local check */
/**
 * netif_subqueue_stopped - test status of subqueue
 * @dev: network device
 * @queue_index: sub queue index
 *
 * Check individual transmit queue of a device with multiple transmit queues.
 */
static inline int __netif_subqueue_stopped(const struct net_device *dev,
					u16 queue_index)
{
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	return test_bit(__LINK_STATE_XOFF,
	&dev->egress_subqueue[queue_index].state);
#else
	return 0;
#endif
}

/* Note: although the backport implementation for netif_subqueue_stopped
 * on older kernels is identical to upstream __netif_subqueue_stopped()
 * (except for a const qualifier) we implement netif_subqueue_stopped()
 * as part of mac80211 as it relies on internal mac80211 structures we
 * use for MQ support. We this implement it in mq_compat.h */

#endif /* Local 2.6.23 check */

/*
 * Force link bug if constructor is used, can't be done compatibly
 * because constructor arguments were swapped since then!
 */
extern void __incompatible_kmem_cache_create(void);

/* 2.6.21 and 2.6.22 kmem_cache_create() takes 6 arguments */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
#define kmem_cache_create(name, objsize, align, flags, ctor) 	\
	({							\
		if (ctor) __incompatible_kmem_cache_create();	\
		kmem_cache_create((name), (objsize), (align),	\
				  (flags), NULL, NULL);		\
	})
#endif

/* 2.6.23 kmem_cache_create() takes 5 arguments */
#if (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,23))
#define kmem_cache_create(name, objsize, align, flags, ctor) 	\
	({							\
		if (ctor) __incompatible_kmem_cache_create();	\
		kmem_cache_create((name), (objsize), (align),	\
				  (flags), NULL);		\
	})
#endif

/* From include/linux/mod_devicetable.h */

/* SSB core, see drivers/ssb/ */
#ifndef SSB_DEVICE
struct ssb_device_id {
	__u16   vendor;
	__u16   coreid;
	__u8    revision;
};
#define SSB_DEVICE(_vendor, _coreid, _revision)  \
	{ .vendor = _vendor, .coreid = _coreid, .revision = _revision, }
#define SSB_DEVTABLE_END  \
	{ 0, },

#define SSB_ANY_VENDOR          0xFFFF
#define SSB_ANY_ID              0xFFFF
#define SSB_ANY_REV             0xFF
#endif


/* Namespace stuff, introduced on 2.6.24 */
#define dev_get_by_index(a, b)		dev_get_by_index(b)
#define __dev_get_by_index(a, b)	__dev_get_by_index(b)

/*
 * Display a 6 byte device address (MAC) in a readable format.
 */
#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
extern char *print_mac(char *buf, const u8 *addr);
#define DECLARE_MAC_BUF(var) char var[18] __maybe_unused

extern int		eth_header(struct sk_buff *skb, struct net_device *dev,
				unsigned short type, void *daddr,
				void *saddr, unsigned len);
extern int		eth_rebuild_header(struct sk_buff *skb);
extern void		eth_header_cache_update(struct hh_cache *hh, struct net_device *dev,
				unsigned char * haddr);
extern int		eth_header_cache(struct neighbour *neigh,
			struct hh_cache *hh);

/* This structure is simply not present on 2.6.22 and 2.6.23 */
struct header_ops {
	int     (*create) (struct sk_buff *skb, struct net_device *dev,
		unsigned short type, void *daddr,
		void *saddr, unsigned len);
	int     (*parse)(const struct sk_buff *skb, unsigned char *haddr);
	int     (*rebuild)(struct sk_buff *skb);
	#define HAVE_HEADER_CACHE
	int     (*cache)(struct neighbour *neigh, struct hh_cache *hh);
	void    (*cache_update)(struct hh_cache *hh,
		struct net_device *dev,
		unsigned char *haddr);
};

/* net/ieee80211/ieee80211_crypt_tkip uses sg_init_table. This was added on 
 * 2.6.24. CONFIG_DEBUG_SG was added in 2.6.24 as well, so lets just ignore
 * the debug stuff. Note that adding this required changes to the struct
 * scatterlist on include/asm/scatterlist*, so the right way to port this
 * is to simply ignore the new structure changes and zero the scatterlist
 * array. We lave the kdoc intact for reference.
 */

/**
 * sg_mark_end - Mark the end of the scatterlist
 * @sg:          SG entryScatterlist
 *
 * Description:
 *   Marks the passed in sg entry as the termination point for the sg
 *   table. A call to sg_next() on this entry will return NULL.
 *
 **/
static inline void sg_mark_end(struct scatterlist *sg)
{
}

/**
 * sg_init_table - Initialize SG table
 * @sgl:           The SG table
 * @nents:         Number of entries in table
 *
 * Notes:
 *   If this is part of a chained sg table, sg_mark_end() should be
 *   used only on the last table part.
 *
 **/
static inline void sg_init_table(struct scatterlist *sgl, unsigned int nents)
{
	memset(sgl, 0, sizeof(*sgl) * nents);
}

/**
 * usb_endpoint_num - get the endpoint's number
 * @epd: endpoint to be checked
 *
 * Returns @epd's number: 0 to 15.
 */
static inline int usb_endpoint_num(const struct usb_endpoint_descriptor *epd)
{
	return epd->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
}

/* Helper to make struct pci_dev is_pcie compatibility code smaller */
int compat_is_pcie(struct pci_dev *pdev);

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)) */

/* Compat work for kernels <= 2.6.22 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))

/* dev_mc_list was replaced with dev_addr_list as of 2.6.23,
 * only new member added is da_synced. */
#define dev_addr_list	dev_mc_list
#define da_addr		dmi_addr
#define da_addrlen	dmi_addrlen
#define da_users	dmi_users
#define da_gusers	dmi_gusers

/* dev_set_promiscuity() was moved to __dev_set_promiscuity() on 2.6.23 and 
 * dev_set_promiscuity() became a wrapper. */
#define __dev_set_promiscuity dev_set_promiscuity

/* Our own 2.6.22 port on compat.c */
extern void	dev_mc_unsync(struct net_device *to, struct net_device *from);
extern int	dev_mc_sync(struct net_device *to, struct net_device *from);

/* Our own 2.6.22 port on compat.c */
extern void	__dev_set_rx_mode(struct net_device *dev);

/* Simple to add this */
extern int cancel_delayed_work_sync(struct delayed_work *work);

#define cancel_delayed_work_sync cancel_rearming_delayed_work

#define debugfs_rename(a, b, c, d) 1

/* nl80211 requires multicast group support which is new and added on
 * 2.6.23. We can't add support for it for older kernels to support it
 * genl_family structure was changed. Lets just let through the
 * genl_register_mc_group call. This means no multicast group suppport */

#define genl_register_mc_group(a, b) 0

/**
 * struct genl_multicast_group - generic netlink multicast group
 * @name: name of the multicast group, names are per-family
 * @id: multicast group ID, assigned by the core, to use with
 * 	genlmsg_multicast().
 * @list: list entry for linking
 * @family: pointer to family, need not be set before registering
 */
struct genl_multicast_group
{
	struct genl_family      *family;        /* private */
	struct list_head        list;           /* private */
	char                    name[GENL_NAMSIZ];
	u32                     id;
};


/* Added as of 2.6.23 */
int pci_try_set_mwi(struct pci_dev *dev);

/* Added as of 2.6.23 */
#ifdef CONFIG_PM_SLEEP
/*
 * Tell the freezer that the current task should be frozen by it
 */
static inline void set_freezable(void)
{
	current->flags &= ~PF_NOFREEZE;
}

#else
static inline void set_freezable(void) {}
#endif /* CONFIG_PM_SLEEP */

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)) */





#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
/* Added on 2.6.24 in include/linux/types.h by Al viro on commit 142956af */
typedef unsigned long               uintptr_t;
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)) */




/* Compat work for 2.6.24 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25))

/* The patch:
 * commit 8b5f6883683c91ad7e1af32b7ceeb604d68e2865
 * Author: Marcin Slusarz <marcin.slusarz@gmail.com>
 * Date:   Fri Feb 8 04:20:12 2008 -0800
 *
 *     byteorder: move le32_add_cpu & friends from OCFS2 to core
 *
 * moves le*_add_cpu and be*_add_cpu functions from OCFS2 to core
 * header (1st) and converted some existing code to it. We port
 * it here as later kernels will most likely use it.
 */
static inline void le16_add_cpu(__le16 *var, u16 val)
{
	*var = cpu_to_le16(le16_to_cpu(*var) + val);
}

static inline void le32_add_cpu(__le32 *var, u32 val)
{
	*var = cpu_to_le32(le32_to_cpu(*var) + val);
}

static inline void le64_add_cpu(__le64 *var, u64 val)
{
	*var = cpu_to_le64(le64_to_cpu(*var) + val);
}

static inline void be16_add_cpu(__be16 *var, u16 val)
{
	u16 v = be16_to_cpu(*var);
	*var = cpu_to_be16(v + val);
}

static inline void be32_add_cpu(__be32 *var, u32 val)
{
	u32 v = be32_to_cpu(*var);
	*var = cpu_to_be32(v + val);
}

static inline void be64_add_cpu(__be64 *var, u64 val)
{
	u64 v = be64_to_cpu(*var);
	*var = cpu_to_be64(v + val);
}

/* 2.6.25 changes hwrng_unregister()'s behaviour by supporting 
 * suspend of its parent device (the misc device, which is itself the
 * hardware random number generator). It does this by passing a parameter to
 * unregister_miscdev() which is not supported in older kernels. The suspend
 * parameter allows us to enable access to the device's hardware
 * number generator during suspend. As far as wireless is concerned this means
 * if a driver goes to suspend it you won't have the HNR available in
 * older kernels. */
static inline void __hwrng_unregister(struct hwrng *rng, bool suspended)
{
	hwrng_unregister(rng);
}

static inline void led_classdev_unregister_suspended(struct led_classdev *lcd)
{
	led_classdev_unregister(lcd);
}

/**
 * The following things are out of ./include/linux/kernel.h
 * The new iwlwifi driver is using them.
 */
extern int strict_strtoul(const char *, unsigned int, unsigned long *);
extern int strict_strtol(const char *, unsigned int, long *);

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)) */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))

/* from include/linux/device.h */
/* device_create_drvdata() is new */
extern struct device *device_create_drvdata(struct class *cls,
	struct device *parent,
	dev_t devt,
	void *drvdata,
	const char *fmt, ...)
__attribute__((format(printf, 5, 6)));

/* This is from include/linux/list.h */

/**
 * list_is_singular - tests whether a list has just one entry.
 * @head: the list to test.
 */
static inline int list_is_singular(const struct list_head *head)
{
	return !list_empty(head) && (head->next == head->prev);
}

/* This is from include/linux/device.h, which was added as of 2.6.26 */
static inline const char *dev_name(struct device *dev)
{
	/* will be changed into kobject_name(&dev->kobj) in the near future */
	return dev->bus_id;
}

/* This is from include/linux/kernel.h, which was added as of 2.6.26 */ 

/**
 * clamp_val - return a value clamped to a given range using val's type
 * @val: current value
 * @min: minimum allowable value
 * @max: maximum allowable value
 * 
 * This macro does no typechecking and uses temporary variables of whatever
 * type the input argument 'val' is.  This is useful when val is an unsigned
 * type and min and max are literals that will otherwise be assigned a signed
 * integer type.
 */ 

#define clamp_val(val, min, max) ({             \
	typeof(val) __val = (val);              \
	typeof(val) __min = (min);              \
	typeof(val) __max = (max);              \
	__val = __val < __min ? __min: __val;   \
	__val > __max ? __max: __val; })


/* 2.6.26 added its own unaligned API which the 
 * new drivers can use. Lets port it here by including it in older
 * kernels and also deal with the architecture handling here. */

#ifdef CONFIG_ALPHA

#include <linux/unaligned/be_struct.h>
#include <linux/unaligned/le_byteshift.h>
#include <linux/unaligned/generic.h>

#endif /* alpha */
#ifdef CONFIG_ARM

/* arm */
#include <linux/unaligned/le_byteshift.h>
#include <linux/unaligned/be_byteshift.h>
#include <linux/unaligned/generic.h>

#endif /* arm */
#ifdef CONFIG_AVR32

/*
 * AVR32 can handle some unaligned accesses, depending on the
 * implementation.  The AVR32 AP implementation can handle unaligned
 * words, but halfwords must be halfword-aligned, and doublewords must
 * be word-aligned.
 * 
 * However, swapped word loads must be word-aligned so we can't
 * optimize word loads in general.
 */ 

#include <linux/unaligned/be_struct.h>
#include <linux/unaligned/le_byteshift.h>
#include <linux/unaligned/generic.h>

#endif
#ifdef CONFIG_BLACKFIN

#include <linux/unaligned/le_struct.h>
#include <linux/unaligned/be_byteshift.h>
#include <linux/unaligned/generic.h>

#endif /* blackfin */
#ifdef CONFIG_CRIS

/*
 * CRIS can do unaligned accesses itself. 
 */
#include <linux/unaligned/access_ok.h>
#include <linux/unaligned/generic.h>

#endif /* cris */
#ifdef CONFIG_FRV

#include <linux/unaligned/le_byteshift.h>
#include <linux/unaligned/be_byteshift.h>
#include <linux/unaligned/generic.h>

#endif /* frv */
#ifdef CONFIG_H8300

#include <linux/unaligned/be_memmove.h>
#include <linux/unaligned/le_byteshift.h>
#include <linux/unaligned/generic.h>

#endif /* h8300 */
#ifdef  CONFIG_IA64

#include <linux/unaligned/le_struct.h>
#include <linux/unaligned/be_byteshift.h>
#include <linux/unaligned/generic.h>

#endif /* ia64 */
#ifdef CONFIG_M32R

#if defined(__LITTLE_ENDIAN__)
# include <linux/unaligned/le_memmove.h>
# include <linux/unaligned/be_byteshift.h>
# include <linux/unaligned/generic.h>
#else
# include <linux/unaligned/be_memmove.h>
# include <linux/unaligned/le_byteshift.h>
# include <linux/unaligned/generic.h>
#endif

#endif /* m32r */
#ifdef CONFIG_M68K /* this handles both m68k and m68knommu */

#ifdef CONFIG_COLDFIRE
#include <linux/unaligned/be_struct.h>
#include <linux/unaligned/le_byteshift.h>
#include <linux/unaligned/generic.h>
#else

/*
 * The m68k can do unaligned accesses itself.
 */
#include <linux/unaligned/access_ok.h>
#include <linux/unaligned/generic.h>
#endif

#endif /* m68k and m68knommu */
#ifdef CONFIG_MIPS

#if defined(__MIPSEB__)
# include <linux/unaligned/be_struct.h>
# include <linux/unaligned/le_byteshift.h>
# include <linux/unaligned/generic.h>
# define get_unaligned  __get_unaligned_be
# define put_unaligned  __put_unaligned_be
#elif defined(__MIPSEL__)
# include <linux/unaligned/le_struct.h>
# include <linux/unaligned/be_byteshift.h>
# include <linux/unaligned/generic.h>
#endif

#endif /* mips */
#ifdef CONFIG_MN10300

#include <linux/unaligned/access_ok.h>
#include <linux/unaligned/generic.h>

#endif /* mn10300 */
#ifdef CONFIG_PARISC

#include <linux/unaligned/be_struct.h>
#include <linux/unaligned/le_byteshift.h>
#include <linux/unaligned/generic.h>

#endif /* parisc */
#ifdef CONFIG_PPC
/*
 * The PowerPC can do unaligned accesses itself in big endian mode.
 */ 
#include <linux/unaligned/access_ok.h>
#include <linux/unaligned/generic.h>

#endif /* ppc */
#ifdef CONFIG_S390

/*
 * The S390 can do unaligned accesses itself. 
 */
#include <linux/unaligned/access_ok.h>
#include <linux/unaligned/generic.h>

#endif /* s390 */
#ifdef CONFIG_SUPERH

/* SH can't handle unaligned accesses. */
#ifdef __LITTLE_ENDIAN__
# include <linux/unaligned/le_struct.h>
# include <linux/unaligned/be_byteshift.h>
# include <linux/unaligned/generic.h>
#else
# include <linux/unaligned/be_struct.h>
# include <linux/unaligned/le_byteshift.h>
# include <linux/unaligned/generic.h>
#endif

#endif /* sh - SUPERH */
#ifdef CONFIG_SPARC

/* sparc and sparc64 */
#include <linux/unaligned/be_struct.h>
#include <linux/unaligned/le_byteshift.h>
#include <linux/unaligned/generic.h>

#endif  /* sparc */
#ifdef CONFIG_UML

#include "asm/arch/unaligned.h"

#endif /* um - uml */
#ifdef CONFIG_V850

#include <linux/unaligned/be_byteshift.h>
#include <linux/unaligned/le_byteshift.h>
#include <linux/unaligned/generic.h>

#endif /* v850 */
#ifdef CONFIG_X86
/*
 * The x86 can do unaligned accesses itself.
 */
#include <linux/unaligned/access_ok.h>
#include <linux/unaligned/generic.h>

#endif /* x86 */
#ifdef CONFIG_XTENSA

#ifdef __XTENSA_EL__
# include <linux/unaligned/le_memmove.h>
# include <linux/unaligned/be_byteshift.h>
# include <linux/unaligned/generic.h>
#elif defined(__XTENSA_EB__)
# include <linux/unaligned/be_memmove.h>
# include <linux/unaligned/le_byteshift.h>
# include <linux/unaligned/generic.h>
#else
# error processor byte order undefined!
#endif

#endif /* xtensa */

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)) */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))

/* This is from include/linux/rfkill.h */
#define RFKILL_STATE_SOFT_BLOCKED	RFKILL_STATE_OFF
#define RFKILL_STATE_UNBLOCKED		RFKILL_STATE_ON
/* This one is new */
#define RFKILL_STATE_HARD_BLOCKED	2

int rfkill_force_state(struct rfkill *rfkill, enum rfkill_state state);

int register_rfkill_notifier(struct notifier_block *nb);
int unregister_rfkill_notifier(struct notifier_block *nb);

/* This is from include/linux/ieee80211.h */
#define IEEE80211_HT_CAP_DSSSCCK40		0x1000

/* New link list changes added as of 2.6.27, needed for ath9k */

static inline void __list_cut_position(struct list_head *list,
		struct list_head *head, struct list_head *entry)
{
	struct list_head *new_first = entry->next;
	list->next = head->next;
	list->next->prev = list;
	list->prev = entry;
	entry->next = list;
	head->next = new_first;
	new_first->prev = head;
}

/**
 * list_cut_position - cut a list into two
 * @list: a new list to add all removed entries
 * @head: a list with entries
 * @entry: an entry within head, could be the head itself
 *	and if so we won't cut the list
 *
 * This helper moves the initial part of @head, up to and
 * including @entry, from @head to @list. You should
 * pass on @entry an element you know is on @head. @list
 * should be an empty list or a list you do not care about
 * losing its data.
 *
 */
static inline void list_cut_position(struct list_head *list,
		struct list_head *head, struct list_head *entry)
{
	if (list_empty(head))
		return;
	if (list_is_singular(head) &&
		(head->next != entry && head != entry))
		return;
	if (entry == head)
		INIT_LIST_HEAD(list);
	else
		__list_cut_position(list, head, entry);
}


/* __list_splice as re-implemented on 2.6.27, we backport it */
static inline void __compat_list_splice_new_27(const struct list_head *list,
				 struct list_head *prev,
				 struct list_head *next)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

/**
 * list_splice_tail - join two lists, each list being a queue
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice_tail(struct list_head *list,
				struct list_head *head)
{
	if (!list_empty(list))
		__compat_list_splice_new_27(list, head->prev, head);
}

/**
 * list_splice_tail_init - join two lists and reinitialise the emptied list
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 *
 * Each of the lists is a queue.
 * The list at @list is reinitialised
 */
static inline void list_splice_tail_init(struct list_head *list,
					 struct list_head *head)
{
	if (!list_empty(list)) {
		__compat_list_splice_new_27(list, head->prev, head);
		INIT_LIST_HEAD(list);
	}
}

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)) */

#endif /* LINUX_26_COMPAT_H */

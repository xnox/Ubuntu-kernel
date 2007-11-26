#ifndef __kcompat_skb_mac_header_h__
#define __kcompat_skb_mac_header_h__

static inline void skb_set_mac_header(struct sk_buff *skb, int offset)
{
	skb->mac.raw = skb->data + offset;
}

static inline void skb_set_network_header(struct sk_buff *skb, int offset)
{
	skb->nh.raw = skb->data + offset;
}

static inline void skb_set_transport_header(struct sk_buff *skb, 
					    int offset)
{
	skb->h.raw = skb->data + offset;
}

static inline const u8 *skb_mac_header(const struct sk_buff *skb)
{
	return skb->mac.raw;
}

static inline const u8 *skb_network_header(const struct sk_buff *skb)
{
	return skb->nh.raw;
}

static inline const u8 *skb_transport_header(const struct sk_buff *skb)
{
	return skb->h.raw;
}

static inline const u8 *skb_tail_pointer(const struct sk_buff *skb)
{
	return skb->tail;
}

#endif

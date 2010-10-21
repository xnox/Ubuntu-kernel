#ifndef PKTLOG_H
#define PKTLOG_H

#ifdef CONFIG_ATH9K_PKTLOG
#define CUR_PKTLOG_VER          10010  /* Packet log version */
#define PKTLOG_MAGIC_NUM        7735225
#define ATH_PKTLOG_TX		0x000000001
#define ATH_PKTLOG_RX		0x000000002
#define ATH_PKTLOG_RCFIND	0x000000004
#define ATH_PKTLOG_RCUPDATE	0x000000008

#define ATH_DEBUGFS_PKTLOG_SIZE_DEFAULT (1024 * 1024)
#define ATH_PKTLOG_FILTER_DEFAULT (ATH_PKTLOG_TX | ATH_PKTLOG_RX |	\
		ATH_PKTLOG_RCFIND | ATH_PKTLOG_RCUPDATE)

#define PHFLAGS_MACVERSION_MASK 0x00ff0000
#define PHFLAGS_MACVERSION_SFT  16
#define PHFLAGS_MACREV_MASK 0xff0  /* MAC revision */
#define PHFLAGS_MACREV_SFT  4

struct ath_pktlog_hdr {
	u32 flags;
	u16 log_type; /* Type of log information foll this header */
	int16_t size; /* Size of variable length log information in bytes */
	u32 timestamp;
}  __packed;

/* Types of packet log events */
#define PKTLOG_TYPE_TXCTL    0
#define PKTLOG_TYPE_TXSTATUS 1
#define PKTLOG_TYPE_RX       2
#define PKTLOG_TYPE_RCFIND   3
#define PKTLOG_TYPE_RCUPDATE 4

#define PKTLOG_MAX_TXCTL_WORDS 12
#define PKTLOG_MAX_TXSTATUS_WORDS 10
#define PKTLOG_MAX_PROTO_WORDS  16

struct ath_pktlog_txctl {
	__le16 framectrl;       /* frame control field from header */
	__le16 seqctrl;         /* frame control field from header */
	u16 bssid_tail;      /* last two octets of bssid */
	u16 sa_tail;         /* last two octets of SA */
	u16 da_tail;         /* last two octets of DA */
	u16 resvd;
	u32 txdesc_ctl[PKTLOG_MAX_TXCTL_WORDS];     /* Tx descriptor words */
	unsigned long proto_hdr;   /* protocol header (variable length!) */
	int32_t misc[0]; /* Can be used for HT specific or other misc info */
}  __packed;

struct ath_pktlog_txstatus {
	/* Tx descriptor status words */
	u32 txdesc_status[PKTLOG_MAX_TXSTATUS_WORDS];
	int32_t misc[0]; /* Can be used for HT specific or other misc info */
}  __packed;

#define PKTLOG_MAX_RXSTATUS_WORDS 11

struct ath_pktlog_rx {
	u16 framectrl;       /* frame control field from header */
	u16 seqctrl;         /* sequence control field */
	u16 bssid_tail;      /* last two octets of bssid */
	u16 sa_tail;         /* last two octets of SA */
	u16 da_tail;         /* last two octets of DA */
	u16 resvd;
	u32 rxdesc_status[PKTLOG_MAX_RXSTATUS_WORDS];  /* Rx descriptor words */
	unsigned long proto_hdr;   /* protocol header (variable length!) */
	int32_t misc[0];    /* Can be used for HT specific or other misc info */
}  __packed;

struct ath_pktlog_rcfind {
	u8 rate;
	u8 rateCode;
	s8 rcRssiLast;
	s8 rcRssiLastPrev;
	s8 rcRssiLastPrev2;
	s8 rssiReduce;
	u8 rcProbeRate;
	s8 isProbing;
	s8 primeInUse;
	s8 currentPrimeState;
	u8 rcRateTableSize;
	u8 rcRateMax;
	u8 ac;
	int32_t misc[0]; /* Can be used for HT specific or other misc info */
}  __packed;

struct ath_pktlog_rcupdate {
	u8 txRate;
	u8 rateCode;
	s8 rssiAck;
	u8 Xretries;
	u8 retries;
	s8 rcRssiLast;
	s8 rcRssiLastLkup;
	s8 rcRssiLastPrev;
	s8 rcRssiLastPrev2;
	u8 rcProbeRate;
	u8 rcRateMax;
	s8 useTurboPrime;
	s8 currentBoostState;
	u8 rcHwMaxRetryRate;
	u8 ac;
	u8 resvd[2];
	s8 rcRssiThres[RATE_TABLE_SIZE];
	u8 rcPer[RATE_TABLE_SIZE];
	u8 resv2[RATE_TABLE_SIZE + 5];
	int32_t misc[0]; /* Can be used for HT specific or other misc info */
};

#define TXCTL_OFFSET(ah)      (AR_SREV_9300_20_OR_LATER(ah) ? 11 : 2)
#define TXCTL_NUMWORDS(ah)    (AR_SREV_5416_20_OR_LATER(ah) ? 12 : 8)
#define TXSTATUS_OFFSET(ah)   (AR_SREV_9300_20_OR_LATER(ah) ? 2 : 14)
#define TXSTATUS_NUMWORDS(ah) (AR_SREV_9300_20_OR_LATER(ah) ? 7 : 10)

#define RXCTL_OFFSET(ah)      (AR_SREV_9300_20_OR_LATER(ah) ? 0 : 3)
#define RXCTL_NUMWORDS(ah)    (AR_SREV_9300_20_OR_LATER(ah) ? 0 : 1)
#define RXSTATUS_OFFSET(ah)   (AR_SREV_9300_20_OR_LATER(ah) ? 1 : 4)
#define RXSTATUS_NUMWORDS(ah) (AR_SREV_9300_20_OR_LATER(ah) ? 11 : 9)

struct ath_desc_info {
	u8 txctl_offset;
	u8 txctl_numwords;
	u8 txstatus_offset;
	u8 txstatus_numwords;
	u8 rxctl_offset;
	u8 rxctl_numwords;
	u8 rxstatus_offset;
	u8 rxstatus_numwords;
};

#define PKTLOG_MOV_RD_IDX(_rd_offset, _log_buf, _log_size)  \
	do { \
		if ((_rd_offset + sizeof(struct ath_pktlog_hdr) + \
		    ((struct ath_pktlog_hdr *)((_log_buf)->log_data + \
			    (_rd_offset)))->size) <= _log_size) { \
			_rd_offset = ((_rd_offset) + \
					sizeof(struct ath_pktlog_hdr) + \
					((struct ath_pktlog_hdr *) \
					 ((_log_buf)->log_data + \
					  (_rd_offset)))->size); \
		} else { \
			_rd_offset = ((struct ath_pktlog_hdr *) \
					((_log_buf)->log_data +  \
					 (_rd_offset)))->size;  \
		} \
		(_rd_offset) = (((_log_size) - (_rd_offset)) >= \
				sizeof(struct ath_pktlog_hdr)) ? \
		_rd_offset : 0; \
	} while (0);

struct ath_pktlog_bufhdr {
	u32 magic_num;  /* Used by post processing scripts */
	u32 version;    /* Set to CUR_PKTLOG_VER */
};

struct ath_pktlog_buf {
	struct ath_pktlog_bufhdr bufhdr;
	int32_t rd_offset;
	int32_t wr_offset;
	char log_data[0];
};

struct ath_pktlog {
	struct ath_pktlog_buf *pktlog_buf;
	u32 pktlog_filter;
	u32 pktlog_buf_size;           /* Size of buffer in bytes */
	spinlock_t pktlog_lock;
};

struct ath_pktlog_debugfs {
	struct dentry *debugfs_pktlog;
	struct dentry *pktlog_enable;
	struct dentry *pktlog_start;
	struct dentry *pktlog_filter;
	struct dentry *pktlog_size;
	struct dentry *pktlog_dump;
	struct ath_pktlog pktlog;
};

void ath_pktlog_txctl(struct ath_softc *sc, struct ath_buf *bf);
void ath_pktlog_txstatus(struct ath_softc *sc, void *ds);
void ath_pktlog_rx(struct ath_softc *sc, void *ds, struct sk_buff *skb);
void ath9k_pktlog_rc(struct ath_softc *sc, struct ath_rate_priv *ath_rc_priv,
		int8_t ratecode, u8 rate, int8_t is_probing, u16 ac);
void ath9k_pktlog_rcupdate(struct ath_softc *sc,
			   struct ath_rate_priv *ath_rc_priv, u8 tx_rate,
			   u8 rate_code, u8 xretries, u8 retries, int8_t rssi,
			   u16 ac);
void ath9k_pktlog_txcomplete(struct ath_softc *sc ,struct list_head *bf_head,
			     struct ath_buf *bf, struct ath_buf *bf_last);
void ath9k_pktlog_txctrl(struct ath_softc *sc, struct list_head *bf_head,
			 struct ath_buf *lastbf);
int ath9k_init_pktlog(struct ath_softc *sc);
void ath9k_deinit_pktlog(struct ath_softc *sc);
#else /* CONFIG_ATH9K_PKTLOG */
static inline void ath_pktlog_txstatus(struct ath_softc *sc, void *ds)
{
}

static inline void ath_pktlog_rx(struct ath_softc *sc, void *ds,
				 struct sk_buff *skb)
{
}

static inline void ath9k_pktlog_rc(struct ath_softc *sc,
				   struct ath_rate_priv *ath_rc_priv,
				   int8_t ratecode, u8 rate,
				   int8_t is_probing, u16 ac)
{
}

static inline void ath9k_pktlog_rcupdate(struct ath_softc *sc,
					 struct ath_rate_priv *ath_rc_priv,
					 u8 tx_rate, u8 rate_code,
					 u8 xretries, u8 retries,
					 int8_t rssi, u16 ac)
{
}

static inline void ath9k_pktlog_txcomplete(struct ath_softc *sc,
					   struct list_head *bf_head,
					   struct ath_buf *bf,
					   struct ath_buf *bf_last)
{
}

static inline void ath9k_pktlog_txctrl(struct ath_softc *sc,
				       struct list_head *bf_head,
				       struct ath_buf *lastbf)
{
}
static inline int ath9k_init_pktlog(struct ath_softc *sc)
{
	return 0;
}
static inline void ath9k_deinit_pktlog(struct ath_softc *sc)
{
}
#endif /* CONFIG_ATH9K_PKTLOG */

#endif

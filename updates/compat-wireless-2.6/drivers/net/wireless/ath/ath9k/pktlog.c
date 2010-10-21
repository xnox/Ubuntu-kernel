
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include "ath9k.h"

static int ath9k_debugfs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static struct page *pktlog_virt_to_logical(void *addr)
{
	struct page *page;
	unsigned long vpage = 0UL;

	page = vmalloc_to_page(addr);
	if (page) {
		vpage = (unsigned long) page_address(page);
		vpage |= ((unsigned long) addr & (PAGE_SIZE - 1));
	}
	return virt_to_page((void *) vpage);
}

static void ath_pktlog_release(struct ath_pktlog *pktlog)
{
	unsigned long page_cnt, vaddr;
	struct page *page;

	page_cnt =
		((sizeof(*(pktlog->pktlog_buf)) +
		pktlog->pktlog_buf_size) / PAGE_SIZE) + 1;

	for (vaddr = (unsigned long) (pktlog->pktlog_buf); vaddr <
			(unsigned long) (pktlog->pktlog_buf) +
			(page_cnt * PAGE_SIZE);
			vaddr += PAGE_SIZE) {
		page = pktlog_virt_to_logical((void *) vaddr);
		clear_bit(PG_reserved, &page->flags);
	}

	vfree(pktlog->pktlog_buf);
	pktlog->pktlog_buf = NULL;
}

static int ath_alloc_pktlog_buf(struct ath_softc *sc)
{
	u32 page_cnt;
	unsigned long vaddr;
	struct page *page;
	struct ath_pktlog *pktlog = &sc->pktlog.pktlog;

	if (pktlog->pktlog_buf_size == 0)
		return -EINVAL;

	page_cnt = (sizeof(*(pktlog->pktlog_buf)) +
		    pktlog->pktlog_buf_size) / PAGE_SIZE;

	pktlog->pktlog_buf =  vmalloc((page_cnt + 2) * PAGE_SIZE);
	if (pktlog->pktlog_buf == NULL) {
		printk(KERN_ERR "Failed to allocate memory  for pktlog");
		return -ENOMEM;
	}

	pktlog->pktlog_buf = (struct ath_pktlog_buf *)
				     (((unsigned long)
				      (pktlog->pktlog_buf)
				     + PAGE_SIZE - 1) & PAGE_MASK);

	for (vaddr = (unsigned long) (pktlog->pktlog_buf);
		      vaddr < ((unsigned long) (pktlog->pktlog_buf)
		      + (page_cnt * PAGE_SIZE)); vaddr += PAGE_SIZE) {
		page = pktlog_virt_to_logical((void *)vaddr);
		set_bit(PG_reserved, &page->flags);
	}

	return 0;
}

static void ath_init_pktlog_buf(struct ath_pktlog *pktlog)
{
	pktlog->pktlog_buf->bufhdr.magic_num = PKTLOG_MAGIC_NUM;
	pktlog->pktlog_buf->bufhdr.version = CUR_PKTLOG_VER;
	pktlog->pktlog_buf->rd_offset = -1;
	pktlog->pktlog_buf->wr_offset = 0;
	if (pktlog->pktlog_filter == 0)
		pktlog->pktlog_filter = ATH_PKTLOG_FILTER_DEFAULT;
}

static char *ath_pktlog_getbuf(struct ath_pktlog *pl_info,
			       u16 log_type, size_t log_size,
		               u32 flags)
{
	struct ath_pktlog_buf *log_buf;
	struct ath_pktlog_hdr *log_hdr;
	int32_t cur_wr_offset, buf_size;
	char *log_ptr;

	log_buf = pl_info->pktlog_buf;
	buf_size = pl_info->pktlog_buf_size;

	spin_lock_bh(&pl_info->pktlog_lock);
	cur_wr_offset = log_buf->wr_offset;
	/* Move read offset to the next entry if there is a buffer overlap */
	if (log_buf->rd_offset >= 0) {
		if ((cur_wr_offset <= log_buf->rd_offset)
				&& (cur_wr_offset +
				sizeof(struct ath_pktlog_hdr)) >
				log_buf->rd_offset)
			PKTLOG_MOV_RD_IDX(log_buf->rd_offset, log_buf,
					  buf_size);
	} else {
		log_buf->rd_offset = cur_wr_offset;
	}

	log_hdr =
		(struct ath_pktlog_hdr *) (log_buf->log_data + cur_wr_offset);
	log_hdr->log_type = log_type;
	log_hdr->flags = flags;
	log_hdr->timestamp = jiffies;
	log_hdr->size = (u16) log_size;

	cur_wr_offset += sizeof(*log_hdr);

	if ((buf_size - cur_wr_offset) < log_size) {
		while ((cur_wr_offset <= log_buf->rd_offset)
				&& (log_buf->rd_offset < buf_size))
			PKTLOG_MOV_RD_IDX(log_buf->rd_offset, log_buf,
					  buf_size);
		cur_wr_offset = 0;
	}

	while ((cur_wr_offset <= log_buf->rd_offset)
			&& (cur_wr_offset + log_size) > log_buf->rd_offset)
		PKTLOG_MOV_RD_IDX(log_buf->rd_offset, log_buf, buf_size);

	log_ptr = &(log_buf->log_data[cur_wr_offset]);

	cur_wr_offset += log_hdr->size;

	log_buf->wr_offset =
		((buf_size - cur_wr_offset) >=
		 sizeof(struct ath_pktlog_hdr)) ? cur_wr_offset : 0;
	spin_unlock_bh(&pl_info->pktlog_lock);

	return log_ptr;
}

static void ath9k_hw_get_descinfo(struct ath_hw *ah, struct ath_desc_info *desc_info)
{
       desc_info->txctl_numwords = TXCTL_NUMWORDS(ah);
       desc_info->txctl_offset = TXCTL_OFFSET(ah);
       desc_info->txstatus_numwords = TXSTATUS_NUMWORDS(ah);
       desc_info->txstatus_offset = TXSTATUS_OFFSET(ah);

       desc_info->rxctl_numwords = RXCTL_NUMWORDS(ah);
       desc_info->rxctl_offset = RXCTL_OFFSET(ah);
       desc_info->rxstatus_numwords = RXSTATUS_NUMWORDS(ah);
       desc_info->rxstatus_offset = RXSTATUS_OFFSET(ah);
}

static int  pktlog_pgfault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	unsigned long address = (unsigned long) vmf->virtual_address;

	if (address == 0UL)
		return VM_FAULT_NOPAGE;

	if (vmf->pgoff > vma->vm_end)
		return VM_FAULT_SIGBUS;

	get_page(virt_to_page(address));
	vmf->page = virt_to_page(address);
	return VM_FAULT_MINOR;
}

static struct vm_operations_struct pktlog_vmops = {
	.fault = pktlog_pgfault
};

static int ath_pktlog_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ath_softc *sc = file->private_data;

	/* entire buffer should be mapped */
	if (vma->vm_pgoff != 0)
		return -EINVAL;

	if (!sc->pktlog.pktlog.pktlog_buf) {
		printk(KERN_ERR "Can't allocate pktlog buf");
		return -ENOMEM;
	}

	vma->vm_flags |= VM_LOCKED;
	vma->vm_ops = &pktlog_vmops;

	return 0;
}

static ssize_t ath_pktlog_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	size_t bufhdr_size;
	size_t nbytes = 0, ret_val = 0;
	int rem_len;
	int start_offset, end_offset;
	int fold_offset, ppos_data, cur_rd_offset;
	struct ath_softc *sc = file->private_data;
	struct ath_pktlog *pktlog_info = &sc->pktlog.pktlog;
	struct ath_pktlog_buf *log_buf = pktlog_info->pktlog_buf;

	if (log_buf == NULL)
		return 0;

	bufhdr_size = sizeof(log_buf->bufhdr);

	/* copy valid log entries from circular buffer into user space */
	rem_len = count;

	nbytes = 0;

	if (*ppos < bufhdr_size) {
		nbytes = min((int) (bufhdr_size -  *ppos), rem_len);
		if (copy_to_user(userbuf,
		    ((char *) &log_buf->bufhdr) + *ppos, nbytes))
			return -EFAULT;
		rem_len -= nbytes;
		ret_val += nbytes;
	}

	start_offset = log_buf->rd_offset;

	if ((rem_len == 0) || (start_offset < 0))
		goto read_done;

	fold_offset = -1;
	cur_rd_offset = start_offset;

	/* Find the last offset and fold-offset if the buffer is folded */
	do {
		struct ath_pktlog_hdr *log_hdr;
		int log_data_offset;

		log_hdr =
			(struct ath_pktlog_hdr *) (log_buf->log_data +
							cur_rd_offset);

		log_data_offset = cur_rd_offset + sizeof(struct ath_pktlog_hdr);

		if ((fold_offset == -1)
				&& ((pktlog_info->pktlog_buf_size -
				    log_data_offset) <= log_hdr->size))
			fold_offset = log_data_offset - 1;

		PKTLOG_MOV_RD_IDX(cur_rd_offset, log_buf,
				  pktlog_info->pktlog_buf_size);

		if ((fold_offset == -1) && (cur_rd_offset == 0)
				&& (cur_rd_offset != log_buf->wr_offset))
			fold_offset = log_data_offset + log_hdr->size - 1;

		end_offset = log_data_offset + log_hdr->size - 1;
	} while (cur_rd_offset != log_buf->wr_offset);

	ppos_data = *ppos + ret_val - bufhdr_size + start_offset;

	if (fold_offset == -1) {
		if (ppos_data > end_offset)
			goto read_done;

		nbytes = min(rem_len, end_offset - ppos_data + 1);
		if (copy_to_user(userbuf + ret_val,
				 log_buf->log_data + ppos_data, nbytes))
			return -EFAULT;
		ret_val += nbytes;
		rem_len -= nbytes;
	} else {
		if (ppos_data <= fold_offset) {
			nbytes = min(rem_len, fold_offset - ppos_data + 1);
			if (copy_to_user(userbuf + ret_val,
						log_buf->log_data + ppos_data,
						nbytes))
				return -EFAULT;
			ret_val += nbytes;
			rem_len -= nbytes;
		}

		if (rem_len == 0)
			goto read_done;

		ppos_data =
			*ppos + ret_val - (bufhdr_size +
					(fold_offset - start_offset + 1));

		if (ppos_data <= end_offset) {
			nbytes = min(rem_len, end_offset - ppos_data + 1);
			if (copy_to_user(userbuf + ret_val, log_buf->log_data
					 + ppos_data,
					 nbytes))
				return -EFAULT;
			ret_val += nbytes;
			rem_len -= nbytes;
		}
	}

read_done:
		*ppos += ret_val;

		return ret_val;
}

static const struct file_operations fops_pktlog_dump = {
	.read = ath_pktlog_read,
	.mmap = ath_pktlog_mmap,
	.open = ath9k_debugfs_open
};

static ssize_t write_pktlog_start(struct file *file, const char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	struct ath_softc *sc = file->private_data;
	struct ath_pktlog *pktlog = &sc->pktlog.pktlog;
	char buf[32];
	int buf_size;
	int start_pktlog, err;

	buf_size = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, ubuf, buf_size))
		return -EFAULT;

	sscanf(buf, "%d", &start_pktlog);
	if (start_pktlog) {
		if (pktlog->pktlog_buf != NULL)
			ath_pktlog_release(pktlog);

		err = ath_alloc_pktlog_buf(sc);
		if (err != 0)
			return err;

		ath_init_pktlog_buf(pktlog);
		pktlog->pktlog_buf->rd_offset = -1;
		pktlog->pktlog_buf->wr_offset = 0;
		sc->is_pkt_logging = 1;
	} else {
		sc->is_pkt_logging = 0;
	}

	sc->sc_ah->is_pkt_logging = sc->is_pkt_logging;
	return count;
}

static ssize_t read_pktlog_start(struct file *file, char __user *ubuf,
				  size_t count, loff_t *ppos)
{
	char buf[32];
	struct ath_softc *sc = file->private_data;
	int len = 0;

	len = scnprintf(buf, sizeof(buf) - len, "%d", sc->is_pkt_logging);
	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_start = {
	.read = read_pktlog_start,
	.write = write_pktlog_start,
	.open = ath9k_debugfs_open
};

static ssize_t pktlog_size_write(struct file *file, const char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct ath_softc *sc = file->private_data;
	char buf[32];
	u32 pktlog_size;
	int buf_size;

	buf_size = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, ubuf, buf_size))
		return -EFAULT;

	sscanf(buf, "%d", &pktlog_size);

	if (pktlog_size == sc->pktlog.pktlog.pktlog_buf_size)
		return count;

	if (sc->is_pkt_logging) {
		printk(KERN_DEBUG "Stop packet logging before"
			" changing the pktlog size \n");
		return -EINVAL;
	}

	sc->pktlog.pktlog.pktlog_buf_size = pktlog_size;

	return count;
}

static ssize_t pktlog_size_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char buf[32];
	struct ath_softc *sc = file->private_data;
	int len = 0;

	len = scnprintf(buf, sizeof(buf) - len, "%ul",
			    sc->pktlog.pktlog.pktlog_buf_size);
	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_size = {
	.read = pktlog_size_read,
	.write = pktlog_size_write,
	.open = ath9k_debugfs_open
};

static ssize_t pktlog_filter_write(struct file *file, const char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	char buf[32];
	struct ath_softc *sc = file->private_data;
	u32 filter;
	int buf_count;

	buf_count = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, ubuf, buf_count))
		return -EFAULT;

	if (sscanf(buf, "%x", &filter))
		sc->pktlog.pktlog.pktlog_filter = filter;
	else
		sc->pktlog.pktlog.pktlog_filter = 0;

	return count;
}

static ssize_t  pktlog_filter_read(struct file *file, char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	char buf[32];
	struct ath_softc *sc = file->private_data;
	int len = 0;

	len = scnprintf(buf, sizeof(buf) - len, "%ul",
			    sc->pktlog.pktlog.pktlog_filter);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_filter = {
	.read = pktlog_filter_read,
	.write = pktlog_filter_write,
	.open = ath9k_debugfs_open
};

void ath_pktlog_txctl(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ath_pktlog_txctl *tx_log;
	struct ath_pktlog *pl_info;
	struct ieee80211_hdr *hdr;
	struct ath_desc_info desc_info;
	int i;
	u32 *ds_words, flags = 0;

	pl_info = &sc->pktlog.pktlog;

	if ((pl_info->pktlog_filter & ATH_PKTLOG_TX) == 0 ||
	    bf->bf_mpdu == NULL || !sc->is_pkt_logging)
		return;

	flags |= (((sc->sc_ah->hw_version.macRev <<
			PHFLAGS_MACREV_SFT) & PHFLAGS_MACREV_MASK) |
			((sc->sc_ah->hw_version.macVersion << PHFLAGS_MACVERSION_SFT)
			& PHFLAGS_MACVERSION_MASK));

	tx_log = (struct ath_pktlog_txctl *)ath_pktlog_getbuf(pl_info,
			PKTLOG_TYPE_TXCTL, sizeof(*tx_log), flags);

	memset(tx_log, 0, sizeof(*tx_log));

	hdr = (struct ieee80211_hdr *) bf->bf_mpdu->data;
	tx_log->framectrl = hdr->frame_control;
	tx_log->seqctrl   = hdr->seq_ctrl;

	if (ieee80211_has_tods(tx_log->framectrl)) {
		tx_log->bssid_tail = (hdr->addr1[ETH_ALEN - 2] << 8) |
				     (hdr->addr1[ETH_ALEN - 1]);
		tx_log->sa_tail    = (hdr->addr2[ETH_ALEN - 2] << 8) |
				     (hdr->addr2[ETH_ALEN - 1]);
		tx_log->da_tail    = (hdr->addr3[ETH_ALEN - 2] << 8) |
				     (hdr->addr3[ETH_ALEN - 1]);
	} else if (ieee80211_has_fromds(tx_log->framectrl)) {
		tx_log->bssid_tail = (hdr->addr2[ETH_ALEN - 2] << 8) |
				     (hdr->addr2[ETH_ALEN - 1]);
		tx_log->sa_tail    = (hdr->addr3[ETH_ALEN - 2] << 8) |
				     (hdr->addr3[ETH_ALEN - 1]);
		tx_log->da_tail    = (hdr->addr1[ETH_ALEN - 2] << 8) |
				     (hdr->addr1[ETH_ALEN - 1]);
	} else {
		tx_log->bssid_tail = (hdr->addr3[ETH_ALEN - 2] << 8) |
				     (hdr->addr3[ETH_ALEN - 1]);
		tx_log->sa_tail	   = (hdr->addr2[ETH_ALEN - 2] << 8) |
				     (hdr->addr2[ETH_ALEN - 1]);
		tx_log->da_tail    = (hdr->addr1[ETH_ALEN - 2] << 8) |
				     (hdr->addr1[ETH_ALEN - 1]);
	}

	ath9k_hw_get_descinfo(sc->sc_ah, &desc_info);

	ds_words = (u32 *)(bf->bf_desc) + desc_info.txctl_offset;
	for (i = 0; i < desc_info.txctl_numwords; i++)
		tx_log->txdesc_ctl[i] = ds_words[i];
}

void ath_pktlog_txstatus(struct ath_softc *sc, void *ds)
{
	struct ath_pktlog_txstatus *tx_log;
	struct ath_pktlog *pl_info;
	struct ath_desc_info desc_info;
	int i;
	u32 *ds_words, flags = 0;

	pl_info = &sc->pktlog.pktlog;

	if ((pl_info->pktlog_filter & ATH_PKTLOG_TX) == 0 ||
	    !sc->is_pkt_logging)
		return;

	flags |= (((sc->sc_ah->hw_version.macRev <<
		  PHFLAGS_MACREV_SFT) & PHFLAGS_MACREV_MASK) |
		  ((sc->sc_ah->hw_version.macVersion << PHFLAGS_MACVERSION_SFT)
		  & PHFLAGS_MACVERSION_MASK));
	tx_log = (struct ath_pktlog_txstatus *)ath_pktlog_getbuf(pl_info,
			PKTLOG_TYPE_TXSTATUS, sizeof(*tx_log), flags);

	memset(tx_log, 0, sizeof(*tx_log));

	ath9k_hw_get_descinfo(sc->sc_ah, &desc_info);

	ds_words = (u32 *)(ds) + desc_info.txstatus_offset;

	for (i = 0; i < desc_info.txstatus_numwords; i++)
		tx_log->txdesc_status[i] = ds_words[i];
}

void ath_pktlog_rx(struct ath_softc *sc, void *desc, struct sk_buff *skb)
{
	struct ath_pktlog_rx *rx_log;
	struct ath_pktlog *pl_info;
	struct ieee80211_hdr *hdr;
	struct ath_desc_info desc_info;
	int i;
	u32 *ds_words, flags = 0;

	pl_info = &sc->pktlog.pktlog;

	if ((pl_info->pktlog_filter & ATH_PKTLOG_RX) == 0 ||
	    !sc->is_pkt_logging)
		return;

	flags |= (((sc->sc_ah->hw_version.macRev <<
		  PHFLAGS_MACREV_SFT) & PHFLAGS_MACREV_MASK) |
		  ((sc->sc_ah->hw_version.macVersion <<
		 PHFLAGS_MACVERSION_SFT) & PHFLAGS_MACVERSION_MASK));

	rx_log = (struct ath_pktlog_rx *)ath_pktlog_getbuf(pl_info, PKTLOG_TYPE_RX,
			sizeof(*rx_log), flags);

	memset(rx_log, 0, sizeof(*rx_log));

	if (skb->len > sizeof(struct ieee80211_hdr)) {
		hdr = (struct ieee80211_hdr *) skb->data;
		rx_log->framectrl = hdr->frame_control;
		rx_log->seqctrl   = hdr->seq_ctrl;

		if (ieee80211_has_tods(rx_log->framectrl)) {
			rx_log->bssid_tail = (hdr->addr1[ETH_ALEN - 2] << 8) |
					     (hdr->addr1[ETH_ALEN - 1]);
			rx_log->sa_tail    = (hdr->addr2[ETH_ALEN - 2] << 8) |
					     (hdr->addr2[ETH_ALEN - 1]);
			rx_log->da_tail    = (hdr->addr3[ETH_ALEN - 2] << 8) |
					     (hdr->addr3[ETH_ALEN - 1]);
		} else if (ieee80211_has_fromds(rx_log->framectrl)) {
			rx_log->bssid_tail = (hdr->addr2[ETH_ALEN - 2] << 8) |
					     (hdr->addr2[ETH_ALEN - 1]);
			rx_log->sa_tail    = (hdr->addr3[ETH_ALEN - 2] << 8) |
					     (hdr->addr3[ETH_ALEN - 1]);
			rx_log->da_tail    = (hdr->addr1[ETH_ALEN - 2] << 8) |
					     (hdr->addr1[ETH_ALEN - 1]);
		} else {
			rx_log->bssid_tail = (hdr->addr3[ETH_ALEN - 2] << 8) |
					     (hdr->addr3[ETH_ALEN - 1]);
			rx_log->sa_tail    = (hdr->addr2[ETH_ALEN - 2] << 8) |
					     (hdr->addr2[ETH_ALEN - 1]);
			rx_log->da_tail    = (hdr->addr1[ETH_ALEN - 2] << 8) |
					     (hdr->addr1[ETH_ALEN - 1]);
		}
	} else {
		hdr = (struct ieee80211_hdr *) skb->data;

		if (ieee80211_is_ctl(hdr->frame_control)) {
			rx_log->framectrl = hdr->frame_control;
			rx_log->da_tail = (hdr->addr1[ETH_ALEN - 2] << 8) |
					     (hdr->addr1[ETH_ALEN - 1]);
			if (skb->len < sizeof(struct ieee80211_rts)) {
				rx_log->sa_tail = 0;
			} else {
				rx_log->sa_tail = (hdr->addr2[ETH_ALEN - 2]
						  << 8) |
						  (hdr->addr2[ETH_ALEN - 1]);
			}
		} else {
			rx_log->framectrl = 0xFFFF;
			rx_log->da_tail = 0;
			rx_log->sa_tail = 0;
		}

		rx_log->seqctrl   = 0;
		rx_log->bssid_tail = 0;
	}

	ath9k_hw_get_descinfo(sc->sc_ah, &desc_info);

	ds_words = (u32 *)(desc) + desc_info.rxstatus_offset;

	for (i = 0; i < desc_info.rxstatus_numwords; i++)
		rx_log->rxdesc_status[i] = ds_words[i];
}

void ath9k_pktlog_rc(struct ath_softc *sc, struct ath_rate_priv *ath_rc_priv,
		     int8_t ratecode, u8 rate, int8_t is_probing, u16 ac)
{
	struct ath_pktlog_rcfind *rcf_log;
	struct ath_pktlog *pl_info;
	u32 flags = 0;

	pl_info = &sc->pktlog.pktlog;

	if ((pl_info->pktlog_filter & ATH_PKTLOG_RCFIND) == 0 ||
	    !sc->is_pkt_logging)
		return;

	flags |= (((sc->sc_ah->hw_version.macRev <<
		 PHFLAGS_MACREV_SFT) & PHFLAGS_MACREV_MASK) |
		 ((sc->sc_ah->hw_version.macVersion <<
		 PHFLAGS_MACVERSION_SFT) & PHFLAGS_MACVERSION_MASK));
	rcf_log = (struct ath_pktlog_rcfind *)ath_pktlog_getbuf(pl_info,
		  PKTLOG_TYPE_RCFIND, sizeof(*rcf_log), flags);

	rcf_log->rate = rate;
	rcf_log->rateCode = ratecode;
	rcf_log->rcProbeRate = is_probing ? ath_rc_priv->probe_rate : 0;
	rcf_log->isProbing = is_probing;
	rcf_log->ac = ac;
	rcf_log->rcRateMax = ath_rc_priv->rate_max_phy;
	rcf_log->rcRateTableSize = ath_rc_priv->rate_table_size;
}

void ath9k_pktlog_rcupdate(struct ath_softc *sc, struct ath_rate_priv *ath_rc_priv, u8 tx_rate,
			   u8 rate_code, u8 xretries, u8 retries, int8_t rssi, u16 ac)
{
	struct ath_pktlog_rcupdate *rcu_log;
	struct ath_pktlog *pl_info;
	int i;
	u32 flags = 0;

	pl_info = &sc->pktlog.pktlog;

	if ((pl_info->pktlog_filter & ATH_PKTLOG_RCUPDATE) == 0 ||
	    !sc->is_pkt_logging)
		return;

	flags |= (((sc->sc_ah->hw_version.macRev <<
		 PHFLAGS_MACREV_SFT) & PHFLAGS_MACREV_MASK) |
		 ((sc->sc_ah->hw_version.macVersion <<
		 PHFLAGS_MACVERSION_SFT) & PHFLAGS_MACVERSION_MASK));
	rcu_log = (struct ath_pktlog_rcupdate *)ath_pktlog_getbuf(pl_info,
						PKTLOG_TYPE_RCUPDATE,
						sizeof(*rcu_log), flags);

	memset(rcu_log, 0, sizeof(*rcu_log));

	rcu_log->txRate = tx_rate;
	rcu_log->rateCode = rate_code;
	rcu_log->Xretries = xretries;
	rcu_log->retries = retries;
	rcu_log->rssiAck = rssi;
	rcu_log->ac = ac;
	rcu_log->rcProbeRate = ath_rc_priv->probe_rate;
	rcu_log->rcRateMax = ath_rc_priv->rate_max_phy;

	for (i = 0; i < RATE_TABLE_SIZE; i++) {
		rcu_log->rcPer[i] = ath_rc_priv->per[i];
	}
}

void ath9k_pktlog_txcomplete(struct ath_softc *sc, struct list_head *bf_head,
			     struct ath_buf *bf, struct ath_buf *bf_last)
{
	struct log_tx ;
	struct ath_buf *tbf;

	list_for_each_entry(tbf, bf_head, list)
		ath_pktlog_txctl(sc, tbf);

	if (bf->bf_next == NULL && bf_last->bf_stale)
		ath_pktlog_txctl(sc, bf_last);
}

void ath9k_pktlog_txctrl(struct ath_softc *sc, struct list_head *bf_head, struct ath_buf *lastbf)
{
	struct log_tx ;
	struct ath_buf *tbf;

	list_for_each_entry(tbf, bf_head, list)
		ath_pktlog_txctl(sc, tbf);

	/* log the last descriptor. */
	ath_pktlog_txctl(sc, lastbf);
}

static void pktlog_init(struct ath_softc *sc)
{
	spin_lock_init(&sc->pktlog.pktlog.pktlog_lock);
	sc->pktlog.pktlog.pktlog_buf_size = ATH_DEBUGFS_PKTLOG_SIZE_DEFAULT;
	sc->pktlog.pktlog.pktlog_buf = NULL;
	sc->pktlog.pktlog.pktlog_filter = 0;
}

int ath9k_init_pktlog(struct ath_softc *sc)
{
	sc->pktlog.debugfs_pktlog = debugfs_create_dir("pktlog",
			sc->debug.debugfs_phy);
	if (!sc->pktlog.debugfs_pktlog)
		goto err;

	sc->pktlog.pktlog_start = debugfs_create_file("pktlog_start",
			S_IRUGO | S_IWUSR,
			sc->pktlog.debugfs_pktlog,
			sc, &fops_pktlog_start);
	if (!sc->pktlog.pktlog_start)
		goto err;

	sc->pktlog.pktlog_size = debugfs_create_file("pktlog_size",
			S_IRUGO | S_IWUSR,
			sc->pktlog.debugfs_pktlog,
			sc, &fops_pktlog_size);
	if (!sc->pktlog.pktlog_size)
		goto err;

	sc->pktlog.pktlog_filter = debugfs_create_file("pktlog_filter",
			S_IRUGO | S_IWUSR,
			sc->pktlog.debugfs_pktlog,
			sc, &fops_pktlog_filter);
	if (!sc->pktlog.pktlog_filter)
		goto err;

	sc->pktlog.pktlog_dump = debugfs_create_file("pktlog_dump",
			S_IRUGO,
			sc->pktlog.debugfs_pktlog,
			sc, &fops_pktlog_dump);
	if (!sc->pktlog.pktlog_dump)
		goto err;

	pktlog_init(sc);

	return 0;

err:
	return -ENOMEM;
}

void ath9k_deinit_pktlog(struct ath_softc *sc)
{
	struct ath_pktlog *pktlog = &sc->pktlog.pktlog;

	if (pktlog->pktlog_buf != NULL)
		ath_pktlog_release(pktlog);

	debugfs_remove(sc->pktlog.pktlog_start);
	debugfs_remove(sc->pktlog.pktlog_size);
	debugfs_remove(sc->pktlog.pktlog_filter);
	debugfs_remove(sc->pktlog.pktlog_dump);
	debugfs_remove(sc->pktlog.debugfs_pktlog);
}

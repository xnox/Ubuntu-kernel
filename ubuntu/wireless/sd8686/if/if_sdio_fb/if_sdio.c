/** @file if_sdio.c
 *  @brief This file contains SDIO IF (interface) module
 *  related functions.
 *  
 * Copyright 2005 Intel Corporation and its suppliers. All rights reserved
 * 
 * (c) Copyright © 2003-2007, Marvell International Ltd. 
 *
 * This software file (the "File") is distributed by Marvell International 
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991 
 * (the "License").  You may use, redistribute and/or modify this File in 
 * accordance with the terms and conditions of the License, a copy of which 
 * is available along with the File in the gpl.txt file or by writing to 
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
 * 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
 * this warranty disclaimer.
 *
 */

#include "if_sdio.h"
#include <linux/mmc/sdio_protocol.h>
#include <asm/scatterlist.h>

#include <linux/firmware.h>

#define DEFAULT_HELPER_NAME "mrvl/helper_sd.bin"
#define DEFAULT_FW_NAME "mrvl/sd8686.bin"

extern u8 *helper_name;
extern u8 *fw_name;

/* 
 * Define the SD Block Size
 * for SD8381-B0/B1, SD8385/B0 chip, it could be 32,64,128,256,512
 * for all other chips including SD8381-A0, it must be 32
 */

/* define SD block size for firmware download */
#define SD_BLOCK_SIZE_FW_DL     32
#define SDIO_HEADER_LEN		4

/* define SD block size for data Tx/Rx */
#define SD_BLOCK_SIZE           320     /* To minimize the overhead of ethernet frame
                                           with 1514 bytes, 320 bytes block size is used */

#define ALLOC_BUF_SIZE          (((MAX(MRVDRV_ETH_RX_PACKET_BUFFER_SIZE, \
                                        MRVDRV_SIZE_OF_CMD_BUFFER) + SDIO_HEADER_LEN \
                                        + SD_BLOCK_SIZE - 1) / SD_BLOCK_SIZE) * SD_BLOCK_SIZE)

#define GPIO_PORT_NUM
#define GPIO_PORT_INIT()
#define GPIO_PORT_TO_HIGH()
#define GPIO_PORT_TO_LOW()

/*
 * Poll the Card Status register until the bits specified in the argument
 * are turned on.
 */

#define BLOCK_MODE 1
#define FIXED_ADDRESS 0

struct net_device *the_netdev = NULL;
extern void mss_set_sdio_int(struct mss_host *, int);
extern int mss_send_request(struct mss_request *req);

static int
mss_sdio_read_write(struct mss_card *card, int action,
                    struct mss_rw_arg *darg, u8 * byte)
{
    struct mss_request request;
    struct mss_rw_result result;
    int ret;

    request.card = card;
    request.action = action;
    request.result = &result;
    request.arg = darg;

    PRINTM(INFO, "ready to send request, card %x", (u32) card);
    ret = mss_send_request(&request);
    PRINTM(INFO, "get request, ret %d, bytes xferred = %d", ret,
           result.bytes_xfered);
    if ((darg->nob == 1) && (darg->block_len == 0) && (action == MSS_READ_MEM)
        && byte) {
        *byte = (u8) (result.bytes_xfered);
    }
    return ret;
}

static int
sdio_single_byte(int action, struct mss_card *card, int fun_num, u32 address,
                 u8 * byte)
{
    struct mss_rw_arg sdio_request;
    int ret;

    sdio_request.nob = 1;
    sdio_request.block = address;
    sdio_request.val = *byte;
    sdio_request.func = fun_num;
    sdio_request.opcode = 0;
    sdio_request.sg = NULL;
    sdio_request.sg_len = 0;
    sdio_request.block_len = 0;

    ret = mss_sdio_read_write(card, action, &sdio_request, byte);
    return ret;
}

static wlan_private *pwlanpriv;
static wlan_private *(*wlan_add_callback) (void *dev_id);
static int (*wlan_remove_callback) (void *dev_id);
static int cmd_result = 0;

int mv_sdio_read_event_cause(wlan_private * priv);

void
sbi_interrupt(struct mss_card *card)
{
    ENTER();

#ifdef TXRX_DEBUG
    wlan_private *priv = pwlanpriv;

    PRINTM(INFO, "sbi_interrupt called IntCounter= %d\n",
           priv->adapter->IntCounter);
#endif

//      TXRX_DEBUG_GET_ALL(0x00, 0xff, 0xff);
    wlan_interrupt(the_netdev);

    LEAVE();
}

/*
 * This function probes the SDIO card's CIS space to see if it is a
 * card that this driver supports and handles.
 */

static int
sbi_dev_probe_card(struct device *dev)
{
    int result, ret = WLAN_STATUS_FAILURE;
    u8 chiprev, bic;
    struct mss_card *cdev;
    struct sdio_card *card_info;

    ENTER();

    cdev = container_of(dev, struct mss_card, dev);
    if (!((cdev->card_type == MSS_SDIO_CARD) ||
          (cdev->card_type == MSS_COMBO_CARD)))
        goto done;

    if (!cdev->prot_card)
        goto done;

    /* Check for MANFID */
    PRINTM(INFO, "1 &&& in wlan probe &&&, card: 0x%p, slot: 0x%p\n", cdev,
           cdev->slot);
    card_info = (struct sdio_card *) cdev->prot_card;
    result = card_info->ccis.manufacturer;

    PRINTM(INFO, "card man fid code is 0x:%x\n", result);
    if (result == 0x02df) {
        PRINTM(INFO, "Marvell SDIO card detected!\n");
    } else {
        PRINTM(MSG, "Ignoring a non-Marvell SDIO card...\n");
        ret = WLAN_STATUS_FAILURE;
        goto done;
    }

    result =
        sdio_single_byte(MSS_READ_MEM, cdev, FN1, CARD_REVISION_REG,
                         &chiprev);

    /* read Revision Register to get the hw revision number */
    if (result) {
        PRINTM(FATAL, "cannot read CARD_REVISION_REG\n");
    } else {
        PRINTM(INFO, "revision=0x%x\n", chiprev);
        switch (chiprev) {
        default:
#if 0
            card->hw_tx_done = TRUE;
            card->block_size_512 = TRUE;
            card->async_int_mode = TRUE;
#endif
            /* enable async interrupt mode */
            sdio_single_byte(MSS_READ_MEM, cdev, FN0, BUS_IF_CTRL, &bic);
            bic |= ASYNC_INT_MODE;
            sdio_single_byte(MSS_WRITE_MEM, cdev, FN0, BUS_IF_CTRL, &bic);
            break;
        }
    }

    if (!wlan_add_callback)
        goto done;

    PRINTM(INFO, "2 &&& in wlan probe &&&, card: 0x%p, slot: 0x%p\n", cdev,
           cdev->slot);
    pwlanpriv = wlan_add_callback(cdev->slot);

    if (pwlanpriv)
        ret = WLAN_STATUS_SUCCESS;
    else
        ret = WLAN_STATUS_FAILURE;
  done:
    LEAVE();
    return ret;
}

int
sbi_probe_card(void *val)
{

    ENTER();
    LEAVE();
    return WLAN_STATUS_SUCCESS;
}

static int
sbi_dev_remove_card(struct device *dev)
{
    struct mss_slot *slot;
    struct mss_card *card;

    ENTER();
    card = container_of(dev, struct mss_card, dev);
    slot = card->slot;

    PRINTM(INFO, "1 &&& in wlan remove &&&, card: 0x%p, slot: 0x%p\n", card,
           slot);
    if (!wlan_remove_callback)
        return WLAN_STATUS_FAILURE;
    pwlanpriv = NULL;
    mss_set_sdio_int(slot->host, MSS_SDIO_INT_DIS);
    LEAVE();
    return wlan_remove_callback(slot);
}

static void
sbi_io_request_done(struct mss_request *req)
{
    ENTER();
    cmd_result = (req->errno) ? 0 : -1;
    LEAVE();
}

static struct mss_driver wlan_driver = {
    .sdio_int_handler = sbi_interrupt,
    .request_done = sbi_io_request_done,
    .driver = {
               .name = "mss_io",
               .probe = sbi_dev_probe_card,
               .remove = sbi_dev_remove_card,
               .shutdown = NULL,
               .suspend = NULL,
               .resume = NULL,
               },
};

int *
sbi_register(wlan_notifier_fn_add add, wlan_notifier_fn_remove remove,
             void *arg)
{
    wlan_add_callback = add;
    wlan_remove_callback = remove;

    ENTER();
    register_mss_driver(&wlan_driver);
    PRINTM(INFO, "register SDWLAN driver\n");
    LEAVE();
    return (int *) wlan_add_callback;
}

void
sbi_unregister(void)
{
    ENTER();
    unregister_mss_driver(&wlan_driver);

    wlan_add_callback = NULL;
    wlan_remove_callback = NULL;
    LEAVE();
}

int
sbi_read_ioreg(wlan_private * priv, u8 func, u32 reg, u8 * dat)
{
    struct mss_slot *slot = (struct mss_slot *) (priv->wlan_dev.card);
    int result;

    PRINTM(INFO, "READ at func :%d, reg: 0x%x,", func, reg);
    result = sdio_single_byte(MSS_READ_MEM, slot->card, func, reg, dat);
    PRINTM(INFO, "GET data: 0x%x, result :%d\n", *dat, result);
    return result;
}

int
sbi_write_ioreg(wlan_private * priv, u8 func, u32 reg, u8 dat)
{
    struct mss_slot *slot = (struct mss_slot *) (priv->wlan_dev.card);
    int result;

    PRINTM(INFO, "WRITE at func :%d, reg: 0x%x,", func, reg);
    result = sdio_single_byte(MSS_WRITE_MEM, slot->card, func, reg, &dat);
    PRINTM(INFO, "SET data: %d, result :%d\n", dat, result);
    return result;

}

int
sbi_write_iomem(wlan_private * priv, u8 func, u32 reg, u8 blockmode,
                u8 opcode, ssize_t cnt, ssize_t blksz, u8 * dat)
{
    struct mss_rw_arg sdio_request;
    struct mss_slot *slot = (struct mss_slot *) (priv->wlan_dev.card);
    struct scatterlist sg;
    int result;

    ENTER();
    PRINTM(INFO,
           "WRITE IOMEM: func:%d, reg:0x%x, blockmode:%d, opcode:%d, cnt:%d, blksz:%d\n",
           func, reg, blockmode, opcode, cnt, blksz);
    sdio_request.func = func;
    sdio_request.block = reg;
    sdio_request.nob = cnt;
    sdio_request.val = blockmode;
    sdio_request.opcode = opcode;
    sdio_request.sg = &sg;
    sdio_request.sg->page = virt_to_page(dat);
    sdio_request.sg->offset = offset_in_page(dat);
    sdio_request.sg->length = cnt * blksz;
    sdio_request.sg_len = 1;
    sdio_request.block_len = blksz;

    result =
        mss_sdio_read_write(slot->card, MSS_WRITE_MEM, &sdio_request, NULL);
    LEAVE();
    return result;
}

int
sbi_read_iomem(wlan_private * priv, u8 func, u32 reg, u8 blockmode, u8 opcode,
               ssize_t cnt, ssize_t blksz, u8 * dat)
{
    struct mss_rw_arg sdio_request;
    struct mss_slot *slot = (struct mss_slot *) (priv->wlan_dev.card);
    struct scatterlist sg;
    int result;

    ENTER();
    PRINTM(INFO,
           "READ IOMEM: func:%d, reg:0x%x, blockmode:%d, opcode:%d, cnt:%d, blksz:%d\n",
           func, reg, blockmode, opcode, cnt, blksz);
    sdio_request.func = func;
    sdio_request.block = reg;
    sdio_request.nob = cnt;
    sdio_request.val = blockmode;       //byte mode
    sdio_request.opcode = opcode;
    sdio_request.sg = &sg;
    sdio_request.sg->page = virt_to_page(dat);
    sdio_request.sg->offset = offset_in_page(dat);
    sdio_request.sg->length = cnt * blksz;
    sdio_request.sg_len = 1;
    sdio_request.block_len = blksz;

    result =
        mss_sdio_read_write(slot->card, MSS_READ_MEM, &sdio_request, NULL);
    LEAVE();
    return result;
}

int
sbi_read_intr_reg(wlan_private * priv, u8 * ireg)
{
    return sbi_read_ioreg(priv, FN1, HOST_INTSTATUS_REG, ireg);
}

int
sbi_read_card_reg(wlan_private * priv, u8 * cs)
{
    return sbi_read_ioreg(priv, FN1, CARD_STATUS_REG, cs);
}

int
sbi_clear_int_status(wlan_private * priv, u8 mask)
{
    return sbi_write_ioreg(priv, FN1, HOST_INTSTATUS_REG, 0x0);
}

int
sbi_get_int_status(wlan_private * priv, u8 * ireg)
{
    int ret = WLAN_STATUS_SUCCESS;
    u8 cs, *cmdBuf;
    wlan_dev_t *wlan_dev = &priv->wlan_dev;
    struct sk_buff *skb;
    struct mss_slot *slot = (struct mss_slot *) (priv->wlan_dev.card);

    ENTER();

    /*disable SDIO interrupt from FB card */
    mss_set_sdio_int(slot->host, MSS_SDIO_INT_DIS);

    if ((ret = sbi_read_ioreg(priv, FN1, HOST_INTSTATUS_REG, ireg)) < 0) {
        PRINTM(WARN, "sdio_read_ioreg: reading interrupt status register"
               " failed\n");
        ret = WLAN_STATUS_FAILURE;
        goto end;
    }

    if (*ireg != 0) {           /* DN_LD_HOST_INT_STATUS and/or UP_LD_HOST_INT_STATUS */
        /* Clear the interrupt status register */
        PRINTM(INFO, "ireg = 0x%x, ~ireg = 0x%x\n", *ireg, ~(*ireg));
        if ((ret = sbi_write_ioreg(priv, FN1, HOST_INTSTATUS_REG,
                                   ~(*ireg) & (DN_LD_HOST_INT_STATUS |
                                               UP_LD_HOST_INT_STATUS))) < 0) {
            PRINTM(WARN, "sdio_write_ioreg: clear interrupt status"
                   " register failed\n");
            ret = WLAN_STATUS_FAILURE;
            goto end;
        }
    }

    if (*ireg & DN_LD_HOST_INT_STATUS) {
        *ireg |= HIS_TxDnLdRdy;
        if (!priv->wlan_dev.dnld_sent) {        // tx_done already received
            PRINTM(INFO, "warning: tx_done already received:"
                   " dnld_sent=0x%x ireg=0x%x cs=0x%x\n",
                   priv->wlan_dev.dnld_sent, *ireg, cs);
        } else {
            PRINTM(INFO, "tx_done received: dnld_sent=0x%x ireg=0x%x"
                   " cs=0x%x\n", priv->wlan_dev.dnld_sent, *ireg, cs);
            if (priv->wlan_dev.dnld_sent == DNLD_DATA_SENT)
                os_start_queue(priv);
            priv->wlan_dev.dnld_sent = DNLD_RES_RECEIVED;
        }
    }

    if (*ireg & UP_LD_HOST_INT_STATUS) {

        /*
         * DMA read data is by block alignment,so we need alloc extra block
         * to avoid wrong memory access.
         */
        if (!(skb = dev_alloc_skb(ALLOC_BUF_SIZE))) {
            PRINTM(WARN, "No free skb\n");
            priv->stats.rx_dropped++;
            return WLAN_STATUS_FAILURE;
        }

        /* Transfer data from card */
        /* TODO: Check for error return on the read */
        /* skb->tail is passed as we are calling skb_put after we
         * are reading the data */
        if (mv_sdio_card_to_host(priv, &wlan_dev->upld_typ,
                                 (int *) &wlan_dev->upld_len, skb->tail,
                                 ALLOC_BUF_SIZE) < 0) {
            PRINTM(WARN, "Card to host failed: ireg=0x%x cs=0x%x\n",
                   *ireg, cs);
            if (sbi_read_ioreg(priv, FN1, CONFIGURATION_REG, &cs) < 0)
                PRINTM(WARN, "sdio_read_ioreg failed\n");

            PRINTM(INFO, "Config Reg val = %d\n", cs);
            if (sbi_write_ioreg(priv, FN1, CONFIGURATION_REG, (cs | 0x04)) <
                0)
                PRINTM(WARN, "write ioreg failed\n");

            PRINTM(INFO, "write success\n");
            if (sbi_read_ioreg(priv, FN1, CONFIGURATION_REG, &cs) < 0)
                PRINTM(WARN, "sdio_read_ioreg failed\n");

            PRINTM(INFO, "Config reg val =%x\n", cs);
            ret = WLAN_STATUS_FAILURE;
            kfree_skb(skb);
            goto end;
        }

        PRINTM(INFO, "Reading data in to skb size=%d\n", wlan_dev->upld_len);
        if (wlan_dev->upld_typ == MVSD_DAT) {
            PRINTM(INFO, "Up load type is Data\n");
            *ireg |= HIS_RxUpLdRdy;
            skb_put(skb, priv->wlan_dev.upld_len);
            skb_pull(skb, SDIO_HEADER_LEN);
            list_add_tail((struct list_head *) skb,
                          (struct list_head *) &priv->adapter->RxSkbQ);
        } else if (wlan_dev->upld_typ == MVSD_CMD) {
            *ireg &= ~(HIS_RxUpLdRdy);
            *ireg |= HIS_CmdUpLdRdy;

            /* take care of CurCmd = NULL case by reading the 
             * data to clear the interrupt */
            if (!priv->adapter->CurCmd) {
                cmdBuf = priv->wlan_dev.upld_buf;
                priv->adapter->HisRegCpy &= ~HIS_CmdUpLdRdy;
//                              *ireg &= ~HIS_RxUpLdRdy;
            } else {
                cmdBuf = priv->adapter->CurCmd->BufVirtualAddr;
            }

            priv->wlan_dev.upld_len -= SDIO_HEADER_LEN;
            memcpy(cmdBuf, skb->data + SDIO_HEADER_LEN,
                   MIN(MRVDRV_SIZE_OF_CMD_BUFFER, priv->wlan_dev.upld_len));
            kfree_skb(skb);
        } else if (wlan_dev->upld_typ == MVSD_EVENT) {
            *ireg |= HIS_CardEvent;
            kfree_skb(skb);
        }

        *ireg |= HIS_CmdDnLdRdy;
    }

    ret = WLAN_STATUS_SUCCESS;
  end:
    sbi_reenable_host_interrupt(priv, 0x00);
    LEAVE();
    return ret;
}

int
sbi_poll_cmd_dnld_rdy(wlan_private * priv)
{
    return mv_sdio_poll_card_status(priv, CARD_IO_READY | UP_LD_CARD_RDY);
}

int
sbi_card_to_host(wlan_private * priv, u32 type, u32 * nb, u8 * payload,
                 u16 npayload)
{
    return WLAN_STATUS_SUCCESS;
}

int
sbi_read_event_cause(wlan_private * priv)
{
    return WLAN_STATUS_SUCCESS;
}

int
mv_sdio_read_event_cause(wlan_private * priv)
{
    int ret;
    u8 scr2;

    ENTER();
    /* the SCRATCH_REG @ 0x8fc tells the cause for the Mac Event */
    if ((ret = sbi_read_ioreg(priv, FN0, 0x80fc, &scr2))
        < 0) {
        PRINTM(INFO, "Unable to read Event cause\n");
        return ret;
    }

    priv->adapter->EventCause = scr2;

    LEAVE();
    return 0;
}

int
sbi_retrigger(wlan_private * priv)
{

    ENTER();
    if (sbi_write_ioreg(priv, FN1, CONFIGURATION_REG, 0x4) < 0) {
        return -1;
    }
    LEAVE();
    return 0;
}

int
if_dnld_ready(wlan_private * priv)
{
    int rval;
    u8 cs;
    ENTER();
    rval = sbi_read_ioreg(priv, FN1, CARD_STATUS_REG, &cs);
    if (rval < 0)
        return -EBUSY;
    LEAVE();
    return (cs & DN_LD_CARD_RDY) && (cs & CARD_IO_READY);
}

int
sbi_is_tx_download_ready(wlan_private * priv)
{
    int rval;

    ENTER();
    rval = if_dnld_ready(priv);
    LEAVE();
    return (rval < 0) ? -1 : (rval == 1) ? 0 : -EBUSY;  //Check again
}

int
sbi_reenable_host_interrupt(wlan_private * priv, u8 bits)
{

    struct mss_slot *slot = (struct mss_slot *) (priv->wlan_dev.card);
    ENTER();
    mss_set_sdio_int(slot->host, MSS_SDIO_INT_EN);
    LEAVE();
    return WLAN_STATUS_SUCCESS;
}

/**  @brief This function disables the host interrupts mask.
 *
 *  @param priv    A pointer to wlan_private structure
 *  @param mask    the interrupt mask
 *  @return        WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
static int
disable_host_int_mask(wlan_private * priv, u8 mask)
{
    int ret = WLAN_STATUS_SUCCESS;
    u8 host_int_mask;

    ENTER();
    /* Read back the host_int_mask register */
    ret = sbi_read_ioreg(priv, FN1, HOST_INT_MASK_REG, &host_int_mask);
    if (ret < 0) {
        ret = WLAN_STATUS_FAILURE;
        goto done;
    }

    /* Update with the mask and write back to the register */
    host_int_mask &= ~mask;
    ret = sbi_write_ioreg(priv, FN1, HOST_INT_MASK_REG, host_int_mask);
    if (ret < 0) {
        PRINTM(WARN, "Unable to diable the host interrupt!\n");
        ret = WLAN_STATUS_FAILURE;
    }
  done:
    LEAVE();
    return ret;
}

/**
 *  @brief This function disables the host interrupts.
 *
 *  @param priv    A pointer to wlan_private structure
 *  @return        WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_disable_host_int(wlan_private * priv)
{
    return disable_host_int_mask(priv, HIM_DISABLE);
}

/**
 *  @brief This function enables the host interrupts mask
 *
 *  @param priv    A pointer to wlan_private structure
 *  @param mask    the interrupt mask
 *  @return        WLAN_STATUS_SUCCESS
 */
static int
enable_host_int_mask(wlan_private * priv, u8 mask)
{
    int ret = WLAN_STATUS_SUCCESS;

    ENTER();
    /* Simply write the mask to the register */
    ret = sbi_write_ioreg(priv, FN1, HOST_INT_MASK_REG, mask);

    if (ret < 0) {
        PRINTM(WARN, "ret = %d\n", ret);
        ret = WLAN_STATUS_FAILURE;
    }

    priv->adapter->HisRegCpy = 1;

    LEAVE();
    return ret;
}

/**
 *  @brief This function enables the host interrupts.
 *
 *  @param priv    A pointer to wlan_private structure
 *  @return        WLAN_STATUS_SUCCESS
 */
int
sbi_enable_host_int(wlan_private * priv)
{
    return enable_host_int_mask(priv, HIM_ENABLE);
}

int
sbi_unregister_dev(wlan_private * priv)
{
    ENTER();

    if (priv->wlan_dev.card != NULL) {
        /* Release the SDIO IRQ */
        //sdio_free_irq(priv->wlan_dev.card, priv->wlan_dev.netdev);
        PRINTM(WARN, "Making the sdio dev card as NULL\n");
    }
    LEAVE();
    return WLAN_STATUS_SUCCESS;
}

int
mv_sdio_poll_card_status(wlan_private * priv, u8 bits)
{
    int tries;
    int rval;
    u8 cs;

    ENTER();
    for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
        rval = sbi_read_ioreg(priv, FN1, CARD_STATUS_REG, &cs);
        PRINTM(INFO, "rval = %x\n cs&bits =%x\n", rval, (cs & bits));
        if (rval == 0 && (cs & bits) == bits) {
            return 0;
        }

        udelay(100);
    }

    PRINTM(INFO, "mv_sdio_poll_card_status: FAILED!\n");
    LEAVE();
    return -EBUSY;
}

int
sbi_register_dev(wlan_private * priv)
{
    int ret = WLAN_STATUS_SUCCESS;
    u8 reg;
    ENTER();

    the_netdev = priv->wlan_dev.netdev;

    /* Initialize the private structure */
    strncpy(priv->wlan_dev.name, "sdio0", sizeof(priv->wlan_dev.name));
    priv->wlan_dev.ioport = 0;
    priv->wlan_dev.upld_rcv = 0;
    priv->wlan_dev.upld_typ = 0;
    priv->wlan_dev.upld_len = 0;

    /* Read the IO port */
    ret = sbi_read_ioreg(priv, FN1, IO_PORT_0_REG, &reg);
    if (ret)
        goto failed;
    else
        priv->wlan_dev.ioport |= reg;

    ret = sbi_read_ioreg(priv, FN1, IO_PORT_1_REG, &reg);
    if (ret)
        goto failed;
    else
        priv->wlan_dev.ioport |= (reg << 8);

    ret = sbi_read_ioreg(priv, FN1, IO_PORT_2_REG, &reg);
    if (ret)
        goto failed;
    else
        priv->wlan_dev.ioport |= (reg << 16);

    PRINTM(INFO, "SDIO FUNC1 IO port: 0x%x\n", priv->wlan_dev.ioport);

    /* Disable host interrupt first. */
    if ((ret = disable_host_int_mask(priv, 0xff)) < 0) {
        PRINTM(WARN, "Warning: unable to disable host interrupt!\n");
    }
#if 0                           //marked by xc, we have init it
    /* Request the SDIO IRQ */
    PRINTM(INFO, "Before request_irq Address is if==>%p\n", isr_function);
    ret = sdio_request_irq(priv->wlan_dev.card,
                           (handler_fn_t) isr_function, 0,
                           "sdio_irq", priv->wlan_dev.netdev);

    PRINTM(INFO, "IrqLine: %d\n", card->ctrlr->tmpl->irq_line);

    if (ret < 0) {
        PRINTM(INFO, "Failed to request IRQ on SDIO bus (%d)\n", ret);
        goto failed;
    }
#endif
    //mod by xc
#if 0
    priv->wlan_dev.netdev->irq = card->ctrlr->tmpl->irq_line;
    priv->adapter->irq = priv->wlan_dev.netdev->irq;
    priv->adapter->chip_rev = card->chiprev;
#endif

    priv->hotplug_device =
        &((struct mss_slot *) (priv->wlan_dev.card))->card->dev;
    if (helper_name == NULL) {
        helper_name = DEFAULT_HELPER_NAME;
    }
    if (fw_name == NULL) {
        fw_name = DEFAULT_FW_NAME;
    }
    pwlanpriv = priv;
    return WLAN_STATUS_SUCCESS; /* success */

  failed:
    PRINTM(INFO, "register device fail\n");
    priv->wlan_dev.card = NULL;

    LEAVE();
    return WLAN_STATUS_FAILURE;
}

/*
 * This fuction is used for sending data to the SDIO card.
 */

int
sbi_host_to_card(wlan_private * priv, u8 type, u8 * payload, u16 nb)
{
    int ret = WLAN_STATUS_SUCCESS;
    int buf_block_len;
    int blksz;

    ENTER();

    priv->adapter->HisRegCpy = 0;

    blksz = SD_BLOCK_SIZE;
    buf_block_len = (nb + SDIO_HEADER_LEN + blksz - 1) / blksz;

    priv->adapter->TmpTxBuf[0] = (nb + SDIO_HEADER_LEN) & 0xff;
    priv->adapter->TmpTxBuf[1] = ((nb + SDIO_HEADER_LEN) >> 8) & 0xff;
    priv->adapter->TmpTxBuf[2] = type;
    priv->adapter->TmpTxBuf[3] = 0x0;

    if (payload != NULL && nb > 0) {
        if (type == MVMS_CMD)
            memcpy(&priv->adapter->TmpTxBuf[SDIO_HEADER_LEN], payload, nb);
    } else {
        PRINTM(WARN, "sbi_host_to_card(): Error: payload=%p, nb=%d\n",
               payload, nb);
    }

    /* The host polls for the IO_READY bit */
    ret = mv_sdio_poll_card_status(priv, CARD_IO_READY);
    if (ret < 0) {
        PRINTM(INFO, "<1> Poll failed in host_to_card : %d\n", ret);
        ret = WLAN_STATUS_FAILURE;
        goto exit;
    }

    /* Transfer data to card */
    ret = sbi_write_iomem(priv, FN1, priv->wlan_dev.ioport,
                          BLOCK_MODE, FIXED_ADDRESS, buf_block_len,
                          blksz, priv->adapter->TmpTxBuf);

    if (ret < 0) {
        PRINTM(WARN, "sdio_write_iomem failed: ret=%d\n", ret);
        ret = WLAN_STATUS_FAILURE;
        goto exit;
    } else {
        PRINTM(INFO, "sdio write -dnld val =>%d\n", ret);
    }

    if (type == MVSD_DAT)
        priv->wlan_dev.dnld_sent = DNLD_DATA_SENT;
    else
        priv->wlan_dev.dnld_sent = DNLD_CMD_SENT;
  exit:
    LEAVE();
    return ret;
}

/*
 * This function is used to read data from the card.
 */

int
mv_sdio_card_to_host(wlan_private * priv,
                     u32 * type, int *nb, u8 * payload, int npayload)
{
    int ret = WLAN_STATUS_SUCCESS;
    u16 buf_len = 0;
    int buf_block_len;
    int blksz;
    u32 *pevent;

    ENTER();
    if (!payload) {
        PRINTM(WARN, "payload NULL pointer received!\n");
        ret = WLAN_STATUS_FAILURE;
        goto exit;
    }

    /* Read the length of data to be transferred */
    ret = mv_sdio_read_scratch(priv, &buf_len);
    if (ret < 0) {
        PRINTM(WARN, "Failed to read the scratch reg\n");
        ret = WLAN_STATUS_FAILURE;
        goto exit;
    }

    PRINTM(INFO, "Receiving %d bytes from card at scratch reg value\n",
           buf_len);
    if (buf_len - SDIO_HEADER_LEN <= 0 || buf_len > npayload) {
        PRINTM(WARN, "Invalid packet size from firmware, size = %d\n",
               buf_len);
        ret = WLAN_STATUS_FAILURE;
        goto exit;
//              if (buf_len > npayload + 4)
//                      buf_len = npayload + 4;
    }

    /* Allocate buffer */
    blksz = SD_BLOCK_SIZE;
    buf_block_len = (buf_len + blksz - 1) / blksz;

    /* The host polls for the IO_READY bit */
    ret = mv_sdio_poll_card_status(priv, CARD_IO_READY);
    if (ret < 0) {
        PRINTM(WARN, "<1> Poll failed in card_to_host : %d\n", ret);
        ret = WLAN_STATUS_FAILURE;
        goto exit;
    }

    ret = sbi_read_iomem(priv, FN1, priv->wlan_dev.ioport,
                         BLOCK_MODE, FIXED_ADDRESS, buf_block_len,
                         blksz, payload);

    if (ret < 0) {
        PRINTM(INFO, "<1>sdio_read_iomem failed - mv_sdio_card_to_host\n");
        ret = WLAN_STATUS_FAILURE;
        goto exit;
    }
    *nb = buf_len;

    if (*nb <= 0) {
        PRINTM(INFO, "Null packet recieved \n");
        ret = WLAN_STATUS_FAILURE;
        goto exit;
    }

    *type = (payload[2] | (payload[3] << 8));
    if (*type == MVSD_EVENT) {
        pevent = (u32 *) & payload[4];
        priv->adapter->EventCause = MVSD_EVENT | (((u16) (*pevent)) << 3);
    }
  exit:
    LEAVE();
    return ret;
}

/*
 * Read from the special scratch 'port'.
 */

int
mv_sdio_read_scratch(wlan_private * priv, u16 * dat)
{
    int ret = WLAN_STATUS_SUCCESS;
    u8 scr0;
    u8 scr1;

    ENTER();
    ret = sbi_read_ioreg(priv, FN1, CARD_OCR_0_REG, &scr0);
    if (ret < 0)
        return WLAN_STATUS_FAILURE;
    PRINTM(INFO, "SCRATCH_0_REG = %x\n", scr0);

    ret = sbi_read_ioreg(priv, FN1, CARD_OCR_1_REG, &scr1);
    if (ret < 0)
        return WLAN_STATUS_FAILURE;
    PRINTM(INFO, "SCRATCH_1_REG = %x\n", scr1);

    *dat = (((u16) scr1) << 8) | scr0;
    LEAVE();
    return WLAN_STATUS_SUCCESS;
}

/*
 * 	Get the CIS Table 
 */
int
sbi_get_cis_info(wlan_private * priv)
{
    wlan_adapter *Adapter = priv->adapter;
    u8 tupledata[255];
    char cisbuf[512];
    int ofs = 0, i;
    u32 ret = WLAN_STATUS_SUCCESS;
    struct mss_slot *slot = (struct mss_slot *) (priv->wlan_dev.card);
    struct sdio_card *card_info;

    ENTER();
    card_info = (struct sdio_card *) slot->card->prot_card;
    ret = card_info->ccis.manufacturer;

    /* Read the Tuple Data */
    for (i = 0; i < sizeof(tupledata); i++) {
        ret = sbi_read_ioreg(priv, FN0, card_info->cccr.pccis + i,
                             &tupledata[i]);
        if (ret < 0) {
            PRINTM(WARN, "sbi_get_cis_info failed!!!\n");
            return WLAN_STATUS_FAILURE;
        }
    }

    memset(cisbuf, 0x0, sizeof(cisbuf));
    memcpy(cisbuf + ofs, tupledata, sizeof(cisbuf));

    /* Copy the CIS Table to Adapter */
    memset(Adapter->CisInfoBuf, 0x0, sizeof(cisbuf));
    memcpy(Adapter->CisInfoBuf, &cisbuf, sizeof(cisbuf));
    Adapter->CisInfoLen = sizeof(cisbuf);

    LEAVE();
    return WLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function downloads firmware image to the card.
 *
 *  @param priv         A pointer to wlan_private structure
 *  @param firmware     A pointer to firmware image buffer
 *  @param firmwarelen  the length of firmware image
 *  @return             WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
static int
sbi_download_wlan_fw_image(wlan_private * priv,
                           const u8 * firmware, int firmwarelen)
{
    u8 base0;
    u8 base1;
    int ret = WLAN_STATUS_SUCCESS;
    int offset;
    u8 *fwbuf = priv->adapter->TmpTxBuf;
    int timeout = 5000;
    u16 len;
    int txlen = 0;
    int tx_blocks = 0;
#ifdef FW_DOWNLOAD_SPEED
    u32 tv1, tv2;
#endif

    ENTER();

    PRINTM(INFO, "WLAN_FW: Downloading firmware of size %d bytes\n",
           firmwarelen);
#ifdef FW_DOWNLOAD_SPEED
    tv1 = get_utimeofday();
#endif

    /* Wait initially for the first non-zero value */
    do {
        if ((ret = sbi_read_ioreg(priv, FN1, HOST_F1_RD_BASE_0, &base0)) < 0) {
            PRINTM(WARN, "Dev BASE0 register read failed:"
                   " base0=0x%04X(%d)\n", base0, base0);
            ret = WLAN_STATUS_FAILURE;
            goto done;
        }
        if ((ret = sbi_read_ioreg(priv, FN1, HOST_F1_RD_BASE_1, &base1)) < 0) {
            PRINTM(WARN, "Dev BASE1 register read failed:"
                   " base1=0x%04X(%d)\n", base1, base1);
            ret = WLAN_STATUS_FAILURE;
            goto done;
        }
        len = (((u16) base1) << 8) | base0;
        mdelay(1);
    } while (!len && --timeout);

    if (!timeout) {
        PRINTM(MSG, "Helper downloading finished.\n");
        PRINTM(MSG, "Timeout for firmware downloading!\n");
        ret = WLAN_STATUS_FAILURE;
        goto done;
    }

    /* The host polls for the DN_LD_CARD_RDY and IO_READY bits */
    ret = mv_sdio_poll_card_status(priv, CARD_IO_READY | DN_LD_CARD_RDY);
    if (ret < 0) {
        PRINTM(FATAL, "Firmware download died @ the end\n");
        ret = WLAN_STATUS_FAILURE;
        goto done;
    }

    PRINTM(INFO, "WLAN_FW: Len got from firmware = 0x%04X(%d)\n", len, len);
    len &= ~B_BIT_0;

    /* Perform firmware data transfer */
    for (offset = 0; offset < firmwarelen; offset += txlen) {
        txlen = len;

        /* Set blocksize to transfer - checking for last block */
        if (firmwarelen - offset < txlen) {
            txlen = firmwarelen - offset;
        }
        PRINTM(INFO, "WLAN_FW: offset=%d, txlen = 0x%04X(%d)\n",
               offset, txlen, txlen);

        /* The host polls for the DN_LD_CARD_RDY and IO_READY bits */
        ret = mv_sdio_poll_card_status(priv, CARD_IO_READY | DN_LD_CARD_RDY);
        if (ret < 0) {
            PRINTM(FATAL, "Firmware download died @ %d\n", offset);
            goto done;
        }

        tx_blocks = (txlen + SD_BLOCK_SIZE_FW_DL - 1) / SD_BLOCK_SIZE_FW_DL;
        PRINTM(INFO, "WLAN_FW:     tx_blocks = 0x%04X(%d)\n",
               tx_blocks, tx_blocks);

        /* Copy payload to buffer */
        memcpy(fwbuf, &firmware[offset], txlen);

        /* Send data */
        ret = sbi_write_iomem(priv, FN1,
                              priv->wlan_dev.ioport, BLOCK_MODE,
                              FIXED_ADDRESS, tx_blocks, SD_BLOCK_SIZE_FW_DL,
                              fwbuf);

        if (ret < 0) {
            PRINTM(FATAL, "IO error:transferring @ %d\n", offset);
            goto done;
        }

        do {
            udelay(10);
            if ((ret = sbi_read_ioreg(priv, FN1,
                                      HOST_F1_CARD_RDY, &base0)) < 0) {
                PRINTM(WARN, "Dev CARD_RDY register read failed:"
                       " base0=0x%04X(%d)\n", base0, base0);
                ret = WLAN_STATUS_FAILURE;
                goto done;
            }
            PRINTM(INFO, "offset=0x%08X len=0x%04X: FN1,"
                   "HOST_F1_CARD_RDY: 0x%04X\n", offset, txlen, base0);
        } while (!(base0 & 0x08) || !(base0 & 0x01));

        if ((ret = sbi_read_ioreg(priv, FN1, HOST_F1_RD_BASE_0, &base0)) < 0) {
            PRINTM(WARN, "Dev BASE0 register read failed:"
                   " base0=0x%04X(%d)\n", base0, base0);
            ret = WLAN_STATUS_FAILURE;
            goto done;
        }
        if ((ret = sbi_read_ioreg(priv, FN1, HOST_F1_RD_BASE_1, &base1)) < 0) {
            PRINTM(WARN, "Dev BASE1 register read failed:"
                   " base1=0x%04X(%d)\n", base1, base1);
            ret = WLAN_STATUS_FAILURE;
            goto done;
        }
        len = (((u16) base1) << 8) | base0;

        if (!len) {
            PRINTM(INFO, "WLAN Firmware Download Over\n");
            break;
        }

        if (len & B_BIT_0) {
            PRINTM(INFO, "CRC32 Error indicated by the helper:"
                   " len=0x%04X(%d)\n", len, len);
            len &= ~B_BIT_0;
            /* Setting this to 0 to resend from same offset */
            txlen = 0;
        } else {
            PRINTM(INFO, "%d,%d bytes block of firmware downloaded\n",
                   offset, txlen);
        }
    }

    PRINTM(INFO, "Firmware Image of Size %d bytes downloaded\n", firmwarelen);

    ret = WLAN_STATUS_SUCCESS;
  done:
#ifdef FW_DOWNLOAD_SPEED
    tv2 = get_utimeofday();
    PRINTM(INFO, "firmware: %ld.%03ld.%03ld ", tv1 / 1000000,
           (tv1 % 1000000) / 1000, tv1 % 1000);
    PRINTM(INFO, " -> %ld.%03ld.%03ld ", tv2 / 1000000,
           (tv2 % 1000000) / 1000, tv2 % 1000);
    tv2 -= tv1;
    PRINTM(INFO, " == %ld.%03ld.%03ld\n", tv2 / 1000000,
           (tv2 % 1000000) / 1000, tv2 % 1000);
#endif
    LEAVE();
    return ret;
}

static int
sbi_prog_firmware_image(wlan_private * priv, const u8 * firmware,
                        int firmwarelen)
{
    int ret = WLAN_STATUS_SUCCESS;
    u16 firmwarestat;
    u8 *fwbuf = priv->adapter->TmpTxBuf;
    int fwblknow;
    u32 tx_len;
#ifdef FW_DOWNLOAD_SPEED
    unsigned long tv1, tv2;
#endif

    ENTER();

    /* Check if the firmware is already downloaded */
    if ((ret = mv_sdio_read_scratch(priv, &firmwarestat)) < 0) {
        PRINTM(INFO, "read scratch returned <0\n");
        goto done;
    }

    if (firmwarestat == FIRMWARE_READY) {
        PRINTM(INFO, "Firmware already downloaded!\n");
        /* TODO: We should be returning success over here */
        ret = WLAN_STATUS_SUCCESS;
        goto done;
    }

    PRINTM(INFO, "Downloading helper image (%d bytes), block size %d bytes ",
           firmwarelen, SD_BLOCK_SIZE_FW_DL);

#ifdef FW_DOWNLOAD_SPEED
    tv1 = get_utimeofday();
#endif
    /* Perform firmware data transfer */
    tx_len =
        (FIRMWARE_TRANSFER_NBLOCK * SD_BLOCK_SIZE_FW_DL) - SDIO_HEADER_LEN;
    for (fwblknow = 0; fwblknow < firmwarelen; fwblknow += tx_len) {

        /* The host polls for the DN_LD_CARD_RDY and IO_READY bits */
        ret = mv_sdio_poll_card_status(priv, CARD_IO_READY | DN_LD_CARD_RDY);
        if (ret < 0) {
            PRINTM(INFO, "Firmware download died @ %d\n", fwblknow);
            goto done;
        }

        /* Set blocksize to transfer - checking for last block */
        if (firmwarelen - fwblknow < tx_len)
            tx_len = firmwarelen - fwblknow;

        fwbuf[0] = ((tx_len & 0x000000ff) >> 0);        /* Little-endian */
        fwbuf[1] = ((tx_len & 0x0000ff00) >> 8);
        fwbuf[2] = ((tx_len & 0x00ff0000) >> 16);
        fwbuf[3] = ((tx_len & 0xff000000) >> 24);

        /* Copy payload to buffer */
        memcpy(&fwbuf[SDIO_HEADER_LEN], &firmware[fwblknow], tx_len);

        PRINTM(INFO, ".");

        /* Send data */
        ret = sbi_write_iomem(priv, FN1,
                              priv->wlan_dev.ioport, BLOCK_MODE,
                              FIXED_ADDRESS, FIRMWARE_TRANSFER_NBLOCK,
                              SD_BLOCK_SIZE_FW_DL, fwbuf);

        if (ret) {
            PRINTM(INFO, "IO error: transferring block @ %d\n", fwblknow);
            goto done;
        }
    }

    PRINTM(INFO, "\ndone (%d/%d bytes)\n", fwblknow, firmwarelen);
#ifdef FW_DOWNLOAD_SPEED
    tv2 = get_utimeofday();
    PRINTM(INFO, "helper: %ld.%03ld.%03ld ", tv1 / 1000000,
           (tv1 % 1000000) / 1000, tv1 % 1000);
    PRINTM(INFO, " -> %ld.%03ld.%03ld ", tv2 / 1000000,
           (tv2 % 1000000) / 1000, tv2 % 1000);
    tv2 -= tv1;
    PRINTM(INFO, " == %ld.%03ld.%03ld\n", tv2 / 1000000,
           (tv2 % 1000000) / 1000, tv2 % 1000);
#endif

    /* Write last EOF data */
    PRINTM(INFO, "Transferring EOF block\n");
    memset(fwbuf, 0x0, SD_BLOCK_SIZE_FW_DL);
    ret = sbi_write_iomem(priv, FN1,
                          priv->wlan_dev.ioport, BLOCK_MODE,
                          FIXED_ADDRESS, 1, SD_BLOCK_SIZE_FW_DL, fwbuf);

    if (ret < 0) {
        PRINTM(INFO, "IO error in writing EOF firmware block\n");
        goto done;
    }

    ret = WLAN_STATUS_SUCCESS;

  done:
    LEAVE();
    return ret;
}

int
sbi_prog_helper(wlan_private * priv)
{
    if (priv->fw_helper) {
        return sbi_prog_firmware_image(priv,
                                       priv->fw_helper->data,
                                       priv->fw_helper->size);
    } else {
        PRINTM(MSG, "No hotplug helper image\n");
        return WLAN_STATUS_FAILURE;
    }
}

int
sbi_prog_firmware_w_helper(wlan_private * priv)
{
    if (priv->firmware) {
        return sbi_download_wlan_fw_image(priv,
                                          priv->firmware->data,
                                          priv->firmware->size);
    } else {
        PRINTM(MSG, "No hotplug firmware image\n");
        return WLAN_STATUS_FAILURE;
    }
}

int
sbi_prog_firmware(wlan_private * priv)
{
    if (priv->firmware) {
        return sbi_download_wlan_fw_image(priv,
                                          priv->firmware->data,
                                          priv->firmware->size);
    } else {
        PRINTM(MSG, "No hotplug firmware image\n");
        return WLAN_STATUS_FAILURE;
    }
}

int
sbi_set_bus_clock(wlan_private * priv, u8 option)
{
    return WLAN_STATUS_SUCCESS;
}

int
sbi_exit_deep_sleep(wlan_private * priv)
{
    int ret = WLAN_STATUS_SUCCESS;
    PRINTM(INFO,
           "Trying to wakeup device... Conn=%d IntC=%d PS_Mode=%d PS_State=%d\n",
           priv->adapter->MediaConnectStatus, priv->adapter->IntCounter,
           priv->adapter->PSMode, priv->adapter->PSState);
//      sbi_set_bus_clock(priv, TRUE);

    if (priv->adapter->fwWakeupMethod == WAKEUP_FW_THRU_GPIO) {
//              GPIO_PORT_TO_LOW();
    } else
        ret = sbi_write_ioreg(priv, FN1, CONFIGURATION_REG, HOST_POWER_UP);

    return ret;
}

int
sbi_reset_deepsleep_wakeup(wlan_private * priv)
{

    int ret = WLAN_STATUS_SUCCESS;

    ENTER();

    if (priv->adapter->fwWakeupMethod == WAKEUP_FW_THRU_GPIO) {
        GPIO_PORT_TO_HIGH();
    } else
        ret = sbi_write_ioreg(priv, FN1, CONFIGURATION_REG, 0);

    LEAVE();

    return ret;
}

int
sbi_verify_fw_download(wlan_private * priv)
{
    int ret;
    u16 firmwarestat;
    int tries;
    u8 rsr;

    ENTER();
    /* Wait for firmware initialization event */
    for (tries = 0; tries < MAX_FIRMWARE_POLL_TRIES; tries++) {
        if ((ret = mv_sdio_read_scratch(priv, &firmwarestat)) < 0)
            continue;

        if (firmwarestat == FIRMWARE_READY) {
            ret = 0;
            PRINTM(INFO, "Firmware successfully downloaded\n");
            break;
        } else {
            mdelay(10);
            ret = -ETIMEDOUT;
        }
    }

    if (ret < 0) {
        PRINTM(INFO, "Timeout waiting for firmware to become active\n");
        goto done;
    }

    ret = sbi_read_ioreg(priv, FN1, HOST_INT_RSR_REG, &rsr);
    if (ret < 0) {
        PRINTM(INFO, "sdio_read_ioreg: reading INT RSR register failed\n");
        return -1;
    } else
        PRINTM(INFO, "sdio_read_ioreg: RSR register 0x%x\n", rsr);

    ret = 0;
  done:
    LEAVE();
    return ret;
}

#ifdef DEEP_SLEEP_XC

//mod by xc

/*extern int start_bus_clock(mmc_controller_t);
extern int stop_bus_clock_2(mmc_controller_t);
*/
int
sbi_enter_deep_sleep(wlan_private * priv)
{
    int ret;

    sbi_write_ioreg(priv, FN1, CONFIGURATION_REG, 0);
    mdelay(2);
    ret = sbi_write_ioreg(priv, FN1, CONFIGURATION_REG, HOST_POWER_DOWN);
//mod by xc
    //stop_bus_clock_2(((mmc_card_t)((priv->wlan_dev).card))->ctrlr);
    mdelay(2);

    return ret;
}

int
sbi_exit_deep_sleep(wlan_private * priv)
{
    int ret = 0;

    PRINTM(INFO,
           "Trying to wakeup device... Conn=%d IntC=%d PS_Mode=%d PS_State=%d\n",
           priv->adapter->MediaConnectStatus, priv->adapter->IntCounter,
           priv->adapter->PSMode, priv->adapter->PSState);
    //mod by xc
    //start_bus_clock(((mmc_card_t)((priv->wlan_dev).card))->ctrlr);

    if (priv->adapter->fwWakeupMethod == WAKEUP_FW_THRU_GPIO) {
        GPIO_PORT_TO_LOW();
    } else                      // SDIO method 
        ret = sbi_write_ioreg(priv, FN1, CONFIGURATION_REG, HOST_POWER_UP);

    return ret;
}

int
sbi_reset_deepsleep_wakeup(wlan_private * priv)
{
    ENTER();

    int ret = 0;

    if (priv->adapter->fwWakeupMethod == WAKEUP_FW_THRU_GPIO) {
        GPIO_PORT_TO_HIGH();
    } else                      // SDIO method 
        ret = sbi_write_ioreg(priv, FN1, CONFIGURATION_REG, 0);

    LEAVE();

    return ret;
}
#endif /* DEEP_SLEEP */

#ifdef CONFIG_MARVELL_PM
inline int
sbi_suspend(wlan_private * priv)
{
    int ret;

    ENTER();

    ret = sdio_suspend(priv->wlan_dev.card);

    LEAVE();
    return ret;
}

inline int
sbi_resume(wlan_private * priv)
{
    int ret;

    ENTER();

    ret = sdio_resume(priv->wlan_dev.card);

    LEAVE();
    return ret;
}
#endif

MODULE_LICENSE("GPL");

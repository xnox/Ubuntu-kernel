/** @file wlan_sdio.c
 *  @brief This file contains SDIO IF (interface) module
 *  related functions.
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
/****************************************************
Change log:
	10/14/05: add Doxygen format comments 
	01/05/06: add kernel 2.6.x support
	01/23/06: add fw downlaod
	06/06/06: add macro SD_BLOCK_SIZE_FW_DL for firmware download
		  add macro ALLOC_BUF_SIZE for cmd resp/Rx data skb buffer allocation
****************************************************/

#include	"wlan_sdio.h"

#include <linux/firmware.h>

/** define SD block size for firmware download */
#define SD_BLOCK_SIZE_FW_DL	32

/** define SD block size for data Tx/Rx */
#define SD_BLOCK_SIZE		256

/** define allocated buffer size */
#define ALLOC_BUF_SIZE		(((MAX(MRVDRV_ETH_RX_PACKET_BUFFER_SIZE, \
					MRVDRV_SIZE_OF_CMD_BUFFER) + SDIO_HEADER_LEN \
					+ SD_BLOCK_SIZE - 1) / SD_BLOCK_SIZE) * SD_BLOCK_SIZE)

/** Max retry number of CMD53 write */
#define MAX_WRITE_IOMEM_RETRY	2

/* The macros below are hardware platform dependent.
   The definition should match the actual platform */
#define GPIO_PORT_INIT()
/** Set GPIO port to high */
#define GPIO_PORT_TO_HIGH()
/** Set GPIO port to low */
#define GPIO_PORT_TO_LOW()

/********************************************************
		Local Variables
********************************************************/

static wlan_private *pwlanpriv;
static wlan_private *(*wlan_add_callback) (void *dev_id);
static int (*wlan_remove_callback) (void *dev_id);

/** SDIO func 1 */
static u8 sdio_func_wlan = SDIO_FUNC1;

/** SDIO Rx unit */
static u8 sdio_rx_unit = 0;

/********************************************************
		Global Variables
********************************************************/

#define SDIO_CLK_RATE_6MHZ	6       /**< 6 MHz */
#define SDIO_CLK_RATE_12MHZ	12      /**< 12 MHz */
#define SDIO_CLK_RATE_25MHZ	25      /**< 25 MHz */
#define SDIO_CLK_RATE_DEFAULT	0   /**< Default rate */
/** SDIO clock rate */
int clkrate = SDIO_CLK_RATE_DEFAULT;
module_param(clkrate, int, 0);

/** ISR */
//sd_int_handler isr_function;
//int request_gpio_irq_callback(void (*callback) (void *), void *arg);
//int release_gpio_irq_callback(void (*callback) (void *), void *arg);

/** Chip ID for 8686 */
#define CHIP_ID_8686 0x3042
/** Chip ID for 8688 */
#define CHIP_ID_8688 0x3130
/** Chip ID for 8688R3 */
#define CHIP_ID_8688R3 0x3131
/** Chip ID for 8682 */
#define CHIP_ID_8682 0x3139
/** Null chip ID */
#define CHIP_ID_NULL 0x0000


/** Default helper name */
#define DEFAULT_HELPER_NAME "mrvl/helper_sd.bin"
/** Maximum length for firmware name */
#define MAX_FW_NAME 32

typedef struct _chipid_fwname
{
    /** Chip ID */
    u16 chip_id;
    /** Firmware filename */
    char fw_name[MAX_FW_NAME];
} chipid_fwname;

static chipid_fwname chip_fw[] = {
    {.chip_id = CHIP_ID_8688,.fw_name = "mrvl/sd8688.bin"},
    {.chip_id = CHIP_ID_8686,.fw_name = "mrvl/sd8686.bin"},
    {.chip_id = CHIP_ID_8688R3,.fw_name = "mrvl/sd8688.bin"},
    {.chip_id = CHIP_ID_8682,.fw_name = "mrvl/sd8682.bin"},
    {.chip_id = CHIP_ID_NULL,.fw_name = ""},    /* must be the last one */
};

extern u8 *helper_name;
extern u8 *fw_name;
/********************************************************
		Local Functions
********************************************************/

/** 
 *  @brief This function removes the card
 *  
 *  @param card    A pointer to the card
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */

static inline u16
get_chip_id(wlan_private * priv)
{
    u8 id0;
    u8 id1;
    u16 chip_id;

    if (sbi_read_ioreg(priv, CARD_CHIP_ID_0_REG, &id0) < 0)
        return CHIP_ID_NULL;

    if (sbi_read_ioreg(priv, CARD_CHIP_ID_1_REG, &id1) < 0)
        return CHIP_ID_NULL;

    chip_id = (((u16) id0) << 8) | id1;
    PRINTM(INFO, "Chip ID %#x\n", chip_id);

    return chip_id;
}

static inline int
get_enhance_flag(wlan_private * priv)
{
    u16 chip_id;

    chip_id = get_chip_id(priv);
    if ((chip_id == CHIP_ID_8688) || (chip_id == CHIP_ID_8682))
        return TRUE;
    else
        return FALSE;
}

static inline char *
find_fw_name(wlan_private * priv)
{
    u16 chip_id;
    int i = 0;

    chip_id = get_chip_id(priv);

    while (chip_id && (chip_fw[i].chip_id != CHIP_ID_NULL)) {
        if (chip_fw[i].chip_id == chip_id) {
            return chip_fw[i].fw_name;
        }
        i++;
    }
    PRINTM(ERROR, "Unknown Chip ID %#x\n", chip_id);
    return NULL;
}

/* some tool function */
int sbi_read_iomem(wlan_private *priv, void *buf, unsigned int reg_addr, int cnt)
{
	struct sdio_mmc_card *card = priv->wlan_dev.card;
	int ret = 0;

	sdio_claim_host(card->func);
	ret = sdio_readsb(card->func, buf, reg_addr, cnt);
	sdio_release_host(card->func);

	return ret;
}

int sbi_write_iomem(wlan_private *priv, void *buf, unsigned int reg_addr, int cnt)
{
	struct sdio_mmc_card *card = priv->wlan_dev.card;
	int ret = 0;

	sdio_claim_host(card->func);
	ret = sdio_writesb(card->func, reg_addr, buf, cnt);
	sdio_release_host(card->func);

	return ret;
}

/** 
 *  @brief This function get rx_unit value
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sd_get_rx_unit(wlan_private * priv)
{
    int ret = WLAN_STATUS_SUCCESS;
    u8 reg;
    ret = sbi_read_ioreg(priv, CARD_RX_UNIT_REG, &reg);
    if (ret == WLAN_STATUS_SUCCESS)
        sdio_rx_unit = reg;
    return ret;
}

/** 
 *  @brief This function reads rx length
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @param dat	   A pointer to keep returned data
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
static int
sd_read_rx_len(wlan_private * priv, u16 * dat)
{
    int ret = WLAN_STATUS_SUCCESS;
    u8 reg;
    ret = sbi_read_ioreg(priv, CARD_RX_LEN_REG, &reg);
    if (ret == WLAN_STATUS_SUCCESS) {
        *dat = (u16) reg << sdio_rx_unit;
    }
    return ret;
}

/** 
 *  @brief This function reads fw status registers
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @param dat	   A pointer to keep returned data
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
static int
sd_read_firmware_status(wlan_private * priv, u16 * dat)
{
    int ret = WLAN_STATUS_SUCCESS;
    u8 fws0;
    u8 fws1;
    ENTER();
    ret = sbi_read_ioreg(priv, CARD_FW_STATUS0_REG, &fws0);
    if (ret < 0)
        return WLAN_STATUS_FAILURE;
    ret = sbi_read_ioreg(priv, CARD_FW_STATUS1_REG, &fws1);
    if (ret < 0)
        return WLAN_STATUS_FAILURE;
    *dat = (((u16) fws1) << 8) | fws0;
    LEAVE();
    return WLAN_STATUS_SUCCESS;
}

/** 
 *  @brief This function reads scratch registers
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @param dat	   A pointer to keep returned data
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
static int
mv_sdio_read_scratch(wlan_private * priv, u16 * dat)
{
    int ret = WLAN_STATUS_SUCCESS;
    u8 scr0;
    u8 scr1;

    ret = sbi_read_ioreg(priv, CARD_OCR_0_REG, &scr0);
    if (ret < 0)
        return WLAN_STATUS_FAILURE;

    ret = sbi_read_ioreg(priv, CARD_OCR_1_REG, &scr1);
    PRINTM(INFO, "CARD_OCR_0_REG = 0x%x, CARD_OCR_1_REG = 0x%x\n", scr0,
           scr1);
    if (ret < 0)
        return WLAN_STATUS_FAILURE;

    *dat = (((u16) scr1) << 8) | scr0;

    return WLAN_STATUS_SUCCESS;
}

/** 
 *  @brief This function polls the card status register.
 *  
 *  @param priv    	A pointer to wlan_private structure
 *  @param bits    	the bit mask
 *  @return 	   	WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
static int
mv_sdio_poll_card_status(wlan_private * priv, u8 bits)
{
    int tries;
    u8 cs;

    for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
        if (sbi_read_ioreg(priv, CARD_STATUS_REG, &cs) < 0)
            break;
        else if ((cs & bits) == bits)
            return WLAN_STATUS_SUCCESS;
        udelay(10);
    }

    PRINTM(WARN, "mv_sdio_poll_card_status failed, tries = %d\n", tries);
    return WLAN_STATUS_FAILURE;
}

/** 
 *  @brief This function set the sdio bus width.
 *  
 *  @param priv    	A pointer to wlan_private structure
 *  @param mode    	1--1 bit mode, 4--4 bit mode
 *  @return 	   	WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sdio_set_bus_width(wlan_private * priv, u8 mode)
{
    return WLAN_STATUS_SUCCESS;
}














/*
 *  @brief This function reads data from the card.
 *  
 *  @param priv    	A pointer to wlan_private structure
 *  @param type	   	A pointer to keep type as data or command
 *  @param nb		A pointer to keep the data/cmd length retured in buffer
 *  @param payload 	A pointer to the data/cmd buffer
 *  @param nb	   	the length of data/cmd buffer
 *  @param npayload	the length of data/cmd buffer
 *  @return 	   	WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
/* external lock needed */
static int
mv_sdio_card_to_host(wlan_private * priv,
                     u32 * type, int *nb, u8 * payload, int npayload)
{
    struct sdio_mmc_card *card = priv->wlan_dev.card;
    int ret = WLAN_STATUS_SUCCESS;
    u16 buf_len = 0;
    int buf_block_len;
    int blksz;
    u32 event;

	ENTER();
	
    if (!card || !card->func) {
        PRINTM(ERROR, "card or function is NULL!\n");
        ret = WLAN_STATUS_FAILURE;
        return ret; 
    }

    if (!payload) {
        PRINTM(WARN, "payload NULL pointer received!\n");
        ret = WLAN_STATUS_FAILURE;
	return ret;
    }

    //sdio_claim_host(card->func);

    /* Read the length of data to be transferred */
    if (priv->enhance_flag)
        ret = sd_read_rx_len(priv, &buf_len);
    else
        ret = mv_sdio_read_scratch(priv, &buf_len);
    if (ret < 0) {
        PRINTM(ERROR, "card_to_host, read scratch reg failed\n");
        ret = WLAN_STATUS_FAILURE;
        goto exit;
    }

    if (buf_len <= SDIO_HEADER_LEN || buf_len > npayload) {
        PRINTM(ERROR, "card_to_host, invalid packet length: %d\n", buf_len);
        ret = WLAN_STATUS_FAILURE;
        goto exit;
    }

    /* Allocate buffer */
    blksz = SD_BLOCK_SIZE;
    buf_block_len = (buf_len + blksz - 1) / blksz;

    ret = sdio_readsb(card->func, payload, priv->wlan_dev.ioport,
                      buf_block_len * blksz);

    if (ret < 0) {
        PRINTM(ERROR, "card_to_host, read iomem failed: %d\n", ret);
        ret = WLAN_STATUS_FAILURE;
        goto exit;
    }
    if (priv->enhance_flag)
        *nb = wlan_le16_to_cpu(*(u16 *) & payload[0]);
    else
        *nb = buf_len;

    DBG_HEXDUMP(IF_D, "SDIO Blk Rd", payload, blksz * buf_block_len);

    *type = wlan_le16_to_cpu(*(u16 *) & payload[2]);
    if (*type == MV_TYPE_EVENT) {
        event = *(u32 *) & payload[4];
        priv->adapter->EventCause = wlan_le32_to_cpu(event);
    }

  exit:
//	sdio_release_host(card->func);
    LEAVE();
    return ret;
}

/** 
 *  @brief This function enables the host interrupts mask
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @param mask	   the interrupt mask
 *  @return 	   WLAN_STATUS_SUCCESS
 */
/* external lock needed */
static int
enable_host_int_mask(wlan_private * priv, u8 mask)
{
    int ret = WLAN_STATUS_SUCCESS;

    ENTER();

    /* Simply write the mask to the register */
    ret = sbi_write_ioreg(priv, HOST_INT_MASK_REG, mask);

    if (ret) {
        PRINTM(WARN, "Unable to enable the host interrupt!\n");
        ret = WLAN_STATUS_FAILURE;
    }

    priv->adapter->HisRegCpy = 1;

    LEAVE();
    return ret;
}

/**  @brief This function disables the host interrupts mask.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @param mask	   the interrupt mask
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
/* external lock needed */
static int
disable_host_int_mask(wlan_private * priv, u8 mask)
{
    int ret = WLAN_STATUS_SUCCESS;
    u8 host_int_mask;

    ENTER();

    /* Read back the host_int_mask register */
    ret = sbi_read_ioreg(priv, HOST_INT_MASK_REG, &host_int_mask);
    if (ret) {
        ret = WLAN_STATUS_FAILURE;
        goto done;
    }

    /* Update with the mask and write back to the register */
    host_int_mask &= ~mask;
    ret = sbi_write_ioreg(priv, HOST_INT_MASK_REG, host_int_mask);
    if (ret < 0) {
        PRINTM(WARN, "Unable to diable the host interrupt!\n");
        ret = WLAN_STATUS_FAILURE;
        goto done;
    }

  done:
    LEAVE();
    return ret;
}

/********************************************************
		Global Functions
********************************************************/

/** 
 *  @brief This function handles the interrupt.
 *  
 *  @param card	   The card handle.
 *  @return 	   n/a
 */
static void
sbi_interrupt(struct sdio_func *func)
{
    struct sdio_mmc_card *card;
    wlan_private *priv;
    u8 ireg = 0;

    ENTER();

    card = sdio_get_drvdata(func);
    if (!card || !card->priv) {
        PRINTM(INFO, "%s: sbi_interrupt(%p) card or priv is NULL, card=%p\n",
               __FUNCTION__, func, card);
        return;
    }

    priv = card->priv;
    wlan_interrupt(priv);
    if (sbi_get_int_status(priv, &ireg)) {
        PRINTM(ERROR, "%s: reading HOST_INT_STATUS_REG failed\n",
               __FUNCTION__);
    } else
        PRINTM(INFO, "%s: HOST_INT_STATUS_REG %#x\n", __FUNCTION__, ireg);
    wake_up_interruptible(&priv->MainThread.waitQ);
}

/** client driver probe handler 		*/
static int
wlan_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
    int ret = WLAN_STATUS_FAILURE;
    struct sdio_mmc_card *card = NULL;
    u8 chiprev;

    ENTER();

    if (!wlan_add_callback) {
        PRINTM(FATAL, "%s: add_card callback function not found!\n",
               __FUNCTION__);
        goto done;
    }

    if (func->class != SDIO_CLASS_WLAN) {
        PRINTM(FATAL, "%s: class-id SD_CLASS_WLAN not found!\n",
               __FUNCTION__);
        goto done;
    }

    PRINTM(INFO, "%s: vendor=0x%4.04X device=0x%4.04X class=%d function=%d\n",
           __FUNCTION__, func->vendor, func->device, func->class, func->num);

    sdio_claim_host(func);

    ret = sdio_enable_func(func);
    if (ret) {
        PRINTM(FATAL, "sdio_enable_func() failed: ret=%d\n", ret);
        goto release_host;
    }

    ret = sdio_claim_irq(func, sbi_interrupt);
    if (ret) {
        PRINTM(FATAL, "sdio_claim_irq failed: ret=%d\n", ret);
        goto disable_func;
    }

    /* read Revision Register to get the chip revision number */
    chiprev = sdio_readb(func, CARD_REVISION_REG, &ret);
    if (ret) {
        PRINTM(FATAL, "cannot read CARD_REVISION_REG\n");
        goto release_irq;
    }

    PRINTM(INFO, "revision=%#x\n", chiprev);

    card = kzalloc(sizeof(struct sdio_mmc_card), GFP_KERNEL);
    if (!card) {
        ret = -ENOMEM;
        goto release_irq;
    }

    card->func = func;

    ret = sdio_set_block_size(card->func, SD_BLOCK_SIZE);
    if (ret) {
        PRINTM(ERROR, "%s: cannot set SDIO block size\n", __FUNCTION__);
                ret = WLAN_STATUS_FAILURE;
        goto release_irq;
            }

    sdio_release_host(func);

    if (!wlan_add_callback(card)) {
        PRINTM(ERROR, "%s: wlan_add_callback failed\n", __FUNCTION__);
                ret = WLAN_STATUS_FAILURE;
        goto reclaim_host;
    }

    ret = WLAN_STATUS_SUCCESS;
    goto done;

  reclaim_host:
    sdio_claim_host(func);
  release_irq:
    sdio_release_irq(func);
  disable_func:
    sdio_disable_func(func);
  release_host:
    sdio_release_host(func);

    if (card) {
        kfree(card);
        card = NULL;
    }

    if (pwlanpriv)
        return WLAN_STATUS_SUCCESS;
    else
        return WLAN_STATUS_FAILURE;
  done:
    LEAVE();
    return ret;
}

/** client driver remove handler 	*/
static void
wlan_remove(struct sdio_func *func)
{
    struct sdio_mmc_card *card;

    ENTER();

    if (!wlan_remove_callback) {
        PRINTM(FATAL, "%s: remove_card callback function not found!\n",
               __FUNCTION__);
        goto done;
        }
    if (func) {
        card = sdio_get_drvdata(func);
        if (card) {
            sdio_claim_host(func);
            sdio_release_irq(func);
            sdio_disable_func(func);
            sdio_release_host(func);

            wlan_remove_callback(card);

            kfree(card);
            card = NULL;
        }
    }

  done:
    LEAVE();
}

#ifdef CONFIG_PM
/* client driver suspend handler 	*/
int wlan_suspend(struct sdio_func *func)
{
    ENTER();
    LEAVE();
    return 0;
}

/* client driver resume handler 	*/
int wlan_resume(struct sdio_func *func)
{
    ENTER();
    LEAVE();
    return 0;
}
#endif

/** WLAN IDs */
static const struct sdio_device_id wlan_ids[] = {
    {SDIO_DEVICE_CLASS(SDIO_CLASS_WLAN)},
    {},
};

MODULE_DEVICE_TABLE(sdio, wlan_ids);

static struct sdio_driver wlan_sdio = {
    .name = "wlan_sdio",
    .id_table = wlan_ids,
    .probe = wlan_probe,
    .remove = wlan_remove,
#ifdef CONFIG_PM
//    .suspend  = wlan_suspend,
//    .resume   = wlan_resume, 
#endif

};

/** 
 *  @brief This function registers the IF module in bus driver.
 *  
 *  @param add	   wlan driver's call back funtion for add card.
 *  @param remove  wlan driver's call back funtion for remove card.
 *  @param arg     not been used
 *  @return	   An int pointer that keeps returned value
 */
int *
sbi_register(wlan_notifier_fn_add add, wlan_notifier_fn_remove remove,
             void *arg)
{
    int *sdio_ret = (int *) 1;

    ENTER();

    wlan_add_callback = add;
    wlan_remove_callback = remove;

    /* SDIO Driver Registration */
    if (sdio_register_driver(&wlan_sdio) != 0) {
        PRINTM(FATAL, "SDIO Driver Registration Failed \n");
        sdio_ret = NULL;
    }

    LEAVE();
    return sdio_ret;
}

/** 
 *  @brief This function de-registers the IF module in bus driver.
 *  
 *  @return 	   n/a
 */
void
sbi_unregister(void)
{
    ENTER();

    /* SDIO Driver Unregistration */
    sdio_unregister_driver(&wlan_sdio);

    LEAVE();
}

/** 
 *  @brief This function reads the IO register.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @param reg	   register to be read
 *  @param dat	   A pointer to variable that keeps returned value
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_read_ioreg(wlan_private * priv, u32 reg, u8 * dat)
{
    struct sdio_mmc_card *card;
    int ret = WLAN_STATUS_FAILURE;

    ENTER();

    card = priv->wlan_dev.card;
    if (!card || !card->func) {
        PRINTM(ERROR, "sbi_read_ioreg(): card or function is NULL!\n");
        goto done;
    }

    *dat = sdio_readb(card->func, reg, &ret);
    if (ret) {
        PRINTM(ERROR, "sbi_read_ioreg(): sdio_readb failed! ret=%d\n", ret);
        goto done;
    }

    PRINTM(INFO, "sbi_read_ioreg() priv=%p func=%d reg=%#x dat=%#x\n", priv,
           card->func->num, reg, *dat);

  done:
    LEAVE();
    return ret;
}

/** 
 *  @brief This function writes the IO register.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @param reg	   register to be written
 *  @param dat	   the value to be written
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_write_ioreg(wlan_private * priv, u32 reg, u8 dat)
{
    struct sdio_mmc_card *card;
    int ret = WLAN_STATUS_FAILURE;

    ENTER();

    card = priv->wlan_dev.card;
    if (!card || !card->func) {
        PRINTM(ERROR, "sbi_write_ioreg(): card or function is NULL!\n");
        goto done;
    }

    PRINTM(INFO, "sbi_write_ioreg() priv=%p func=%d reg=%#x dat=%#x\n", priv,
           card->func->num, reg, dat);

    sdio_writeb(card->func, dat, reg, &ret);
    if (ret) {
        PRINTM(ERROR, "sbi_write_ioreg(): sdio_readb failed! ret=%d\n", ret);
        goto done;
    }

  done:
    LEAVE();
    return ret;
}

/** 
 *  @brief This function checks the interrupt status and handle it accordingly.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @param ireg    A pointer to variable that keeps returned value
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_get_int_status(wlan_private * priv, u8 * ireg)
{
    struct sdio_mmc_card *card = priv->wlan_dev.card;
    int ret = WLAN_STATUS_SUCCESS;
    u8 sdio_ireg = 0;
    u8 *cmdBuf;
    wlan_dev_t *wlan_dev = &priv->wlan_dev;
    struct sk_buff *skb;

    ENTER();

    if (!card || !card->func) {
        PRINTM(ERROR, "%s: card or function is NULL!\n", __FUNCTION__);
        ret = WLAN_STATUS_FAILURE;
	return ret;
    }

        //PRINTM(ERROR, "%s: before claim host!\n", __FUNCTION__);
	//sdio_claim_host(card->func);
        ///PRINTM(ERROR, "%s: after claim host!\n", __FUNCTION__);

    *ireg = 0;
    if ((ret = sbi_read_ioreg(priv, HOST_INTSTATUS_REG, &sdio_ireg))) {
        PRINTM(WARN, "sbi_read_ioreg: read int status register failed\n");
        ret = WLAN_STATUS_FAILURE;
        goto done;
    }

    if (sdio_ireg != 0) {
        /*
         * DN_LD_HOST_INT_STATUS and/or UP_LD_HOST_INT_STATUS
         * Clear the interrupt status register and re-enable the interrupt
         */
        PRINTM(INFO, "sdio_ireg = 0x%x\n", sdio_ireg);
        if ((ret = sbi_write_ioreg(priv, HOST_INTSTATUS_REG,
                                    ~(sdio_ireg) & (DN_LD_HOST_INT_STATUS |
                                                    UP_LD_HOST_INT_STATUS))) <
            0) {
            PRINTM(WARN,
                   "sbi_write_ioreg: clear int status register failed\n");
            ret = WLAN_STATUS_FAILURE;
            goto done;
        }
    } else
        PRINTM(ERROR, "cause=0 sdio_ireg = 0x%x\n", sdio_ireg);

    if (sdio_ireg & DN_LD_HOST_INT_STATUS) {    /* tx_done INT */
        *ireg |= HIS_TxDnLdRdy;
        if (!priv->wlan_dev.dnld_sent) {        /* tx_done already received */
            PRINTM(INFO, "warning: tx_done already received:"
                   " dnld_sent=0x%x int status=0x%x\n",
                   priv->wlan_dev.dnld_sent, sdio_ireg);
        } else {
            wmm_process_fw_iface_tx_xfer_end(priv);
            priv->wlan_dev.dnld_sent = DNLD_RES_RECEIVED;
        }
    }

    if (sdio_ireg & UP_LD_HOST_INT_STATUS) {

        /* 
         * DMA read data is by block alignment,so we need alloc extra block
         * to avoid wrong memory access.
         */
        if (!(skb = dev_alloc_skb(ALLOC_BUF_SIZE))) {
            PRINTM(WARN, "No free skb\n");
            priv->stats.rx_dropped++;
            ret = WLAN_STATUS_FAILURE;
            goto done;
        }

        /* 
         * Transfer data from card
         * skb->tail is passed as we are calling skb_put after we
         * are reading the data
         */
        if (mv_sdio_card_to_host(priv, &wlan_dev->upld_typ,
                                 (int *) &wlan_dev->upld_len, skb->tail,
                                 ALLOC_BUF_SIZE) < 0) {
            u8 cr = 0;

            PRINTM(ERROR, "Card to host failed: int status=0x%x\n",
                   sdio_ireg);
            if (sbi_read_ioreg(priv, CONFIGURATION_REG, &cr) < 0)
                PRINTM(ERROR, "read ioreg failed (CFG)\n");

            PRINTM(INFO, "Config Reg val = %d\n", cr);
            if (sbi_write_ioreg(priv, CONFIGURATION_REG, (cr | 0x04)) < 0)
                PRINTM(ERROR, "write ioreg failed (CFG)\n");

            PRINTM(INFO, "write success\n");
            if (sbi_read_ioreg(priv, CONFIGURATION_REG, &cr) < 0)
                PRINTM(ERROR, "read ioreg failed (CFG)\n");

            PRINTM(INFO, "Config reg val =%x\n", cr);
            ret = WLAN_STATUS_FAILURE;
            kfree_skb(skb);
            goto done;
        }

        switch (wlan_dev->upld_typ) {
        case MV_TYPE_DAT:
            PRINTM(DATA, "Data <= FW\n");
            skb_put(skb, priv->wlan_dev.upld_len);
            skb_pull(skb, SDIO_HEADER_LEN);
            wlan_process_rx_packet(priv, skb);
            priv->adapter->IntCounter = 0;
            /* skb will be freed by kernel later */
            break;

        case MV_TYPE_CMD:
            *ireg |= HIS_CmdUpLdRdy;
            priv->adapter->HisRegCpy |= *ireg;

            /* take care of CurCmd = NULL case */
            if (!priv->adapter->CurCmd) {
                cmdBuf = priv->wlan_dev.upld_buf;
            } else {
                cmdBuf = priv->adapter->CurCmd->BufVirtualAddr;
            }

            priv->wlan_dev.upld_len -= SDIO_HEADER_LEN;
            memcpy(cmdBuf, skb->data + SDIO_HEADER_LEN,
                   MIN(MRVDRV_SIZE_OF_CMD_BUFFER, priv->wlan_dev.upld_len));
            kfree_skb(skb);
            break;

        case MV_TYPE_EVENT:
            *ireg |= HIS_CardEvent;
            priv->adapter->HisRegCpy |= *ireg;
            /* event cause has been saved to priv->adapter->EventCause */
            kfree_skb(skb);
            break;

        default:
            PRINTM(ERROR, "SDIO unknown upld type = 0x%x\n",
                   wlan_dev->upld_typ);
            kfree_skb(skb);
            break;
        }
    }

    ret = WLAN_STATUS_SUCCESS;
done:
    //sdio_release_host(card->func);
    LEAVE();
    return ret;
}

/** 
 *  @brief This function disables the host interrupts.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_disable_host_int(wlan_private * priv)
{
    struct sdio_mmc_card *card = priv->wlan_dev.card;
    int ret;

    sdio_claim_host(card->func);
    ret = disable_host_int_mask(priv, HIM_DISABLE);
    sdio_release_host(card->func);
    return ret;
}

/** 
 *  @brief This function enables the host interrupts.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @return 	   WLAN_STATUS_SUCCESS
 */
int
sbi_enable_host_int(wlan_private * priv)
{
    struct sdio_mmc_card *card = priv->wlan_dev.card;
    int ret;

    sdio_claim_host(card->func);
    ret = enable_host_int_mask(priv, HIM_ENABLE);
    sdio_release_host(card->func);
    return ret;
}

/** 
 *  @brief This function de-registers the device.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @return 	   WLAN_STATUS_SUCCESS
 */
int
sbi_unregister_dev(wlan_private * priv)
{
    struct sdio_mmc_card *card = priv->wlan_dev.card;

    ENTER();

    if (!card || !card->func) {
        PRINTM(ERROR, "Error: card or function is NULL!\n");
        goto done;
    }

    sdio_set_drvdata(card->func, NULL);

    GPIO_PORT_TO_LOW();

  done:
    LEAVE();
    return WLAN_STATUS_SUCCESS;
}

/** 
 *  @brief This function registers the device.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_register_dev(wlan_private * priv)
{
    int ret = WLAN_STATUS_FAILURE;
    u8 reg;
    struct sdio_mmc_card *card = priv->wlan_dev.card;
    struct sdio_func *func;

    ENTER();

    if (!card || !card->func) {
        PRINTM(ERROR, "Error: card or function is NULL!\n");
        goto free_card;
    }

    func = card->func;

    GPIO_PORT_INIT();
    GPIO_PORT_TO_HIGH();

    /* Initialize the private structure */
    strncpy(priv->wlan_dev.name, "sdio0", sizeof(priv->wlan_dev.name));
    priv->wlan_dev.ioport = 0;
    priv->wlan_dev.upld_rcv = 0;
    priv->wlan_dev.upld_typ = 0;
    priv->wlan_dev.upld_len = 0;

    sdio_claim_host(func);

    /* Read the IO port */
    ret = sbi_read_ioreg(priv, IO_PORT_0_REG, &reg);
    if (ret)
        goto release_irq;
    else
        priv->wlan_dev.ioport |= reg;

    ret = sbi_read_ioreg(priv, IO_PORT_1_REG, &reg);
    if (ret)
        goto release_irq;
    else
        priv->wlan_dev.ioport |= (reg << 8);

    ret = sbi_read_ioreg(priv, IO_PORT_2_REG, &reg);
    if (ret)
        goto release_irq;
    else
        priv->wlan_dev.ioport |= (reg << 16);

    PRINTM(INFO, "SDIO FUNC #%d IO port: 0x%x\n", func->num,
           priv->wlan_dev.ioport);

    priv->hotplug_device = &func->dev;
    if (helper_name == NULL) {
        helper_name = DEFAULT_HELPER_NAME;
    }
    if (fw_name == NULL) {
        fw_name = find_fw_name(priv);
        if (fw_name == NULL) {
            goto release_irq;
        }
    }

    sdio_release_host(func);

    sdio_set_drvdata(func, card);

    ret = WLAN_STATUS_SUCCESS;
    goto done;

  release_irq:
    sdio_release_irq(func);
    sdio_disable_func(func);
    sdio_release_host(func);
  free_card:
    if (card) {
        kfree(card);
        card = NULL;
    }

  done:
    LEAVE();
    return ret;
}

/** 
 *  @brief This function sends data to the card.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @param type	   data or command
 *  @param payload A pointer to the data/cmd buffer
 *  @param nb	   the length of data/cmd
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_host_to_card(wlan_private * priv, u8 type, u8 * payload, u16 nb)
{
    struct sdio_mmc_card *card = priv->wlan_dev.card;
    int ret = WLAN_STATUS_SUCCESS;
    int buf_block_len;
    int blksz;
    int i = 0;
    void *tmpcmdbuf = NULL;
    int tmpcmdbufsz;
    u8 *cmdbuf = NULL;

    ENTER();

    if (!card || !card->func) {
        PRINTM(ERROR, "card or function is NULL!\n");
        ret = WLAN_STATUS_FAILURE;
        goto exit;
    }

    priv->adapter->HisRegCpy = 0;

    /* Allocate buffer and copy payload */
    blksz = SD_BLOCK_SIZE;
    buf_block_len = (nb + SDIO_HEADER_LEN + blksz - 1) / blksz;

    /* This is SDIO specific header
     *  u16 length,
     *  u16 type (MV_TYPE_DAT = 0, MV_TYPE_CMD = 1, MV_TYPE_EVENT = 3) 
     */
    if (type == MV_TYPE_DAT) {
        *(u16 *) & payload[0] = wlan_cpu_to_le16(nb + SDIO_HEADER_LEN);
        *(u16 *) & payload[2] = wlan_cpu_to_le16(type);
    } else if (type == MV_TYPE_CMD) {
        tmpcmdbufsz = WLAN_UPLD_SIZE;
        tmpcmdbuf = kmalloc(tmpcmdbufsz, GFP_KERNEL);
        if (!tmpcmdbuf) {
            PRINTM(ERROR, "Unable to allocate buffer for CMD.\n");
            ret = WLAN_STATUS_FAILURE;
            goto exit;
        }
        memset(tmpcmdbuf, 0, tmpcmdbufsz);
        cmdbuf = (u8 *) tmpcmdbuf;

        *(u16 *) & cmdbuf[0] = wlan_cpu_to_le16(nb + SDIO_HEADER_LEN);
        *(u16 *) & cmdbuf[2] = wlan_cpu_to_le16(type);

        if (payload != NULL &&
            (nb > 0 && nb <= (WLAN_UPLD_SIZE - SDIO_HEADER_LEN))) {

            memcpy(&cmdbuf[SDIO_HEADER_LEN], payload, nb);
        } else {
            PRINTM(WARN, "sbi_host_to_card(): Error: payload=%p, nb=%d\n",
                   payload, nb);
        }
    }

    sdio_claim_host(card->func);

    do {

        /* Transfer data to card */
        ret = sdio_writesb(card->func, priv->wlan_dev.ioport,
                           (type == MV_TYPE_DAT) ? payload : cmdbuf,
                           buf_block_len * blksz);
        if (ret < 0) {
            i++;
            PRINTM(ERROR, "host_to_card, write iomem (%d) failed: %d\n", i,
                   ret);
            if (sbi_write_ioreg(priv, CONFIGURATION_REG, 0x04) < 0) {
                PRINTM(ERROR, "write ioreg failed (CFG)\n");
            }
            ret = WLAN_STATUS_FAILURE;
            if (i > MAX_WRITE_IOMEM_RETRY)
                goto exit;
        } else {
            DBG_HEXDUMP(IF_D, "SDIO Blk Wr",
                        (type == MV_TYPE_DAT) ? payload : cmdbuf,
                        blksz * buf_block_len);
        }
    } while (ret == WLAN_STATUS_FAILURE);

    if (type == MV_TYPE_DAT)
        priv->wlan_dev.dnld_sent = DNLD_DATA_SENT;
    else
        priv->wlan_dev.dnld_sent = DNLD_CMD_SENT;

  exit:
    sdio_release_host(card->func);
    if (tmpcmdbuf)
        kfree(tmpcmdbuf);

    LEAVE();
    return ret;
}

/** 
 *  @brief This function reads CIS informaion.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @param cisinfo A pointer to CIS information output buffer
 *  @param cislen  The length of CIS information output buffer
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_get_cis_info(wlan_private * priv, void *cisinfo, int *cislen)
{
#define CIS_PTR (0x8000)
    struct sdio_mmc_card *card = priv->wlan_dev.card;
    unsigned int i, cis_ptr = CIS_PTR;
    int ret = WLAN_STATUS_FAILURE;

    ENTER();

    if (!card || !card->func) {
        PRINTM(ERROR, "sbi_get_cis_info(): card or function is NULL!\n");
        goto exit;
    }
#define MAX_SDIO_CIS_INFO_LEN (256)
    if (!cisinfo || (*cislen < MAX_SDIO_CIS_INFO_LEN)) {
        PRINTM(WARN, "ERROR! get_cis_info: insufficient buffer passed\n");
        goto exit;
    }

    *cislen = MAX_SDIO_CIS_INFO_LEN;

    sdio_claim_host(card->func);

    PRINTM(INFO, "cis_ptr=%#x\n", cis_ptr);

    /* Read the Tuple Data */
    for (i = 0; i < *cislen; i++) {
        ((unsigned char *) cisinfo)[i] =
            sdio_readb(card->func, cis_ptr + i, &ret);
        if (ret) {
            PRINTM(WARN, "get_cis_info error: ret=%d\n", ret);
            ret = WLAN_STATUS_FAILURE;
            goto done;
        }
        PRINTM(INFO, "cisinfo[%d]=%#x\n", i, ((unsigned char *) cisinfo)[i]);
    }

  done:
    sdio_release_host(card->func);
  exit:
    LEAVE();
    return ret;
}

/** 
 *  @brief This function downloads helper image to the card.
 *  
 *  @param priv    	A pointer to wlan_private structure
 *  @return 	   	WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_prog_helper(wlan_private * priv)
{
    struct sdio_mmc_card *card = priv->wlan_dev.card;
    u8 *helper = NULL;
    int helperlen;
    int ret = WLAN_STATUS_SUCCESS;
    void *tmphlprbuf = NULL;
    int tmphlprbufsz;
    u8 *hlprbuf;
    int hlprblknow;
    u32 tx_len;

    ENTER();

    if (!card || !card->func) {
        PRINTM(ERROR, "sbi_prog_helper(): card or function is NULL!\n");
        goto done;
    }

    if (priv->fw_helper) {
        helper = priv->fw_helper->data;
        helperlen = priv->fw_helper->size;
    } else {
        PRINTM(MSG, "No helper image found! Terminating download.\n");
        return WLAN_STATUS_FAILURE;
    }

    PRINTM(INFO, "Downloading helper image (%d bytes), block size %d bytes\n",
           helperlen, SD_BLOCK_SIZE);

    tmphlprbufsz = WLAN_UPLD_SIZE;
    tmphlprbuf = kmalloc(tmphlprbufsz, GFP_KERNEL);
    if (!tmphlprbuf) {
        PRINTM(ERROR,
               "Unable to allocate buffer for helper. Terminating download\n");
        ret = WLAN_STATUS_FAILURE;
        goto done;
    }
    memset(tmphlprbuf, 0, tmphlprbufsz);
    hlprbuf = (u8 *) tmphlprbuf;

    sdio_claim_host(card->func);

    /* Perform helper data transfer */
    tx_len = (FIRMWARE_TRANSFER_NBLOCK * SD_BLOCK_SIZE) - SDIO_HEADER_LEN;
    hlprblknow = 0;
    do {
        /* The host polls for the DN_LD_CARD_RDY and CARD_IO_READY bits */
        ret = mv_sdio_poll_card_status(priv, CARD_IO_READY | DN_LD_CARD_RDY);
        if (ret < 0) {
            PRINTM(FATAL, "Helper download poll status timeout @ %d\n",
                   hlprblknow);
            goto done;
        }

        /* More data? */
        if (hlprblknow >= helperlen)
            break;

        /* Set blocksize to transfer - checking for last block */
        if (helperlen - hlprblknow < tx_len)
            tx_len = helperlen - hlprblknow;

        /* Set length to the 4-byte header */
        *(u32 *) hlprbuf = wlan_cpu_to_le32(tx_len);

        /* Copy payload to buffer */
        memcpy(&hlprbuf[SDIO_HEADER_LEN], &helper[hlprblknow], tx_len);

        PRINTM(INFO, ".");

        /* Send data */
        ret = sdio_writesb(card->func, priv->wlan_dev.ioport,
                           hlprbuf, FIRMWARE_TRANSFER_NBLOCK * SD_BLOCK_SIZE);

        if (ret < 0) {
            PRINTM(FATAL, "IO error during helper download @ %d\n",
                   hlprblknow);
            goto done;
        }

        hlprblknow += tx_len;
    } while (TRUE);

    /* Write last EOF data */
    PRINTM(INFO, "\nTransferring helper image EOF block\n");
    memset(hlprbuf, 0x0, SD_BLOCK_SIZE);
    ret = sdio_writesb(card->func, priv->wlan_dev.ioport,
                       hlprbuf, SD_BLOCK_SIZE);

    if (ret < 0) {
        PRINTM(FATAL, "IO error in writing helper image EOF block\n");
        goto done;
    }

    ret = WLAN_STATUS_SUCCESS;

  done:
    sdio_release_host(card->func);
    if (tmphlprbuf)
        kfree(tmphlprbuf);

    return ret;
}

/** 
 *  @brief This function downloads firmware image to the card.
 *  
 *  @param priv    	A pointer to wlan_private structure
 *  @return 	   	WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_prog_fw_w_helper(wlan_private * priv)
{
    struct sdio_mmc_card *card = priv->wlan_dev.card;
    u8 *firmware = NULL;
    int firmwarelen;
    u8 base0;
    u8 base1;
    int ret = WLAN_STATUS_SUCCESS;
    int offset;
    void *tmpfwbuf = NULL;
    int tmpfwbufsz;
    u8 *fwbuf;
    u16 len;
    int txlen = 0;
    int tx_blocks = 0;
    int i = 0;
    int tries = 0;

    ENTER();

    if (!card || !card->func) {
        PRINTM(ERROR, "sbi_prog_fw_w_helper(): card or function is NULL!\n");
        goto done;
    }

    if (priv->firmware) {
        firmware = priv->firmware->data;
        firmwarelen = priv->firmware->size;
    } else {
        PRINTM(MSG, "No firmware image found! Terminating download.\n");
        return WLAN_STATUS_FAILURE;
    }

    PRINTM(INFO, "Downloading FW image (%d bytes)\n", firmwarelen);

    tmpfwbufsz = WLAN_UPLD_SIZE;
    tmpfwbuf = kmalloc(tmpfwbufsz, GFP_KERNEL);
    if (!tmpfwbuf) {
        PRINTM(ERROR,
               "Unable to allocate buffer for firmware. Terminating download.\n");
        ret = WLAN_STATUS_FAILURE;
        goto done;
    }
    memset(tmpfwbuf, 0, tmpfwbufsz);
    fwbuf = (u8 *) tmpfwbuf;

    sdio_claim_host(card->func);

    /* Perform firmware data transfer */
    offset = 0;
    do {
        /* The host polls for the DN_LD_CARD_RDY and CARD_IO_READY bits */
        ret = mv_sdio_poll_card_status(priv, CARD_IO_READY | DN_LD_CARD_RDY);
        if (ret < 0) {
            PRINTM(FATAL,
                   "FW download with helper poll status timeout @ %d\n",
                   offset);
            goto done;
        }

        /* More data? */
        if (offset >= firmwarelen)
            break;

        for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
            if ((ret = sbi_read_ioreg(priv, HOST_F1_RD_BASE_0, &base0)) < 0) {
                PRINTM(WARN, "Dev BASE0 register read failed:"
                       " base0=0x%04X(%d). Terminating download.\n", base0,
                       base0);
                ret = WLAN_STATUS_FAILURE;
                goto done;
            }
            if ((ret = sbi_read_ioreg(priv, HOST_F1_RD_BASE_1, &base1)) < 0) {
                PRINTM(WARN, "Dev BASE1 register read failed:"
                       " base1=0x%04X(%d). Terminating download.\n", base1,
                       base1);
                ret = WLAN_STATUS_FAILURE;
                goto done;
            }
            len = (((u16) base1) << 8) | base0;

            if (len != 0)
                break;
            udelay(10);
        }

        if (len == 0)
            break;
        else if (len > WLAN_UPLD_SIZE) {
            PRINTM(FATAL, "FW download failure @ %d, invalid length %d\n",
                   offset, len);
            ret = WLAN_STATUS_FAILURE;
            goto done;
        }

        txlen = len;

        if (len & BIT(0)) {
            i++;
            if (i > MAX_WRITE_IOMEM_RETRY) {
                PRINTM(FATAL,
                       "FW download failure @ %d, over max retry count\n",
                       offset);
                ret = WLAN_STATUS_FAILURE;
                goto done;
            }
            PRINTM(ERROR, "FW CRC error indicated by the helper:"
                   " len = 0x%04X, txlen = %d\n", len, txlen);
            len &= ~BIT(0);
            /* Setting this to 0 to resend from same offset */
            txlen = 0;
        } else
            i = 0;

        /* Set blocksize to transfer - checking for last block */
        if (firmwarelen - offset < txlen) {
            txlen = firmwarelen - offset;
        }
        PRINTM(INFO, ".");

        tx_blocks = (txlen + SD_BLOCK_SIZE - 1) / SD_BLOCK_SIZE;

        /* Copy payload to buffer */
        memcpy(fwbuf, &firmware[offset], txlen);

        /* Send data */
        ret = sdio_writesb(card->func, priv->wlan_dev.ioport,
                           fwbuf, tx_blocks * SD_BLOCK_SIZE);
        if (ret < 0) {
            PRINTM(ERROR, "FW download, write iomem (%d) failed @ %d\n", i,
                   offset);
            if (sbi_write_ioreg(priv, CONFIGURATION_REG, 0x04) < 0) {
                PRINTM(ERROR, "write ioreg failed (CFG)\n");
            }
        }

        offset += txlen;
    } while (TRUE);

    PRINTM(INFO, "\nFW download over, size %d bytes\n", firmwarelen);

    ret = WLAN_STATUS_SUCCESS;
  done:
    sdio_release_host(card->func);
    if (tmpfwbuf)
        kfree(tmpfwbuf);

    LEAVE();
    return ret;
}

/** 
 *  @brief This function checks if the firmware is ready to accept
 *  command or not.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_check_fw_status(wlan_private * priv, int pollnum)
{
    struct sdio_mmc_card *card = priv->wlan_dev.card;
    int ret = WLAN_STATUS_SUCCESS;
    u16 firmwarestat;
    int tries;

    ENTER();

    sdio_claim_host(card->func);

    /* Wait for firmware initialization event */
    for (tries = 0; tries < pollnum; tries++) {
        if (!priv->enhance_flag) {
            if ((ret = mv_sdio_read_scratch(priv, &firmwarestat)) < 0)
                continue;
        } else {
            if ((ret = sd_read_firmware_status(priv, &firmwarestat)) < 0)
                continue;
        }

        if (firmwarestat == FIRMWARE_READY) {
            ret = WLAN_STATUS_SUCCESS;
            break;
        } else {
            mdelay(10);
            ret = WLAN_STATUS_FAILURE;
        }
    }

    if (ret < 0) {
        PRINTM(WARN, "Timeout waiting for FW to become active\n");
        goto done;
    }

    ret = WLAN_STATUS_SUCCESS;
    if (priv->enhance_flag)
        sd_get_rx_unit(priv);
  done:
    sdio_release_host(card->func);

    LEAVE();
    return ret;
}

/** 
 *  @brief This function set bus clock on/off
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @param option    TRUE--on , FALSE--off
 *  @return 	   WLAN_STATUS_SUCCESS
 */
int
sbi_set_bus_clock(wlan_private * priv, u8 option)
{
	/*
    if (option == TRUE)
        sd_start_clock((psd_device) priv->wlan_dev.card);
    else
        sd_stop_clock((psd_device) priv->wlan_dev.card);
    */  
        
    return WLAN_STATUS_SUCCESS;
}

/** 
 *  @brief This function makes firmware exiting from deep sleep.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_exit_deep_sleep(wlan_private * priv)
{
    struct sdio_mmc_card *card = priv->wlan_dev.card;
    int ret = WLAN_STATUS_SUCCESS;

    sdio_claim_host(card->func);

    sbi_set_bus_clock(priv, TRUE);

    if (priv->adapter->fwWakeupMethod == WAKEUP_FW_THRU_GPIO) {
        GPIO_PORT_TO_LOW();
    } else
        ret = sbi_write_ioreg(priv, CONFIGURATION_REG, HOST_POWER_UP);

    sdio_release_host(card->func);

    return ret;
}

/** 
 *  @brief This function resets the setting of deep sleep.
 *  
 *  @param priv    A pointer to wlan_private structure
 *  @return 	   WLAN_STATUS_SUCCESS or WLAN_STATUS_FAILURE
 */
int
sbi_reset_deepsleep_wakeup(wlan_private * priv)
{
    struct sdio_mmc_card *card = priv->wlan_dev.card;
    int ret = WLAN_STATUS_SUCCESS;

    ENTER();


	if (priv->adapter->fwWakeupMethod == WAKEUP_FW_THRU_GPIO) {
		GPIO_PORT_TO_HIGH();
	} else {
		sdio_claim_host(card->func);
		ret = sbi_write_ioreg(priv, CONFIGURATION_REG, 0);
		sdio_release_host(card->func);
	}


    LEAVE();

    return ret;
}

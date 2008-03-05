/** @file bt_drv.h
 *  @brief This header file contains global constant/enum definitions,
 *  global variable declaration.
 *       
 *  (c) Copyright 2007, Marvell International Ltd.   
 *
 *  This software file (the "File") is distributed by Marvell International 
 *  Ltd. under the terms of the GNU General Public License Version 2, June 1991 
 *  (the "License").  You may use, redistribute and/or modify this File in 
 *  accordance with the terms and conditions of the License, a copy of which 
 *  is available along with the File in the gpl.txt file or by writing to 
 *  the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
 *  02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 *  THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
 *  IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
 *  ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
 *  this warranty disclaimer.
 *
 */

#ifndef _BT_DRV_H_
#define _BT_DRV_H_

/** Double-Word(32Bit) Bit definition */
/** Double-Word bit 0 */
#define DW_BIT_0	0x00000001
/** Double-Word bit 1 */
#define DW_BIT_1	0x00000002
/** Double-Word bit 2 */
#define DW_BIT_2	0x00000004
/** Double-Word bit 3 */
#define DW_BIT_3	0x00000008
/** Double-Word bit 4 */
#define DW_BIT_4	0x00000010
/** Double-Word bit 5 */
#define DW_BIT_5	0x00000020
/** Double-Word bit 6 */
#define DW_BIT_6	0x00000040
/** Double-Word bit 7 */
#define DW_BIT_7	0x00000080
/** Double-Word bit 8 */
#define DW_BIT_8	0x00000100
/** Double-Word bit 9 */
#define DW_BIT_9	0x00000200
/** Double-Word bit 10 */
#define DW_BIT_10	0x00000400
/** Double-Word bit 11 */
#define DW_BIT_11       0x00000800
/** Double-Word bit 12 */
#define DW_BIT_12       0x00001000
/** Double-Word bit 13 */
#define DW_BIT_13       0x00002000
/** Double-Word bit 14 */
#define DW_BIT_14       0x00004000
/** Double-Word bit 15 */
#define DW_BIT_15       0x00008000
/** Double-Word bit 16 */
#define DW_BIT_16       0x00010000
/** Double-Word bit 17 */
#define DW_BIT_17       0x00020000
/** Double-Word bit 18 */
#define DW_BIT_18       0x00040000
/** Double-Word bit 19 */
#define DW_BIT_19       0x00080000
/** Double-Word bit 20 */
#define DW_BIT_20       0x00100000
/** Double-Word bit 21 */
#define DW_BIT_21       0x00200000
/** Double-Word bit 22 */
#define DW_BIT_22       0x00400000
/** Double-Word bit 23 */
#define DW_BIT_23       0x00800000
/** Double-Word bit 24 */
#define DW_BIT_24       0x01000000
/** Double-Word bit 25 */
#define DW_BIT_25       0x02000000
/** Double-Word bit 26 */
#define DW_BIT_26       0x04000000
/** Double-Word bit 27 */
#define DW_BIT_27       0x08000000
/** Double-Word bit 28 */
#define DW_BIT_28       0x10000000
/** Double-Word bit 29 */
#define DW_BIT_29       0x20000000
/** Double-Word bit 30 */
#define DW_BIT_30	0x40000000
/** Double-Word bit 31 */
#define DW_BIT_31	0x80000000

/** Word (16bit) Bit Definition*/
/** Word bit 0 */
#define W_BIT_0		0x0001
/** Word bit 1 */
#define W_BIT_1		0x0002
/** Word bit 2 */
#define W_BIT_2		0x0004
/** Word bit 3 */
#define W_BIT_3		0x0008
/** Word bit 4 */
#define W_BIT_4		0x0010
/** Word bit 5 */
#define W_BIT_5		0x0020
/** Word bit 6 */
#define W_BIT_6		0x0040
/** Word bit 7 */
#define W_BIT_7		0x0080
/** Word bit 8 */
#define W_BIT_8		0x0100
/** Word bit 9 */
#define W_BIT_9		0x0200
/** Word bit 10 */
#define W_BIT_10	0x0400
/** Word bit 11 */
#define W_BIT_11	0x0800
/** Word bit 12 */
#define W_BIT_12	0x1000
/** Word bit 13 */
#define W_BIT_13	0x2000
/** Word bit 14 */
#define W_BIT_14	0x4000
/** Word bit 15 */
#define W_BIT_15	0x8000

/** Byte (8Bit) Bit definition*/
/** Byte bit 0 */
#define B_BIT_0		0x01
/** Byte bit 1 */
#define B_BIT_1		0x02
/** Byte bit 2 */
#define B_BIT_2		0x04
/** Byte bit 3 */
#define B_BIT_3		0x08
/** Byte bit 4 */
#define B_BIT_4		0x10
/** Byte bit 5 */
#define B_BIT_5		0x20
/** Byte bit 6 */
#define B_BIT_6		0x40
/** Byte bit 7 */
#define B_BIT_7		0x80

/** Debug level : Message */
#define	DBG_MSG			DW_BIT_0
/** Debug level : Fatal */
#define DBG_FATAL		DW_BIT_1
/** Debug level : Error */
#define DBG_ERROR		DW_BIT_2
/** Debug level : Command */
#define DBG_CMD			DW_BIT_3
/** Debug level : Data */
#define DBG_DATA		DW_BIT_27
/** Debug level : Entry */
#define DBG_ENTRY		DW_BIT_28
/** Debug level : Warning */
#define DBG_WARN		DW_BIT_29
/** Debug level : Informative */
#define DBG_INFO		DW_BIT_30

#ifdef	DEBUG_LEVEL1
extern u32 drvdbg;

/** Print informative message */
#define	PRINTM_INFO(msg...)  {if (drvdbg & DBG_INFO)  printk(KERN_DEBUG msg);}
/** Print warning message */
#define	PRINTM_WARN(msg...)  {if (drvdbg & DBG_WARN)  printk(KERN_DEBUG msg);}
/** Print entry message */
#define	PRINTM_ENTRY(msg...) {if (drvdbg & DBG_ENTRY) printk(KERN_DEBUG msg);}
/** Print command message */
#define	PRINTM_CMD(msg...)   {if (drvdbg & DBG_CMD)   printk(KERN_DEBUG msg);}
/** Print erro message */
#define	PRINTM_ERROR(msg...) {if (drvdbg & DBG_ERROR) printk(KERN_DEBUG msg);}
/** Print fatal message */
#define	PRINTM_FATAL(msg...) {if (drvdbg & DBG_FATAL) printk(KERN_DEBUG msg);}
/** Print message */
#define	PRINTM_MSG(msg...)   {if (drvdbg & DBG_MSG)   printk(KERN_ALERT msg);}

/** Print message with required level */
#define	PRINTM(level,msg...) PRINTM_##level(msg)

/** Debug dump buffer length */
#define DBG_DUMP_BUF_LEN 	64
/** Maximum number of dump per line */
#define MAX_DUMP_PER_LINE	16
/** Maximum data dump length */
#define MAX_DATA_DUMP_LEN	48

static inline void
hexdump(char *prompt, u8 * buf, int len)
{
    int i;
    char dbgdumpbuf[DBG_DUMP_BUF_LEN];
    char *ptr = dbgdumpbuf;

    printk(KERN_DEBUG "%s: len=%d\n", prompt, len);
    for (i = 1; i <= len; i++) {
        ptr += sprintf(ptr, "%02x ", *buf);
        buf++;
        if (i % MAX_DUMP_PER_LINE == 0) {
            *ptr = 0;
            printk(KERN_DEBUG "%s\n", dbgdumpbuf);
            ptr = dbgdumpbuf;
        }
    }
    if (len % MAX_DUMP_PER_LINE) {
        *ptr = 0;
        printk(KERN_DEBUG "%s\n", dbgdumpbuf);
    }
}

/** Debug hexdump of debug data */
#define DBG_HEXDUMP_DBG_DATA(x,y,z)     {if (drvdbg & DBG_DATA) hexdump(x,y,z);}

/** Debug hexdump */
#define	DBG_HEXDUMP(level,x,y,z)    DBG_HEXDUMP_##level(x,y,z)

/** Mark entry point */
#define	ENTER()			PRINTM(ENTRY, "Enter: %s, %s:%i\n", __FUNCTION__, \
							__FILE__, __LINE__)
/** Mark exit point */
#define	LEAVE()			PRINTM(ENTRY, "Leave: %s, %s:%i\n", __FUNCTION__, \
							__FILE__, __LINE__)
#else
/** Do nothing */
#define	PRINTM(level,msg...) do {} while (0)
/** Do nothing */
#define DBG_HEXDUMP(level,x,y,z)    do {} while (0)
/** Do nothing */
#define	ENTER()  do {} while (0);
/** Do nothing */
#define	LEAVE()  do {} while (0);
#endif //DEBUG_LEVEL1

/** Length of device name */
#define DEV_NAME_LEN				32
/** Bluetooth upload size */
#define	BT_UPLD_SIZE				2312
/** Bluetooth status success */
#define BT_STATUS_SUCCESS			(0)
/** Bluetooth status failure */
#define BT_STATUS_FAILURE			(-1)

#ifndef	TRUE
/** True value */
#define TRUE			1
#endif
#ifndef	FALSE
/** False value */
#define	FALSE			0
#endif

/** Set thread state */
#define OS_SET_THREAD_STATE(x)		set_current_state(x)
/** Time to wait until Host Sleep state change */
#define WAIT_UNTIL_HS_STATE_CHANGED (5 * HZ)
/** Time to wait cmd resp */
#define WAIT_UNTIL_CMD_RESP	    (5 * HZ)

/** Set interruptible timeout value */
#define os_wait_interruptible_timeout(waitq, cond, timeout) \
	wait_event_interruptible_timeout(waitq, cond, timeout)

typedef struct
{
    /** Task */
    struct task_struct *task;
    /** Queue */
    wait_queue_head_t waitQ;
    /** PID */
    pid_t pid;
    /** Private structure */
    void *priv;
} bt_thread;

static inline void
bt_activate_thread(bt_thread * thr)
{
        /** Record the thread pid */
    thr->pid = current->pid;

        /** Initialize the wait queue */
    init_waitqueue_head(&thr->waitQ);
}

static inline void
bt_deactivate_thread(bt_thread * thr)
{
    thr->pid = 0;
    return;
}

static inline void
bt_create_thread(int (*btfunc) (void *), bt_thread * thr, char *name)
{
    thr->task = kthread_run(btfunc, thr, "%s", name);
}

static inline int
bt_terminate_thread(bt_thread * thr)
{
    /* Check if the thread is active or not */
    if (!thr->pid) {
        return -1;
    }
    kthread_stop(thr->task);
    return 0;
}

static inline void
os_sched_timeout(u32 millisec)
{
    set_current_state(TASK_INTERRUPTIBLE);

    schedule_timeout((millisec * HZ) / 1000);
}

#ifndef __ATTRIB_ALIGN__
#define __ATTRIB_ALIGN__ __attribute__((aligned(4)))
#endif

#ifndef __ATTRIB_PACK__
#define __ATTRIB_PACK__ __attribute__ ((packed))
#endif
/** Data structure for the Marvell Bluetooth device */
typedef struct _bt_dev
{
        /** device name */
    char name[DEV_NAME_LEN];
        /** card pointer */
    void *card;
        /** IO port */
    u32 ioport;
    /** HCI device */
    struct hci_dev *hcidev;

    /** Tx download ready flag */
    u8 tx_dnld_rdy;
    /** Function */
    u8 fn;
    /** Rx unit */
    u8 rx_unit;
    /** Power Save mode */
    u8 psmode;
    /** Power Save command */
    u8 pscmd;
    /** Host Sleep mode */
    u8 hsmode;
    /** Host Sleep command */
    u8 hscmd;
    /** Low byte is gap, high byte is GPIO */
    u16 gpio_gap;
    /** Host Sleep configuration command */
    u8 hscfgcmd;
    /** Host Send Cmd Flag		 */
    u8 sendcmdflag;
} bt_dev_t, *pbt_dev_t;

typedef struct _bt_adapter
{
    /** Temporry Tx buffer */
    u8 TmpTxBuf[BT_UPLD_SIZE] __ATTRIB_ALIGN__;
    /** Chip revision ID */
    u8 chip_rev;
    /** Surprise removed flag */
    u8 SurpriseRemoved;
    /** IRQ number */
    int irq;
    /** Interrupt counter */
    u32 IntCounter;
    /** Tx packet queue */
    struct sk_buff_head tx_queue;
    /** Power Save mode */
    u8 psmode;
    /** Power Save state */
    u8 ps_state;
    /** Host Sleep state */
    u8 hs_state;
    /** Number of wakeup tries */
    u8 WakeupTries;
    /** Host Sleep wait queue */
    wait_queue_head_t cmd_wait_q __ATTRIB_ALIGN__;
    /** Host Cmd complet state */
    u8 cmd_complete;
} bt_adapter, *pbt_adapter;

/** Private structure for the MV device */
typedef struct _bt_private
{
    /** Bluetooth device */
    bt_dev_t bt_dev;
    /** Adapter */
    bt_adapter *adapter;
    /** Firmware helper */
    const struct firmware *fw_helper;
    /** Firmware */
    const struct firmware *firmware;
    /** Hotplug device */
    struct device *hotplug_device;
        /** thread to service interrupts */
    bt_thread MainThread;
    /** Proc directory entry */
    struct proc_dir_entry *proc_entry;
    /** Proc mbt directory entry */
    struct proc_dir_entry *proc_mbt;
} bt_private, *pbt_private;

/** Marvell vendor packet */
#define MRVL_VENDOR_PKT			0xFE
/** Bluetooth command : Sleep mode */
#define BT_CMD_AUTO_SLEEP_MODE		0x23
/** Bluetooth command : Host Sleep configuration */
#define BT_CMD_HOST_SLEEP_CONFIG	0x59
/** Bluetooth command : Host Sleep enable */
#define BT_CMD_HOST_SLEEP_ENABLE	0x5A
/** Bluetooth command : Module Configuration request */
#define BT_CMD_MODULE_CFG_REQ		0x5B
/** Sub Command: Module Bring Up Request */
#define MODULE_BRINGUP_REQ		0xF1
/** Sub Command: Module Shut Down Request */
#define MODULE_SHUTDOWN_REQ		0xF2
/** Sub Command: Host Interface Control Request */
#define MODULE_INTERFACE_CTRL_REQ	0xF5

/** Bluetooth event : Power State */
#define BT_EVENT_POWER_STATE		0x20

/** Bluetooth Power State : Enable */
#define BT_PS_ENABLE			0x02
/** Bluetooth Power State : Disable */
#define BT_PS_DISABLE			0x03
/** Bluetooth Power State : Sleep */
#define BT_PS_SLEEP			0x01
/** Bluetooth Power State : Awake */
#define BT_PS_AWAKE			0x02

/** OGF */
#define OGF				0x3F

/** Host Sleep activated */
#define HS_ACTIVATED			0x01
/** Host Sleep deactivated */
#define HS_DEACTIVATED			0x00

/** Power Save sleep */
#define PS_SLEEP			0x01
/** Power Save awake */
#define PS_AWAKE			0x00

typedef struct _BT_CMD
{
    /** OCF OGF */
    u16 ocf_ogf;
    /** Length */
    u8 length;
    /** Data */
    u8 data[4];
} __ATTRIB_PACK__ BT_CMD;

typedef struct _BT_EVENT
{
    /** Event Counter */
    u8 EC;
    /** Length */
    u8 length;
    /** Data */
    u8 data[4];
} BT_EVENT;

/* Prototype of global function */
bt_private *bt_add_card(void *card);
int bt_remove_card(void *card);
void bt_interrupt(struct hci_dev *hdev);

int bt_proc_init(bt_private * priv);
void bt_proc_remove(bt_private * priv);
int bt_process_event(bt_private * priv, struct sk_buff *skb);
int bt_enable_hs(bt_private * priv);
int bt_prepare_command(bt_private * priv);

int *sbi_register(void);
void sbi_unregister(void);
int sbi_register_dev(bt_private * priv);
int sbi_unregister_dev(bt_private * priv);
int sbi_host_to_card(bt_private * priv, u8 type, u8 * payload, u16 nb);
int sbi_enable_host_int(bt_private * priv);
int sbi_disable_host_int(bt_private * priv);
int sbi_dowload_fw(bt_private * priv);
int sbi_get_int_status(bt_private * priv, u8 * ireg);
int sbi_wakeup_firmware(bt_private * priv);
int sbi_download_fw(bt_private *priv);
#endif /* _BT_DRV_H_ */

/** @file wlan_wext.h
 * @brief This file contains definition for IOCTL call.
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
/********************************************************
Change log:
	10/11/05: Add Doxygen format comments
	12/19/05: Correct a typo in structure _wlan_ioctl_wmm_tspec
	01/11/06: Conditionalize new scan/join ioctls
	04/10/06: Add hostcmd generic API
	04/18/06: Remove old Subscrive Event and add new Subscribe Event
	          implementation through generic hostcmd API
	06/08/06: Add definitions of custom events
	08/29/06: Add ledgpio private command
********************************************************/

#ifndef	_WLAN_WEXT_H_
#define	_WLAN_WEXT_H_

/** Offset for subcommand */
#define SUBCMD_OFFSET			4
/** PRIVATE CMD ID */
#define	WLANIOCTL			0x8BE0

/** Private command ID to set WPA IE */
#define WLANSETWPAIE			(WLANIOCTL + 0)
/** Private command ID to set CIS dump */
#define WLANCISDUMP 			(WLANIOCTL + 1)
#ifdef MFG_CMD_SUPPORT
#define	WLANMANFCMD			(WLANIOCTL + 2)
#endif
/** Private command ID to read/write register */
#define	WLANREGRDWR			(WLANIOCTL + 3)
/** Maximum EEPROM data */
#define MAX_EEPROM_DATA     			256
/** Private command ID to Host command */
#define	WLANHOSTCMD			(WLANIOCTL + 4)

/** Private command ID for ARP filter */
#define WLANARPFILTER			(WLANIOCTL + 6)

/** Private command ID to set/get int */
#define WLAN_SETINT_GETINT		(WLANIOCTL + 7)
/** Private command ID to get Noise Floor value */
#define WLANNF					1
/** Private command ID to get RSSI */
#define WLANRSSI				2
/** Private command ID to set/get BG scan */
#define WLANBGSCAN				4
/** Private command ID to enabled 11d support */
#define WLANENABLE11D				5
/** Private command ID to set/get AdHoc G rate */
#define WLANADHOCGRATE				6
/** Private command ID to set/get SDIO clock */
#define WLANSDIOCLOCK				7
/** Private command ID to enable WMM */
#define WLANWMM_ENABLE				8
/** Private command ID to set Null packet generation */
#define WLANNULLGEN				10
/** Private command ID to set AdHoc */
#define WLANADHOCCSET				11
/** Private command ID to set AdHoc G protection */
#define WLAN_ADHOC_G_PROT			12

/** Private command ID to set/get none */
#define WLAN_SETNONE_GETNONE	        (WLANIOCTL + 8)
/** Private command ID to turn on radio */
#define WLANRADIOON                 		1
/** Private command ID to turn off radio */
#define WLANRADIOOFF                		2
/** Private command ID to Remove AdHoc AES key */
#define WLANREMOVEADHOCAES          		3
/** Private command ID to stop AdHoc mode */
#define WLANADHOCSTOP               		4
/** Private command ID to set WLAN crypto test */
#define WLANCRYPTOTEST				6
#ifdef REASSOCIATION
/** Private command ID to set reassociation to auto mode */
#define WLANREASSOCIATIONAUTO			7
/** Private command ID to set reassociation to user mode */
#define WLANREASSOCIATIONUSER			8
#endif /* REASSOCIATION */
/** Private command ID to turn on wlan idle */
#define WLANWLANIDLEON				9
/** Private command ID to turn off wlan idle */
#define WLANWLANIDLEOFF				10

/** Private command ID to get log */
#define WLANGETLOG                  	(WLANIOCTL + 9)

/** Private command ID to set/get configurations */
#define WLAN_SETCONF_GETCONF		(WLANIOCTL + 10)

/** BG scan configuration */
#define BG_SCAN_CONFIG				1
/** BG scan configuration */
#define BG_SCAN_CFG   				3
/** Calibration data ext configuration */
#define CAL_DATA_EXT_CONFIG         2

/** Private command ID to set scan type */
#define WLANSCAN_TYPE			(WLANIOCTL + 11)

/** Private command ID to set a wext address variable */
#define WLAN_SETADDR_GETNONE  (WLANIOCTL + 12)
/** Private command ID to send deauthentication */
#define WLANDEAUTH                  		1

/** Private command ID to set/get 2k */
#define WLAN_SET_GET_2K         (WLANIOCTL + 13)
/** Private command ID to start scan */
#define WLAN_SET_USER_SCAN              1
/** Private command ID to get the scan table */
#define WLAN_GET_SCAN_TABLE             2
/** Private command ID to set Marvell TLV */
#define WLAN_SET_MRVL_TLV               3
/** Private command ID to get association response */
#define WLAN_GET_ASSOC_RSP              4
/** Private command ID to request ADDTS */
#define WLAN_ADDTS_REQ                  5
/** Private command ID to request DELTS */
#define WLAN_DELTS_REQ                  6
/** Private command ID to queue configuration */
#define WLAN_QUEUE_CONFIG               7
/** Private command ID to queue stats */
#define WLAN_QUEUE_STATS                8
/** Private command ID to get CFP table */
#define WLAN_GET_CFP_TABLE              9
/** Private command ID to set/get MEF configuration */
#define WLAN_MEF_CFG                   10
/** Private command ID to get memory */
#define WLAN_GET_MEM                   11
/** Private command ID to get Tx packet stats */
#define WLAN_TX_PKT_STATS              12

/** Private command ID to set none/get one int */
#define WLAN_SETNONE_GETONEINT		(WLANIOCTL + 15)
/** Private command ID to get region */
#define WLANGETREGION				1
/** Private command ID to get listen interval */
#define WLAN_GET_LISTEN_INTERVAL		2
/** Private command ID to get multiple DTIM */
#define WLAN_GET_MULTIPLE_DTIM			3
/** Private command ID to get Tx rate */
#define WLAN_GET_TX_RATE			4
/** Private command ID to get beacon average */
#define	WLANGETBCNAVG				5
/** Private command ID to get data average */
#define WLANGETDATAAVG				6

/** Private command ID to set ten characters and get none */
#define WLAN_SETTENCHAR_GETNONE		(WLANIOCTL + 16)
/** Private command ID to set band */
#define WLAN_SET_BAND               		1
/** Private command ID to set AdHoc channel */
#define WLAN_SET_ADHOC_CH           		2
/** Private command ID to set/get SW ann for 11h */
#define WLAN_11H_CHANSWANN          		3

/** Private command ID to set none and get ten characters */
#define WLAN_SETNONE_GETTENCHAR		(WLANIOCTL + 17)
/** Private command ID to get band */
#define WLAN_GET_BAND				1
/** Private command ID to get AdHoc channel */
#define WLAN_GET_ADHOC_CH			2

/** Private command ID to set none/get tewlve chars*/
#define WLAN_SETNONE_GETTWELVE_CHAR (WLANIOCTL + 19)
/** Private command ID to get Rx antenna */
#define WLAN_SUBCMD_GETRXANTENNA    1
/** Private command ID to get Tx antenna */
#define WLAN_SUBCMD_GETTXANTENNA    2
/** Private command ID to get TSF value */
#define WLAN_GET_TSF                3
/** Private command ID for WPS session */
#define WLAN_WPS_SESSION            4

/** Private command ID to set word character and get none */
#define WLAN_SETWORDCHAR_GETNONE	(WLANIOCTL + 20)
/** Private command ID to set AdHoc AES */
#define WLANSETADHOCAES				1

/** Private command ID to set one int/get word char */
#define WLAN_SETONEINT_GETWORDCHAR	(WLANIOCTL + 21)
/** Private command ID to get AdHoc AES key */
#define WLANGETADHOCAES				1
/** Private command ID to get version */
#define WLANVERSION				2
/** Private command ID to get extended version */
#define WLANVEREXT				3

/** Private command ID to set one int/get one int */
#define WLAN_SETONEINT_GETONEINT	(WLANIOCTL + 23)
/** Private command ID to set local power for 11h */
#define WLAN_11H_SETLOCALPOWER      		1
/** Private command ID to get WMM QoS information */
#define WLAN_WMM_QOSINFO			2
/** Private command ID to set/get listen interval */
#define	WLAN_LISTENINTRVL			3
/** Private command ID to set/get firmware wakeup method */
#define WLAN_FW_WAKEUP_METHOD			4
/** Firmware wakeup method : Unchanged */
#define WAKEUP_FW_UNCHANGED			0
/** Firmware wakeup method : Through interface */
#define WAKEUP_FW_THRU_INTERFACE		1
/** Firmware wakeup method : Through GPIO*/
#define WAKEUP_FW_THRU_GPIO			2

/** Private command ID to set/get NULL packet interval */
#define WLAN_NULLPKTINTERVAL			5
/** Private command ID to set/get beacon miss timeout */
#define WLAN_BCN_MISS_TIMEOUT			6
/** Private command ID to set/get AdHoc awake period */
#define WLAN_ADHOC_AWAKE_PERIOD			7
/** Private command ID to set/get module type */
#define WLAN_MODULE_TYPE			11
/** Private command ID to enable/disable auto Deep Sleep */
#define WLAN_AUTODEEPSLEEP			12
/** Private command ID to set/get enhance DPS */
#define WLAN_ENHANCEDPS				13
/** Private command ID to wake up MT */
#define WLAN_WAKEUP_MT				14

/** Private command ID to set one int/get none */
#define WLAN_SETONEINT_GETNONE		(WLANIOCTL + 24)
/** Private command ID to set Rx antenna */
#define WLAN_SUBCMD_SETRXANTENNA		1
/** Private command ID to set Tx antenna */
#define WLAN_SUBCMD_SETTXANTENNA		2
/** Private command ID to set authentication algorithm */
#define WLANSETAUTHALG				4
/** Private command ID to set encryption mode */
#define WLANSETENCRYPTIONMODE			5
/** Private command ID to set region */
#define WLANSETREGION				6
/** Private command ID to set listen interval */
#define WLAN_SET_LISTEN_INTERVAL		7

/** Private command ID to set multiple DTIM */
#define WLAN_SET_MULTIPLE_DTIM			8

/** Private command ID to set beacon average */
#define WLANSETBCNAVG				9
/** Private command ID to set data average */
#define WLANSETDATAAVG				10
/** Private command ID to associate */
#define WLANASSOCIATE				11

/** Private command ID to set 64-bit char/get 64-bit char */
#define WLAN_SET64CHAR_GET64CHAR	(WLANIOCTL + 25)
/** Private command ID to set/get sleep parameters */
#define WLANSLEEPPARAMS 			2
/** Private command ID to set/get BCA timeshare */
#define	WLAN_BCA_TIMESHARE			3
/** Private command ID to request TPC for 11h */
#define WLAN_11H_REQUESTTPC         		4
/** Private command ID to set power capabilities */
#define WLAN_11H_SETPOWERCAP        		5
/** Private command ID to set/get scan mode */
#define WLANSCAN_MODE				6

/** Private command ID to get AdHoc status */
#define WLAN_GET_ADHOC_STATUS			9

/** Private command ID to set generic IE */
#define WLAN_SET_GEN_IE                 	10
/** Private command ID to get generic IE */
#define WLAN_GET_GEN_IE                 	11

/** Private command ID to request MEAS */
#define WLAN_MEASREQ                    	12
/** Private command ID to get WMM queue status */
#define WLAN_WMM_QUEUE_STATUS               13
/** Private command ID to get Traffic stream status */
#define WLAN_WMM_TS_STATUS                  14

/** Private command to scan for a specific ESSID */
#define WLANEXTSCAN			(WLANIOCTL + 26)
/** Private command ID to set/get Deep Sleep mode */
#define WLANDEEPSLEEP			(WLANIOCTL + 27)
/** Deep Sleep enable flag */
#define DEEP_SLEEP_ENABLE			1
/** Deep Sleep disable flag */
#define DEEP_SLEEP_DISABLE  			0

/** Private command ID to set/get sixteen int */
#define WLAN_SET_GET_SIXTEEN_INT       (WLANIOCTL + 29)
/** Private command ID to set/get TPC configurations */
#define WLAN_TPCCFG                             1
/** Private command ID to set/get LED GPIO control */
#define WLAN_LED_GPIO_CTRL			5
/** Private command ID to set the number of probe requests per channel */
#define WLAN_SCANPROBES 			6
/** Private command ID to set/get the sleep period */
#define WLAN_SLEEP_PERIOD			7
/** Private command ID to set/get the rate set */
#define	WLAN_ADAPT_RATESET			8
/** Private command ID to set/get the inactivity timeout */
#define	WLAN_INACTIVITY_TIMEOUT			9
/** Private command ID to get the SNR */
#define WLANSNR					10
/** Private command ID to get the rate */
#define WLAN_GET_RATE				11
/** Private command ID to get Rx information */
#define	WLAN_GET_RXINFO				12
/** Private command ID to set the ATIM window */
#define	WLAN_SET_ATIM_WINDOW			13
/** Private command ID to set/get the beacon interval */
#define WLAN_BEACON_INTERVAL			14
/** Private command ID to set/get SDIO pull */
#define WLAN_SDIO_PULL_CTRL			15
/** Private command ID to set/get the scan time */
#define WLAN_SCAN_TIME				16
/** Private command ID to set/get ECL system clock */
#define WLAN_ECL_SYS_CLOCK			17
/** Private command ID to set/get the Tx control */
#define WLAN_TXCONTROL				19
/** Private command ID to set/get Host Sleep configuration */
#define WLANHSCFG				21
/** Private command ID to set Host Sleep parameters */
#define WLANHSSETPARA				22
/** Private command ID for debug configuration */
#define WLANDBGSCFG				24
#ifdef DEBUG_LEVEL1
/** Private command ID to set/get driver debug */
#define WLAN_DRV_DBG				25
#endif
/** Private command ID to set/get the max packet delay passed from drv to fw */
#define WLAN_DRV_DELAY_MAX       27

/** Private command ID to read/write Command 52 */
#define WLANCMD52RDWR			(WLANIOCTL + 30)
/** Private command ID to read/write Command 53 */
#define WLANCMD53RDWR			(WLANIOCTL + 31)
/** Command 53 buffer length */
#define CMD53BUFLEN				512

/** MAC register */
#define	REG_MAC					0x19
/** BBP register */
#define	REG_BBP					0x1a
/** RF register */
#define	REG_RF					0x1b
/** EEPROM register */
#define	REG_EEPROM				0x59

/** Command disabled */
#define	CMD_DISABLED				0
/** Command enabled */
#define	CMD_ENABLED				1
/** Command get */
#define	CMD_GET					2
/** Skip command number */
#define SKIP_CMDNUM				4
/** Skip type */
#define SKIP_TYPE				1
/** Skip size */
#define SKIP_SIZE				2
/** Skip action */
#define SKIP_ACTION				2
/** Skip type and size */
#define SKIP_TYPE_SIZE			(SKIP_TYPE + SKIP_SIZE)
/** Skip type and action */
#define SKIP_TYPE_ACTION		(SKIP_TYPE + SKIP_ACTION)

/** Maximum size of set/get configurations */
#define MAX_SETGET_CONF_SIZE		2000    /* less than MRVDRV_SIZE_OF_CMD_BUFFER */
/** Maximum length of set/get configuration commands */
#define MAX_SETGET_CONF_CMD_LEN		(MAX_SETGET_CONF_SIZE - SKIP_CMDNUM)

/* define custom events */
/** Custom event : Host Sleep activated */
#define CUS_EVT_HS_ACTIVATED		"HS_ACTIVATED "
/** Custom event : Host Sleep deactivated */
#define CUS_EVT_HS_DEACTIVATED		"HS_DEACTIVATED "
/** Custom event : Host Sleep wakeup */
#define CUS_EVT_HS_WAKEUP		"HS_WAKEUP"
/** Custom event : Beacon RSSI low */
#define CUS_EVT_BEACON_RSSI_LOW		"EVENT=BEACON_RSSI_LOW"
/** Custom event : Beacon SNR low */
#define CUS_EVT_BEACON_SNR_LOW		"EVENT=BEACON_SNR_LOW"
/** Custom event : Beacon RSSI high */
#define CUS_EVT_BEACON_RSSI_HIGH	"EVENT=BEACON_RSSI_HIGH"
/** Custom event : Beacon SNR high */
#define CUS_EVT_BEACON_SNR_HIGH		"EVENT=BEACON_SNR_HIGH"
/** Custom event : Max fail */
#define CUS_EVT_MAX_FAIL		"EVENT=MAX_FAIL"
/** Custom event : MIC failure, unicast */
#define CUS_EVT_MLME_MIC_ERR_UNI	"MLME-MICHAELMICFAILURE.indication unicast "
/** Custom event : MIC failure, multicast */
#define CUS_EVT_MLME_MIC_ERR_MUL	"MLME-MICHAELMICFAILURE.indication multicast "

/** Custom event : Data RSSI low */
#define CUS_EVT_DATA_RSSI_LOW		"EVENT=DATA_RSSI_LOW"
/** Custom event : Data SNR low */
#define CUS_EVT_DATA_SNR_LOW		"EVENT=DATA_SNR_LOW"
/** Custom event : Data RSSI high */
#define CUS_EVT_DATA_RSSI_HIGH		"EVENT=DATA_RSSI_HIGH"
/** Custom event : Data SNR high */
#define CUS_EVT_DATA_SNR_HIGH		"EVENT=DATA_SNR_HIGH"

/** Custom event : Deep Sleep awake */
#define CUS_EVT_DEEP_SLEEP_AWAKE	"EVENT=DS_AWAKE"

/** Custom event : AdHoc link sensed */
#define CUS_EVT_ADHOC_LINK_SENSED	"EVENT=ADHOC_LINK_SENSED"
/** Custom event : AdHoc beacon lost */
#define CUS_EVT_ADHOC_BCN_LOST		"EVENT=ADHOC_BCN_LOST"

/** wlan_ioctl */
typedef struct _wlan_ioctl
{
        /** Command ID */
    u16 command;
        /** data length */
    u16 len;
        /** data pointer */
    u8 *data;
} wlan_ioctl;

/** wlan_ioctl_rfantenna */
typedef struct _wlan_ioctl_rfantenna
{
    /** Action */
    u16 Action;
    /** RF Antenna mode */
    u16 AntennaMode;
} wlan_ioctl_rfantenna;

/** wlan_ioctl_regrdwr */
typedef struct _wlan_ioctl_regrdwr
{
        /** Which register to access */
    u16 WhichReg;
        /** Read or Write */
    u16 Action;
    /** Register offset */
    u32 Offset;
    /** NOB */
    u16 NOB;
    /** Value */
    u32 Value;
} wlan_ioctl_regrdwr;

/** wlan_ioctl_cfregrdwr */
typedef struct _wlan_ioctl_cfregrdwr
{
        /** Read or Write */
    u8 Action;
        /** register address */
    u16 Offset;
        /** register value */
    u16 Value;
} wlan_ioctl_cfregrdwr;

/** wlan_ioctl_adhoc_key_info */
typedef struct _wlan_ioctl_adhoc_key_info
{
    /** Action */
    u16 action;
    /** AdHoc key */
    u8 key[16];
    /** TKIP Tx MIC */
    u8 tkiptxmickey[16];
    /** TKIP Rx MIC */
    u8 tkiprxmickey[16];
} wlan_ioctl_adhoc_key_info;

/** sleep_params */
typedef struct _wlan_ioctl_sleep_params_config
{
    /** Action */
    u16 Action;
    /** Error */
    u16 Error;
    /** Offset */
    u16 Offset;
    /** Stable time */
    u16 StableTime;
    /** Calibration control */
    u8 CalControl;
    /** External sleep clock */
    u8 ExtSleepClk;
    /** Reserved */
    u16 Reserved;
} __ATTRIB_PACK__ wlan_ioctl_sleep_params_config,
    *pwlan_ioctl_sleep_params_config;

/** BCA TIME SHARE */
typedef struct _wlan_ioctl_bca_timeshare_config
{
        /** ACT_GET/ACT_SET */
    u16 Action;
        /** Type: WLAN, BT */
    u16 TrafficType;
        /** Interval: 20msec - 60000msec */
    u32 TimeShareInterval;
        /** PTA arbiter time in msec */
    u32 BTTime;
} __ATTRIB_PACK__ wlan_ioctl_bca_timeshare_config,
    *pwlan_ioctl_bca_timeshare_config;

/** Maximum number of CFPs in the list */
#define MAX_CFP_LIST_NUM	64

/** wlan_ioctl_cfp_table */
typedef struct _wlan_ioctl_cfp_table
{
    /** Region */
    u32 region;
    /** CFP number */
    u32 cfp_no;
    struct
    {
        /** CFP channel */
        u16 Channel;
        /** CFP frequency */
        u32 Freq;
        /** Maximum Tx power */
        u16 MaxTxPower;
        /** Unsupported flag */
        u8 Unsupported;
    } cfp[MAX_CFP_LIST_NUM];
} __ATTRIB_PACK__ wlan_ioctl_cfp_table, *pwlan_ioctl_cfp_table;

#endif /* _WLAN_WEXT_H_ */

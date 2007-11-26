/*
 * Copyright(c) 2005 - 2006 Attansic Corporation. All rights reserved.
 * Copyright(c) 2006 xiong huang <xiong.huang@atheros.com>
 *
 * Derived from Intel e1000 driver
 * Copyright(c) 1999 - 2005 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * There are a lot of defines in here that are unused and/or have cryptic
 * names.  Please leave them alone, as they're the closest thing we have
 * to a spec from Attansic at present. *ahem* -- CHS
 */
 
#include <linux/netdevice.h>
#include "at.h"

/* This is the only thing that needs to be changed to adjust the
 * maximum number of ports that the driver can manage.
 */

#define AT_MAX_NIC 32

#define OPTION_UNSET    -1
#define OPTION_DISABLED 0
#define OPTION_ENABLED  1



 /* All parameters are treated the same, as an integer array of values.
 * This macro just reduces the need to repeat the same declaration code
 * over and over (plus this helps to avoid typo bugs).
 */
#define AT_PARAM_INIT { [0 ... AT_MAX_NIC] = OPTION_UNSET }
#ifndef module_param_array
/* Module Parameters are always initialized to -1, so that the driver
 * can tell the difference between no user specified value or the
 * user asking for the default value.
 * The true default values are loaded in when at_check_options is called.
 *
 * This is a GCC extension to ANSI C.
 * See the item "Labeled Elements in Initializers" in the section
 * "Extensions to the C Language Family" of the GCC documentation.
 */

#define AT_PARAM(X, desc) \
    static const int __devinitdata X[AT_MAX_NIC+1] = AT_PARAM_INIT; \
    MODULE_PARM(X, "1-" __MODULE_STRING(AT_MAX_NIC) "i"); \
    MODULE_PARM_DESC(X, desc);
#else
#define AT_PARAM(X, desc) \
    static int __devinitdata X[AT_MAX_NIC+1] = AT_PARAM_INIT; \
    static int num_##X = 0; \
    module_param_array_named(X, X, int, &num_##X, 0); \
    MODULE_PARM_DESC(X, desc);
#endif

/* Transmit Memory Size
 *
 * Valid Range: 64-2048 
 *
 * Default Value: 128
 */
#define AT_MIN_TX_MEMSIZE       4        	// 4KB
#define AT_MAX_TX_MEMSIZE       64     		// 64KB
#define AT_DEFAULT_TX_MEMSIZE   8        	// 8KB
AT_PARAM(TxMemSize, "Bytes of Transmit Memory");

/* Receive Memory Block Count
 *
 * Valid Range: 16-512
 *
 * Default Value: 128
 */
#define AT_MIN_RXD_COUNT                16
#define AT_MAX_RXD_COUNT                512
#define AT_DEFAULT_RXD_COUNT            64
AT_PARAM(RxMemBlock, "Number of receive memory block");

/* User Specified MediaType Override
 *
 * Valid Range: 0-5
 *  - 0    - auto-negotiate at all supported speeds
 *  - 1    - only link at 1000Mbps Full Duplex
 *  - 2    - only link at 100Mbps Full Duplex
 *  - 3    - only link at 100Mbps Half Duplex
 *  - 4    - only link at 10Mbps Full Duplex
 *  - 5    - only link at 10Mbps Half Duplex
 * Default Value: 0
 */

AT_PARAM(MediaType, "MediaType Select");

/* Interrupt Moderate Timer in units of 2 us
 *
 * Valid Range: 10-65535
 *
 * Default Value: 45000(90ms)
 */
#define INT_MOD_DEFAULT_CNT             100 // 200us
#define INT_MOD_MAX_CNT                 65000
#define INT_MOD_MIN_CNT                 50
AT_PARAM(IntModTimer, "Interrupt Moderator Timer");



/* FlashVendor
 * Valid Range: 0-2
 * 0 - Atmel
 * 1 - SST
 * 2 - ST
 */

AT_PARAM(FlashVendor, "SPI Flash Vendor");



#define AUTONEG_ADV_DEFAULT  0x2F
#define AUTONEG_ADV_MASK     0x2F
#define FLOW_CONTROL_DEFAULT FLOW_CONTROL_FULL



#define FLASH_VENDOR_DEFAULT    0
#define FLASH_VENDOR_MIN        0
#define FLASH_VENDOR_MAX        2


struct at_option {
    enum { enable_option, range_option, list_option } type;
    char *name;
    char *err;
    int  def;
    union {
        struct { /* range_option info */
            int min;
            int max;
        } r;
        struct { /* list_option info */
            int nr;
            struct at_opt_list { int i; char *str; } *p;
        } l;
    } arg;
};

static int __devinit
at_validate_option(int *value, struct at_option *opt)
{
    if(*value == OPTION_UNSET) {
        *value = opt->def;
        return 0;
    }

    switch (opt->type) {
    case enable_option:
        switch (*value) {
        case OPTION_ENABLED:
            printk(KERN_INFO "%s Enabled\n", opt->name);
            return 0;
        case OPTION_DISABLED:
            printk(KERN_INFO "%s Disabled\n", opt->name);
            return 0;
        }
        break;
    case range_option:
        if(*value >= opt->arg.r.min && *value <= opt->arg.r.max) {
            printk(KERN_INFO "%s set to %i\n", opt->name, *value);
            return 0;
        }
        break;
    case list_option: {
        int i;
        struct at_opt_list *ent;

        for(i = 0; i < opt->arg.l.nr; i++) {
            ent = &opt->arg.l.p[i];
            if(*value == ent->i) {
                if(ent->str[0] != '\0')
                    printk(KERN_INFO "%s\n", ent->str);
                return 0;
            }
        }
    }
        break;
    default:
        BUG();
    }

    printk(KERN_INFO "Invalid %s specified (%i) %s\n",
           opt->name, *value, opt->err);
    *value = opt->def;
    return -1;
}

/**
 * at_check_options - Range Checking for Command Line Parameters
 * @adapter: board private structure
 *
 * This routine checks all command line parameters for valid user
 * input.  If an invalid value is given, or if no user specified
 * value exists, a default value is used.  The final value is stored
 * in a variable in the adapter structure.
 **/

void __devinit
at_check_options(struct at_adapter *adapter)
{
    int bd = adapter->bd_number;
    if(bd >= AT_MAX_NIC) {
        printk(KERN_NOTICE
               "Warning: no configuration for board #%i\n", bd);
        printk(KERN_NOTICE "Using defaults for all values\n");
#ifndef module_param_array
        bd = AT_MAX_NIC;
#endif
    }

    { /* Bytes of Transmit Memory */
        struct at_option opt = {
            .type = range_option,
            .name = "Bytes of Transmit Memory",
            .err  = "using default of "
                __MODULE_STRING(AT_DEFAULT_TX_MEMSIZE),
            .def  = AT_DEFAULT_TX_MEMSIZE,
            .arg  = { .r = { .min = AT_MIN_TX_MEMSIZE, .max = AT_MAX_TX_MEMSIZE }}
        };
        int val;
#ifdef module_param_array
        if(num_TxMemSize > bd) {
#endif
            val = TxMemSize[bd];
            at_validate_option(&val, &opt);
            adapter->txd_ring_size = ((u32) val) * 1024;
#ifdef module_param_array
        } else {
            adapter->txd_ring_size = ((u32)opt.def) * 1024;
        }
#endif
        // txs ring size:
        adapter->txs_ring_size = adapter->txd_ring_size / 128;
        if (adapter->txs_ring_size > 160)
            adapter->txs_ring_size = 160;
    }

    { /* Receive Memory Block Count */
        struct at_option opt = {
            .type = range_option,
            .name = "Number of receive memory block",
            .err  = "using default of "
                __MODULE_STRING(AT_DEFAULT_RXD_COUNT),
            .def  = AT_DEFAULT_RXD_COUNT,
            .arg  = { .r = { .min = AT_MIN_RXD_COUNT, .max = AT_MAX_RXD_COUNT }}
        };
        int val;
#ifdef module_param_array
        if(num_RxMemBlock > bd) {
#endif          
            val = RxMemBlock[bd];
            at_validate_option(&val, &opt);
            adapter->rxd_ring_size = (u32)val; //((u16)val)&~1; // even number
#ifdef module_param_array
        } else {
            adapter->rxd_ring_size = (u32)opt.def;
        }
#endif

        // init RXD Flow control value
        adapter->hw.fc_rxd_hi = (adapter->rxd_ring_size/8)*7;
        adapter->hw.fc_rxd_lo = (AT_MIN_RXD_COUNT/8) > (adapter->rxd_ring_size/12) ?
                                (AT_MIN_RXD_COUNT/8) : (adapter->rxd_ring_size/12);
    }
    
    { /* Interrupt Moderate Timer */
        struct at_option opt = { 
            .type = range_option,
            .name = "Interrupt Moderate Timer",
            .err  = "using default of " __MODULE_STRING(INT_MOD_DEFAULT_CNT),
            .def  = INT_MOD_DEFAULT_CNT,
            .arg  = { .r = { .min = INT_MOD_MIN_CNT, .max = INT_MOD_MAX_CNT }}
        } ;
        int val;
#ifdef module_param_array
        if(num_IntModTimer > bd) {
#endif          
        val = IntModTimer[bd];
        at_validate_option(&val, &opt); 
        adapter->imt = (u16) val;   
#ifdef module_param_array
        } else {
            adapter->imt = (u16)(opt.def);
        }
#endif               
    }
    
    { /* Flash Vendor */
        struct at_option opt = { 
            .type = range_option,
            .name = "SPI Flash Vendor",
            .err  = "using default of " __MODULE_STRING(FLASH_VENDOR_DEFAULT),
            .def  = FLASH_VENDOR_DEFAULT,
            .arg  = { .r = { .min = FLASH_VENDOR_MIN, .max = FLASH_VENDOR_MAX }}
        } ;
        int val;
#ifdef module_param_array
        if(num_FlashVendor > bd) {
#endif          
        val = FlashVendor[bd];
        at_validate_option(&val, &opt); 
        adapter->hw.flash_vendor = (u8) val;   
#ifdef module_param_array
        } else {
            adapter->hw.flash_vendor = (u8)(opt.def);
        }
#endif               
    }
    
    { /* MediaType */
        struct at_option opt = { 
	        .type = range_option,
	        .name = "Speed/Duplex Selection",
	        .err  = "using default of " __MODULE_STRING(MEDIA_TYPE_AUTO_SENSOR),
	        .def  = MEDIA_TYPE_AUTO_SENSOR,
	        .arg  = { .r = { .min = MEDIA_TYPE_AUTO_SENSOR, .max = MEDIA_TYPE_10M_HALF }}
	    } ;
        int val;
#ifdef module_param_array
	if(num_MediaType > bd) {
#endif	        
        val = MediaType[bd];
	at_validate_option(&val, &opt);	
	adapter->hw.MediaType = (u16) val;
#ifdef module_param_array
	} else {
	    adapter->hw.MediaType = (u16)(opt.def);
	}
#endif	             
    }
}




/* Plz read readme file for Software License information */

#ifndef __RT3370_H__
#define __RT3370_H__

#ifdef RT3370


#ifndef RTMP_USB_SUPPORT
#error "For RT3070, you should define the compile flag -DRTMP_USB_SUPPORT"
#endif

#ifndef RTMP_MAC_USB
#error "For RT3070, you should define the compile flag -DRTMP_MAC_USB"
#endif

#ifndef RTMP_RF_RW_SUPPORT
#error "For RT3070, you should define the compile flag -DRTMP_RF_RW_SUPPORT"
#endif

#ifndef RT33xx
#error "For RT3070, you should define the compile flag -DRT30xx"
#endif

#include "chip/mac_usb.h"
#include "chip/rt33xx.h"

//
// Device ID & Vendor ID, these values should match EEPROM value
//

#endif // RT3370 //

#endif //__RT3370_H__ //


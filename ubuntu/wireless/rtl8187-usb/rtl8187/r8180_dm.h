/* 
       Hardware dynamic mechanism for RTL8187B. 
Notes:	
	This file is ported from RTL8187B Windows driver
*/

#ifndef R8180_DM_H
#define R8180_DM_H

#include "r8187.h"

bool CheckDig(struct net_device *dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void rtl8180_hw_dig_wq (struct work_struct *work);
#else
void rtl8180_hw_dig_wq(struct net_device *dev);
#endif

bool CheckHighPower(struct net_device *dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void rtl8180_tx_pw_wq (struct work_struct *work);
#else
void rtl8180_tx_pw_wq(struct net_device *dev);
#endif


#endif //R8180_PM_H

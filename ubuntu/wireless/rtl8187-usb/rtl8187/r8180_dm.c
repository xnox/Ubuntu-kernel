/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
 	r8180_dig.c
	
Abstract:
 	Hardware dynamic mechanism for RTL8187B
 	    
Major Change History:
	When        	Who      	What
	----------    ---------------   -------------------------------
	2006-11-15     david		Created

Notes:	
	This file is ported from RTL8187B Windows driver.
	
 	
--*/
#include "r8180_dm.h"
#include "r8180_hw.h"
#include "r8180_rtl8225.h"

//================================================================================
//	Local Constant.
//================================================================================
#define Z1_HIPWR_UPPER_TH			99
#define Z1_HIPWR_LOWER_TH			70	
#define Z2_HIPWR_UPPER_TH			99
#define Z2_HIPWR_LOWER_TH			90

bool CheckDig(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;

	if(ieee->state != IEEE80211_LINKED)
		return false;

	if(priv->card_8187 == NIC_8187B) {
		//
		// We need to schedule dig workitem on either of the below mechanisms.
		// By Bruce, 2007-06-01.
		//
		if(!priv->bDigMechanism && !priv->bCCKThMechanism)
			return false;

		if(priv->CurrentOperaRate < 36) // Schedule Dig under all OFDM rates. By Bruce, 2007-06-01.
			return false;
	} else { 
		if(!priv->bDigMechanism)
			return false;

		if(priv->CurrentOperaRate < 48)
			return false;
	}
	return true;
}


//
//	Description:
//		Implementation of DIG for Zebra and Zebra2.	
//
void DIG_Zebra(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	//PHAL_DATA_8187	pHalData = GetHalData8187(Adapter);
	u16			CCKFalseAlarm, OFDMFalseAlarm;
	u16			OfdmFA1, OfdmFA2;
	int			InitialGainStep = 7; // The number of initial gain stages.
	int			LowestGainStage = 4; // The capable lowest stage of performing dig workitem.

//	printk("---------> DIG_Zebra()\n");

	//Read only 1 byte because of HW bug. This is a temporal modification. Joseph
	// Modify by Isaiah 2006-06-27
	if(priv->card_8187_Bversion == VERSION_8187B_B)
	{
		CCKFalseAlarm = 0;
		OFDMFalseAlarm = (u16)(priv->FalseAlarmRegValue);
		OfdmFA1 =  0x01;
		OfdmFA2 = priv->RegDigOfdmFaUpTh; 
	}
	else
	{
		CCKFalseAlarm = (u16)(priv->FalseAlarmRegValue & 0x0000ffff);
		OFDMFalseAlarm = (u16)((priv->FalseAlarmRegValue >> 16) & 0x0000ffff);
		OfdmFA1 =  0x15;
		//OfdmFA2 =  0xC00;
		OfdmFA2 = ((u16)(priv->RegDigOfdmFaUpTh)) << 8;
	}	

//	printk("DIG**********CCK False Alarm: %#X \n",CCKFalseAlarm);
//	printk("DIG**********OFDM False Alarm: %#X \n",OFDMFalseAlarm);



	// The number of initial gain steps is different, by Bruce, 2007-04-13.
	if(priv->card_8187 == NIC_8187) {
		if (priv->InitialGain == 0 ) //autoDIG
		{
			switch( priv->rf_chip)
			{
				case RF_ZEBRA:
					priv->InitialGain = 5; // m74dBm;
					break;
				case RF_ZEBRA2:
					priv->InitialGain = 4; // m78dBm;
					break;
				default:
					priv->InitialGain = 5; // m74dBm;
					break;
			}
		}
		InitialGainStep = 7;
		if(priv->InitialGain > 7)
			priv->InitialGain = 5;
		LowestGainStage = 4;
	} else {
		if (priv->InitialGain == 0 ) //autoDIG
		{ // Advised from SD3 DZ, by Bruce, 2007-06-05.
			priv->InitialGain = 4; // In 87B, m74dBm means State 4 (m82dBm)
		}
		if(priv->card_8187_Bversion != VERSION_8187B_B)
		{ // Advised from SD3 DZ, by Bruce, 2007-06-05.
			OfdmFA1 =  0x20;
		}
		InitialGainStep = 8;
		LowestGainStage = priv->RegBModeGainStage; // Lowest gain stage.
	}

	if (OFDMFalseAlarm > OfdmFA1)
	{
		if (OFDMFalseAlarm > OfdmFA2)
		{
			priv->DIG_NumberFallbackVote++;
			if (priv->DIG_NumberFallbackVote >1)
			{
				//serious OFDM  False Alarm, need fallback
				// By Bruce, 2007-03-29.
				// if (pHalData->InitialGain < 7) // In 87B, m66dBm means State 7 (m74dBm)
				if (priv->InitialGain < InitialGainStep)
				{
					priv->InitialGain = (priv->InitialGain + 1);
					//printk("DIG**********OFDM False Alarm: %#X,  OfdmFA1: %#X, OfdmFA2: %#X\n", OFDMFalseAlarm, OfdmFA1, OfdmFA2);
					//printk("DIG+++++++ fallback OFDM:%d \n", priv->InitialGain);
					UpdateInitialGain(dev); // 2005.01.06, by rcnjko.
				}
				priv->DIG_NumberFallbackVote = 0;
				priv->DIG_NumberUpgradeVote=0;
			}
		}
		else
		{
			if (priv->DIG_NumberFallbackVote)
				priv->DIG_NumberFallbackVote--;
		}
		priv->DIG_NumberUpgradeVote=0;		
	}
	else	//OFDM False Alarm < 0x15
	{
		if (priv->DIG_NumberFallbackVote)
			priv->DIG_NumberFallbackVote--;
		priv->DIG_NumberUpgradeVote++;

		if (priv->DIG_NumberUpgradeVote>9)
		{
			if (priv->InitialGain > LowestGainStage) // In 87B, m78dBm means State 4 (m864dBm)
			{
				priv->InitialGain = (priv->InitialGain - 1);
				//printk("DIG**********OFDM False Alarm: %#X,  OfdmFA1: %#X, OfdmFA2: %#X\n", OFDMFalseAlarm, OfdmFA1, OfdmFA2);
				//printk("DIG--------- Upgrade OFDM:%d \n", priv->InitialGain);
				UpdateInitialGain(dev); // 2005.01.06, by rcnjko.
			}
			priv->DIG_NumberFallbackVote = 0;
			priv->DIG_NumberUpgradeVote=0;
		}
	}

//	printk("DIG+++++++ OFDM:%d\n", priv->InitialGain);	
//	printk("<--------- DIG_Zebra()\n");
}

//
//	Description:
//		Dispatch DIG implementation according to RF. 	
//
void DynamicInitGain(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	switch(priv->rf_chip)
	{
		case RF_ZEBRA:
		case RF_ZEBRA2:  // [AnnieWorkaround] For Zebra2, 2005-08-01.
		//case RF_ZEBRA4:
			DIG_Zebra(dev);
			break;
		
		default:
			printk("DynamicInitGain(): unknown RFChipID(%d) !!!\n", priv->rf_chip);
			break;
	}
}

// By Bruce, 2007-03-29.
//
//	Description:
//		Dispatch CCK Power Detection implementation according to RF.	
//
void DynamicCCKThreshold(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u16			CCK_Up_Th;
	u16			CCK_Lw_Th;
	u16			CCKFalseAlarm;

	printk("=====>DynamicCCKThreshold()\n");

	CCK_Up_Th = priv->CCKUpperTh;
	CCK_Lw_Th = priv->CCKLowerTh;	
	CCKFalseAlarm = (u16)((priv->FalseAlarmRegValue & 0x0000ffff) >> 8); // We only care about the higher byte.	
	printk("DynamicCCKThreshold(): CCK Upper Threshold: 0x%02X, Lower Threshold: 0x%02X, CCKFalseAlarmHighByte: 0x%02X\n", CCK_Up_Th, CCK_Lw_Th, CCKFalseAlarm);

	if(priv->StageCCKTh < 3 && CCKFalseAlarm >= CCK_Up_Th)
	{
		priv->StageCCKTh ++;
		UpdateCCKThreshold(dev);
	}
	else if(priv->StageCCKTh > 0 && CCKFalseAlarm <= CCK_Lw_Th)
	{
		priv->StageCCKTh --;
		UpdateCCKThreshold(dev);
	}
	
	printk("<=====DynamicCCKThreshold()\n");
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void rtl8180_hw_dig_wq (struct work_struct *work)
{
//      struct r8180_priv *priv = container_of(work, struct r8180_priv, watch_dog_wq);
//      struct ieee80211_device * ieee = (struct ieee80211_device*)
//                                             container_of(work, struct ieee80211_device, watch_dog_wq);
        struct delayed_work *dwork = container_of(work,struct delayed_work,work);
        struct ieee80211_device *ieee = container_of(work,struct ieee80211_device,hw_dig_wq);
        struct net_device *dev = ieee->dev;
#else
void rtl8180_hw_dig_wq(struct net_device *dev)
{
	// struct r8180_priv *priv = ieee80211_priv(dev);
#endif
	struct r8180_priv *priv = ieee80211_priv(dev);

	// Read CCK and OFDM False Alarm.
	if(priv->card_8187_Bversion == VERSION_8187B_B) {
		// Read only 1 byte because of HW bug. This is a temporal modification. Joseph
		// Modify by Isaiah 2006-06-27
		priv->FalseAlarmRegValue = (u32)read_nic_byte(dev, (OFDM_FALSE_ALARM+1));
	} else {
		priv->FalseAlarmRegValue = read_nic_dword(dev, CCK_FALSE_ALARM);
	}

	// Adjust Initial Gain dynamically.
	if(priv->bDigMechanism) {
		DynamicInitGain(dev);
	}

	//
	// Move from DynamicInitGain to be independent of the OFDM DIG mechanism, by Bruce, 2007-06-01.
	//
	if(priv->card_8187 == NIC_8187B) {
		// By Bruce, 2007-03-29.
		// Dynamically update CCK Power Detection Threshold.
		if(priv->bCCKThMechanism)
		{
			DynamicCCKThreshold(dev);
		}
	}
}

void SetTxPowerLevel8187(struct net_device *dev, short chan)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	switch(priv->rf_chip)
	{
	case  RF_ZEBRA:
		rtl8225_SetTXPowerLevel(dev,chan);
		break;

	case RF_ZEBRA2:
	//case RF_ZEBRA4:
		rtl8225z2_SetTXPowerLevel(dev,chan);
		break;
	}
}

//
//	Description:
//		Check if input power signal strength exceeds maximum input power threshold 
//		of current HW. 
//		If yes, we set our HW to high input power state:
//			RX: always force TR switch to SW Tx mode to reduce input power. 
//			TX: turn off smaller Tx output power (see RtUsbCheckForHang).
//
//		If no, we restore our HW to normal input power state:
///			RX: restore TR switch to HW controled mode.
//			TX: restore TX output power (see RtUsbCheckForHang).
//
//	TODO: 
//		1. Tx power control shall not be done in Platform-dependent timer (e.g. RtUsbCheckForHang). 
//		2. Allow these threshold adjustable by RF SD.
//
void DoRxHighPower(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	TR_SWITCH_STATE TrSwState;
	u16		HiPwrUpperTh = 0;
	u16		HiPwrLowerTh = 0;
	u16		RSSIHiPwrUpperTh;
	u16		RSSIHiPwrLowerTh;	

	//87S remove TrSwitch mechanism
	if((priv->card_8187 == NIC_8187B)||(priv->card_8187 == NIC_8187)) {

		//printk("----> DoRxHighPower()\n");

		//
		// Get current TR switch setting.
		//
		//Adapter->HalFunc.GetHwRegHandler(Adapter, HW_VAR_TR_SWITCH, (pu1Byte)(&TrSwState));
		TrSwState = priv->TrSwitchState;

		//
		// Determine threshold according to RF type.
		//
		switch(priv->rf_chip)
		{
			case RF_ZEBRA:
				HiPwrUpperTh = Z1_HIPWR_UPPER_TH;
				HiPwrLowerTh = Z1_HIPWR_LOWER_TH;
				printk("DoRxHighPower(): RF_ZEBRA, Upper Threshold: %d LOWER Threshold: %d\n", 
						HiPwrUpperTh, HiPwrLowerTh);
				break;	

			case RF_ZEBRA2:
				if((priv->card_8187 == NIC_8187)) {
					HiPwrUpperTh = Z2_HIPWR_UPPER_TH;
					HiPwrLowerTh = Z2_HIPWR_LOWER_TH;
				} else {
					// By Bruce, 2007-04-11.
					// HiPwrUpperTh = Z2_HIPWR_UPPER_TH;
					// HiPwrLowerTh = Z2_HIPWR_LOWER_TH;

					HiPwrUpperTh = priv->Z2HiPwrUpperTh;
					HiPwrLowerTh = priv->Z2HiPwrLowerTh;
					HiPwrUpperTh = HiPwrUpperTh * 10;
					HiPwrLowerTh = HiPwrLowerTh * 10;

					RSSIHiPwrUpperTh = priv->Z2RSSIHiPwrUpperTh;
					RSSIHiPwrLowerTh = priv->Z2RSSIHiPwrLowerTh;
					//printk("DoRxHighPower(): RF_ZEBRA2, Upper Threshold: %d LOWER Threshold: %d, RSSI Upper Th: %d, RSSI Lower Th: %d\n",HiPwrUpperTh, HiPwrLowerTh, RSSIHiPwrUpperTh, RSSIHiPwrLowerTh);
				}
				break;	

			default:
				printk("DoRxHighPower(): Unknown RFChipID(%d), UndecoratedSmoothedSS(%d), TrSwState(%d)!!!\n", 
						priv->rf_chip, priv->UndecoratedSmoothedSS, TrSwState);
				return;
				break;	
		}

		//printk(">>>>>>>>>>Set TR switch to software control, UndecoratedSmoothedSS:%d, CurCCKRSSI = %d\n",\
					priv->UndecoratedSmoothedSS, priv->CurCCKRSSI);
		if((priv->card_8187 == NIC_8187)) {
			//
			// Perform Rx part High Power Mechanism by UndecoratedSmoothedSS.
			//
			if (priv->UndecoratedSmoothedSS > HiPwrUpperTh)
			{ //  High input power state.
				if( priv->TrSwitchState == TR_HW_CONTROLLED )	
				{
				//	printk(">>>>>>>>>>Set TR switch to software control, UndecoratedSmoothedSS:%d \n", \
							priv->UndecoratedSmoothedSS);
				//	printk(">>>>>>>>>> TR_SW_TX\n");
					write_nic_byte(dev, RFPinsSelect, 
							(u8)(priv->wMacRegRfPinsSelect | TR_SW_MASK_8187 ));
					write_nic_byte(dev, RFPinsOutput, 
						(u8)((priv->wMacRegRfPinsOutput&(~TR_SW_MASK_8187))|TR_SW_MASK_TX_8187));
					priv->TrSwitchState = TR_SW_TX;
					priv->bToUpdateTxPwr = true;
				}
			}
			else if (priv->UndecoratedSmoothedSS < HiPwrLowerTh)
			{ // Normal input power state. 
				if( priv->TrSwitchState == TR_SW_TX)	
				{
				//	printk("<<<<<<<<<<<Set TR switch to hardware control UndecoratedSmoothedSS:%d \n", \
							priv->UndecoratedSmoothedSS);
				//	printk("<<<<<<<<<< TR_HW_CONTROLLED\n");
					write_nic_byte(dev, RFPinsOutput, (u8)(priv->wMacRegRfPinsOutput));
					write_nic_byte(dev, RFPinsSelect, (u8)(priv->wMacRegRfPinsSelect));
					priv->TrSwitchState = TR_HW_CONTROLLED;
					priv->bToUpdateTxPwr = true;
				}
			}
		}else {
			//printk("=====>TrSwState = %s\n", (TrSwState==TR_HW_CONTROLLED)?"TR_HW_CONTROLLED":"TR_SW_TX");
			//printk("UndecoratedSmoothedSS:%d, CurCCKRSSI = %d\n",priv->UndecoratedSmoothedSS, priv->CurCCKRSSI);
			// Asked by SD3 DZ, by Bruce, 2007-04-12.
			if(TrSwState == TR_HW_CONTROLLED)
			{
				if((priv->UndecoratedSmoothedSS > HiPwrUpperTh) ||
						(priv->bCurCCKPkt && (priv->CurCCKRSSI > RSSIHiPwrUpperTh)))
				{
					//printk("===============================> high power!\n");
					//printk(">>>>>>>>>>Set TR switch to software control, UndecoratedSmoothedSS:%d, CurCCKRSSI = %d\n", \
							priv->UndecoratedSmoothedSS, priv->CurCCKRSSI);
					//printk(">>>>>>>>>> TR_SW_TX\n");
					write_nic_byte(dev, RFPinsSelect, (u8)(priv->wMacRegRfPinsSelect|TR_SW_MASK_8187 ));
					write_nic_byte(dev, RFPinsOutput, 
						(u8)((priv->wMacRegRfPinsOutput&(~TR_SW_MASK_8187))|TR_SW_MASK_TX_8187));
					priv->TrSwitchState = TR_SW_TX;
					priv->bToUpdateTxPwr = true;
				}
			}
			else
			{
				if((priv->UndecoratedSmoothedSS < HiPwrLowerTh) &&
						(!priv->bCurCCKPkt || priv->CurCCKRSSI < RSSIHiPwrLowerTh))
				{
					//printk("===============================> normal power!\n");
					//printk("<<<<<<<<<<<Set TR switch to hardware control UndecoratedSmoothedSS:%d, CurCCKRSSI = %d \n", \
							priv->UndecoratedSmoothedSS, priv->CurCCKRSSI);

					//printk("<<<<<<<<<< TR_HW_CONTROLLED\n");
					write_nic_byte(dev, RFPinsOutput, (u8)(priv->wMacRegRfPinsOutput));
					write_nic_byte(dev, RFPinsSelect, (u8)(priv->wMacRegRfPinsSelect));
					priv->TrSwitchState = TR_HW_CONTROLLED;
					priv->bToUpdateTxPwr = true;
				}
			}
			//printk("<=======TrSwState = %s\n", (TrSwState==TR_HW_CONTROLLED)?"TR_HW_CONTROLLED":"TR_SW_TX");
		}
		//printk("<---- DoRxHighPower()\n");
	}
}


//
//	Description:
//		Callback function of UpdateTxPowerWorkItem.
//		Because of some event happend, e.g. CCX TPC, High Power Mechanism, 
//		We update Tx power of current channel again. 
//
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
void rtl8180_tx_pw_wq (struct work_struct *work)
{
//      struct r8180_priv *priv = container_of(work, struct r8180_priv, watch_dog_wq);
//      struct ieee80211_device * ieee = (struct ieee80211_device*)
//                                             container_of(work, struct ieee80211_device, watch_dog_wq);
        struct delayed_work *dwork = container_of(work,struct delayed_work,work);
        struct ieee80211_device *ieee = container_of(work,struct ieee80211_device,tx_pw_wq);
        struct net_device *dev = ieee->dev;
#else
void rtl8180_tx_pw_wq(struct net_device *dev)
{
	// struct r8180_priv *priv = ieee80211_priv(dev);
#endif

	struct r8180_priv *priv = ieee80211_priv(dev);

	//printk("----> UpdateTxPowerWorkItemCallback()\n");
	
	if(priv->bToUpdateTxPwr)
	{
		//printk("DoTxHighPower(): schedule UpdateTxPowerWorkItem......\n");
		priv->bToUpdateTxPwr = false;
		SetTxPowerLevel8187(dev, priv->chan);
	}
	
	DoRxHighPower(dev);	
	//printk("<---- UpdateTxPowerWorkItemCallback()\n");
}

//
//	Description:
//		Return TRUE if we shall perform High Power Mecahnism, FALSE otherwise.	
//
bool CheckHighPower(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;

	if(!priv->bRegHighPowerMechanism)
	{
		return false;
	}
		
	if((ieee->state == IEEE80211_LINKED_SCANNING)||(ieee->state == IEEE80211_MESH_SCANNING))
	{
		return false;
	}

	return true;
}



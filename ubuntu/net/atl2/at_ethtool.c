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

/* ethtool support for at */

#include <linux/netdevice.h>

#ifdef SIOCETHTOOL
#include <linux/ethtool.h>

#include "at.h"

#ifdef ETHTOOL_OPS_COMPAT
#include "kcompat_ethtool.c"
#endif

extern char at_driver_name[];
extern char at_driver_version[];

extern int at_up(struct at_adapter *adapter);
extern void at_down(struct at_adapter *adapter);
extern void at_reinit_locked(struct at_adapter *adapter);
extern int32_t at_reset_hw(struct at_hw *hw);

static int
at_get_settings(struct net_device *netdev, struct ethtool_cmd *ecmd)
{
	struct at_adapter *adapter = netdev_priv(netdev);
	struct at_hw *hw = &adapter->hw;

	ecmd->supported = (SUPPORTED_10baseT_Half |
		               SUPPORTED_10baseT_Full |
		               SUPPORTED_100baseT_Half |
		               SUPPORTED_100baseT_Full |
		               SUPPORTED_Autoneg |
		               SUPPORTED_TP);
	ecmd->advertising = ADVERTISED_TP;

	ecmd->advertising |= ADVERTISED_Autoneg;
	ecmd->advertising |= hw->autoneg_advertised;
	
	ecmd->port = PORT_TP;
	ecmd->phy_address = 0;
	ecmd->transceiver = XCVR_INTERNAL;

	if (adapter->link_speed != SPEED_0) {
		ecmd->speed = adapter->link_speed;
		if (adapter->link_duplex == FULL_DUPLEX)
			ecmd->duplex = DUPLEX_FULL;
		else
			ecmd->duplex = DUPLEX_HALF;
	} else {
		ecmd->speed = -1;
		ecmd->duplex = -1;
	}

	ecmd->autoneg = AUTONEG_ENABLE;
	return 0;
}

static int
at_set_settings(struct net_device *netdev, struct ethtool_cmd *ecmd)
{
	struct at_adapter *adapter = netdev_priv(netdev);
	struct at_hw *hw = &adapter->hw;

	while (test_and_set_bit(__AT_RESETTING, &adapter->flags))
		msleep(1);

	if (ecmd->autoneg == AUTONEG_ENABLE) {

#define MY_ADV_MASK	(ADVERTISE_10_HALF| \
					 ADVERTISE_10_FULL| \
					 ADVERTISE_100_HALF| \
					 ADVERTISE_100_FULL)

		if ((ecmd->advertising&MY_ADV_MASK) == MY_ADV_MASK) {
			hw->MediaType = MEDIA_TYPE_AUTO_SENSOR;
			hw->autoneg_advertised =  MY_ADV_MASK;
		} else if ((ecmd->advertising&MY_ADV_MASK) == ADVERTISE_100_FULL) {
			hw->MediaType = MEDIA_TYPE_100M_FULL;
			hw->autoneg_advertised = ADVERTISE_100_FULL;
		} else if ((ecmd->advertising&MY_ADV_MASK) == ADVERTISE_100_HALF) {
			hw->MediaType = MEDIA_TYPE_100M_HALF;
			hw->autoneg_advertised = ADVERTISE_100_HALF;
		} else if ((ecmd->advertising&MY_ADV_MASK) == ADVERTISE_10_FULL) {
			hw->MediaType = MEDIA_TYPE_10M_FULL;
			hw->autoneg_advertised = ADVERTISE_10_FULL;
		}  else if ((ecmd->advertising&MY_ADV_MASK) == ADVERTISE_10_HALF) {
			hw->MediaType = MEDIA_TYPE_10M_HALF;
			hw->autoneg_advertised = ADVERTISE_10_HALF;
		} else {
			clear_bit(__AT_RESETTING, &adapter->flags);
			return -EINVAL;
		}
		ecmd->advertising = hw->autoneg_advertised 
						| ADVERTISED_TP | ADVERTISED_Autoneg;
	} else {
		clear_bit(__AT_RESETTING, &adapter->flags);
		return -EINVAL;
	}

	/* reset the link */

	if (netif_running(adapter->netdev)) {
		at_down(adapter);
		at_up(adapter);
	} else
		at_reset_hw(&adapter->hw);

	clear_bit(__AT_RESETTING, &adapter->flags);
	return 0;
}


static u32
at_get_tx_csum(struct net_device *netdev)
{
	return (netdev->features & NETIF_F_HW_CSUM) != 0;
}

static u32
at_get_msglevel(struct net_device *netdev)
{
#if defined(DBG)
	return 1;
#else
	return 0;
#endif
}	


static void
at_set_msglevel(struct net_device *netdev, u32 data)
{
}

static int
at_get_regs_len(struct net_device *netdev)
{
#define AT_REGS_LEN 42
	return AT_REGS_LEN * sizeof(u32);
}

static void
at_get_regs(struct net_device *netdev,
	       struct ethtool_regs *regs, void *p)
{
	struct at_adapter *adapter = netdev_priv(netdev);
	struct at_hw *hw = &adapter->hw;
	u32 *regs_buff = p;
	u16 phy_data;

	memset(p, 0, AT_REGS_LEN * sizeof(u32));

	regs->version = (1 << 24) | (hw->revision_id << 16) | hw->device_id;

	regs_buff[0]  = AT_READ_REG(hw, REG_VPD_CAP);
	regs_buff[1]  = AT_READ_REG(hw, REG_SPI_FLASH_CTRL);
	regs_buff[2]  = AT_READ_REG(hw, REG_SPI_FLASH_CONFIG);
	regs_buff[3]  = AT_READ_REG(hw, REG_TWSI_CTRL);
	regs_buff[4]  = AT_READ_REG(hw, REG_PCIE_DEV_MISC_CTRL);
	regs_buff[5]  = AT_READ_REG(hw, REG_MASTER_CTRL);
	regs_buff[6]  = AT_READ_REG(hw, REG_MANUAL_TIMER_INIT);
	regs_buff[7]  = AT_READ_REG(hw, REG_IRQ_MODU_TIMER_INIT);
	regs_buff[8]  = AT_READ_REG(hw, REG_PHY_ENABLE);
	regs_buff[9]  = AT_READ_REG(hw, REG_CMBDISDMA_TIMER);
	regs_buff[10] = AT_READ_REG(hw, REG_IDLE_STATUS);
	regs_buff[11] = AT_READ_REG(hw, REG_MDIO_CTRL);
	regs_buff[12] = AT_READ_REG(hw, REG_SERDES_LOCK);
	regs_buff[13] = AT_READ_REG(hw, REG_MAC_CTRL);
	regs_buff[14] = AT_READ_REG(hw, REG_MAC_IPG_IFG);
	regs_buff[15] = AT_READ_REG(hw, REG_MAC_STA_ADDR);
	regs_buff[16] = AT_READ_REG(hw, REG_MAC_STA_ADDR+4);
	regs_buff[17] = AT_READ_REG(hw, REG_RX_HASH_TABLE);
	regs_buff[18] = AT_READ_REG(hw, REG_RX_HASH_TABLE+4);
	regs_buff[19] = AT_READ_REG(hw, REG_MAC_HALF_DUPLX_CTRL);
	regs_buff[20] = AT_READ_REG(hw, REG_MTU);
	regs_buff[21] = AT_READ_REG(hw, REG_WOL_CTRL);
	regs_buff[22] = AT_READ_REG(hw, REG_SRAM_TXRAM_END);
	regs_buff[23] = AT_READ_REG(hw, REG_DESC_BASE_ADDR_HI);
	regs_buff[24] = AT_READ_REG(hw, REG_TXD_BASE_ADDR_LO);
	regs_buff[25] = AT_READ_REG(hw, REG_TXD_MEM_SIZE);	
	regs_buff[26] = AT_READ_REG(hw, REG_TXS_BASE_ADDR_LO);
	regs_buff[27] = AT_READ_REG(hw, REG_TXS_MEM_SIZE);
	regs_buff[28] = AT_READ_REG(hw, REG_RXD_BASE_ADDR_LO);
	regs_buff[29] = AT_READ_REG(hw, REG_RXD_BUF_NUM);
	regs_buff[30] = AT_READ_REG(hw, REG_DMAR);
	regs_buff[31] = AT_READ_REG(hw, REG_TX_CUT_THRESH);
	regs_buff[32] = AT_READ_REG(hw, REG_DMAW);
	regs_buff[33] = AT_READ_REG(hw, REG_PAUSE_ON_TH);
	regs_buff[34] = AT_READ_REG(hw, REG_PAUSE_OFF_TH);
	regs_buff[35] = AT_READ_REG(hw, REG_MB_TXD_WR_IDX);
	regs_buff[36] = AT_READ_REG(hw, REG_MB_RXD_RD_IDX);	
	regs_buff[38] = AT_READ_REG(hw, REG_ISR);	
	regs_buff[39] = AT_READ_REG(hw, REG_IMR);	
	
	at_read_phy_reg(hw, MII_BMCR, &phy_data);
	regs_buff[40] = (u32)phy_data;
	at_read_phy_reg(hw, MII_BMSR, &phy_data);
	regs_buff[41] = (u32)phy_data;
}

static int
at_get_eeprom_len(struct net_device *netdev)
{
	struct at_adapter *adapter = netdev_priv(netdev);
	
	if (!check_eeprom_exist(&adapter->hw)) {
		return 512;
	} else 
		return 0;
}

static int
at_get_eeprom(struct net_device *netdev,
                      struct ethtool_eeprom *eeprom, u8 *bytes)
{
	struct at_adapter *adapter = netdev_priv(netdev);
	struct at_hw *hw = &adapter->hw;
	u32 *eeprom_buff;
	int first_dword, last_dword;
	int ret_val = 0;
	int i;

	if (eeprom->len == 0)
		return -EINVAL;

	if (check_eeprom_exist(hw)) {
		return -EINVAL;
	}
		
	eeprom->magic = hw->vendor_id | (hw->device_id << 16);

	first_dword = eeprom->offset >> 2;
	last_dword = (eeprom->offset + eeprom->len - 1) >> 2;

	eeprom_buff = kmalloc(sizeof(u32) *
			(last_dword - first_dword + 1), GFP_KERNEL);
	if (!eeprom_buff)
		return -ENOMEM;

	for (i=first_dword; i < last_dword; i++) {
		if (!read_eeprom(hw, i*4, &(eeprom_buff[i-first_dword])))
			return -EIO;
	}

	memcpy(bytes, (u8 *)eeprom_buff + (eeprom->offset & 3),
			eeprom->len);
	kfree(eeprom_buff);

	return ret_val;
}

static int
at_set_eeprom(struct net_device *netdev,
                      struct ethtool_eeprom *eeprom, u8 *bytes)
{
	struct at_adapter *adapter = netdev_priv(netdev);
	struct at_hw *hw = &adapter->hw;
	u32 *eeprom_buff;
	u32 *ptr;
	int max_len, first_dword, last_dword, ret_val = 0;
	int i;

	if (eeprom->len == 0)
		return -EOPNOTSUPP;

	if (eeprom->magic != (hw->vendor_id | (hw->device_id << 16)))
		return -EFAULT;

	max_len = 512;

	first_dword = eeprom->offset >> 2;
	last_dword = (eeprom->offset + eeprom->len - 1) >> 2;
	eeprom_buff = kmalloc(max_len, GFP_KERNEL);
	if (!eeprom_buff)
		return -ENOMEM;

	ptr = (u32 *)eeprom_buff;

	if (eeprom->offset & 3) {
		/* need read/modify/write of first changed EEPROM word */
		/* only the second byte of the word is being modified */
		if (!read_eeprom(hw, first_dword*4, &(eeprom_buff[0])))
			return -EIO;
		ptr++;
	}
	if (((eeprom->offset + eeprom->len) & 3) ) {
		/* need read/modify/write of last changed EEPROM word */
		/* only the first byte of the word is being modified */
		
		if (!read_eeprom(hw, last_dword*4, &(eeprom_buff[last_dword - first_dword])))
			return -EIO;
	}

	/* Device's eeprom is always little-endian, word addressable */
	memcpy(ptr, bytes, eeprom->len);

	for (i = 0; i < last_dword - first_dword + 1; i++) {
		if (!write_eeprom(hw, ((first_dword+i)*4), eeprom_buff[i]))
			return -EIO;
	}

	kfree(eeprom_buff);
	return ret_val;
}

static void
at_get_drvinfo(struct net_device *netdev,
                       struct ethtool_drvinfo *drvinfo)
{
	struct at_adapter *adapter = netdev_priv(netdev);

	strncpy(drvinfo->driver,  at_driver_name, 32);
	strncpy(drvinfo->version, at_driver_version, 32);
	strncpy(drvinfo->fw_version, "L2", 32);
	strncpy(drvinfo->bus_info, pci_name(adapter->pdev), 32);
	drvinfo->n_stats = 0;
	drvinfo->testinfo_len = 0;
	drvinfo->regdump_len = at_get_regs_len(netdev);
	drvinfo->eedump_len = at_get_eeprom_len(netdev);
}

static void
at_get_wol(struct net_device *netdev, struct ethtool_wolinfo *wol)
{
	struct at_adapter *adapter = netdev_priv(netdev);

	wol->supported = WAKE_MAGIC;
	wol->wolopts = 0;

	if (adapter->wol & AT_WUFC_EX)
		wol->wolopts |= WAKE_UCAST;
	if (adapter->wol & AT_WUFC_MC)
		wol->wolopts |= WAKE_MCAST;
	if (adapter->wol & AT_WUFC_BC)
		wol->wolopts |= WAKE_BCAST;
	if (adapter->wol & AT_WUFC_MAG)
		wol->wolopts |= WAKE_MAGIC;

	return;
}

static int
at_set_wol(struct net_device *netdev, struct ethtool_wolinfo *wol)
{
	struct at_adapter *adapter = netdev_priv(netdev);

	if (wol->wolopts & (WAKE_PHY | WAKE_ARP | WAKE_MAGICSECURE))
		return -EOPNOTSUPP;

	if (wol->wolopts & (WAKE_MCAST|WAKE_BCAST|WAKE_MCAST)) {
		AT_DBG("Interface does not support broadcast/multicast frame wake-up packets\n");
		return -EOPNOTSUPP;
	}

	/* these settings will always override what we currently have */
	adapter->wol = 0;

	if (wol->wolopts & WAKE_MAGIC)
		adapter->wol |= AT_WUFC_MAG;

	return 0;
}

static int
at_nway_reset(struct net_device *netdev)
{
	struct at_adapter *adapter = netdev_priv(netdev);
	if (netif_running(netdev))
		at_reinit_locked(adapter);
	return 0;
}


static struct ethtool_ops at_ethtool_ops = {
	.get_settings           = at_get_settings,
	.set_settings           = at_set_settings,
	.get_drvinfo            = at_get_drvinfo,
	.get_regs_len           = at_get_regs_len,
	.get_regs               = at_get_regs,
	.get_wol                = at_get_wol,
	.set_wol                = at_set_wol,
	.get_msglevel           = at_get_msglevel,
	.set_msglevel           = at_set_msglevel,
	.nway_reset             = at_nway_reset,
	.get_link               = ethtool_op_get_link,
	.get_eeprom_len         = at_get_eeprom_len,
	.get_eeprom             = at_get_eeprom,
	.set_eeprom             = at_set_eeprom,
	.get_tx_csum            = at_get_tx_csum,
	.get_sg                 = ethtool_op_get_sg,
	.set_sg                 = ethtool_op_set_sg,
#ifdef NETIF_F_TSO
	.get_tso                = ethtool_op_get_tso,
#endif
};

void at_set_ethtool_ops(struct net_device *netdev)
{
	SET_ETHTOOL_OPS(netdev, &at_ethtool_ops);
}
#endif	/* SIOCETHTOOL */


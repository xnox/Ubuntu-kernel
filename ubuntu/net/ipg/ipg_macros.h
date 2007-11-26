/*
 *
 * ipg_macros.h
 *
 * Include file with macros for Gigabit Ethernet
 * device driver for Network Interface Cards (NICs) utilizing the
 * Tamarack Microelectronics Inc. IPG Gigabit or Triple Speed
 * Ethernet Media Access Controller.
 *
 * Craig Rich
 * Sundance Technology, Inc.
 * 1485 Saratoga Avenue
 * Suite 200
 * San Jose, CA 95129
 * 408 873 4117
 * www.sundanceti.com
 * craig_rich@sundanceti.com
 *
 * Rev  Date     Description
 * --------------------------------------------------------------
 * 0.1  3/30/01  New file created from original ipg.h
 */

/*
 * Miscellaneous macros.
 */

/* Marco for printing debug statements.
#  define IPG_DDEBUG_MSG(args...) printk(KERN_DEBUG "IPG: " ## args) */
#ifdef IPG_DEBUG
#  define IPG_DEBUG_MSG(args...) 
#  define IPG_DDEBUG_MSG(args...) printk(KERN_DEBUG "IPG: " args)
#  define IPG_DUMPRFDLIST(args) ipg_dump_rfdlist(args)
#  define IPG_DUMPTFDLIST(args) ipg_dump_tfdlist(args)
#else
#  define IPG_DEBUG_MSG(args...)
#  define IPG_DDEBUG_MSG(args...)
#  define IPG_DUMPRFDLIST(args)
#  define IPG_DUMPTFDLIST(args)
#endif

/*
 * End miscellaneous macros.
 */

/*
 * Register access macros.
 */

#ifdef CONFIG_NET_IPG_IO

/* Use I/O access for IPG registers. */

#define RD8 inb
#define RD16 inw
#define RD32 inl
#define WR8 outb
#define WR16 outw
#define WR32 outl

#else

/* Use memory access for IPG registers. */

#define RD8 readb
#define RD16 readw
#define RD32 readl
#define WR8 writeb
#define WR16 writew
#define WR32 writel

#endif

#define		IPG_READ_BYTEREG(regaddr)	RD8(regaddr)

#define		IPG_READ_WORDREG(regaddr) RD16(regaddr)

#define		IPG_READ_LONGREG(regaddr)	RD32(regaddr)

#define		IPG_WRITE_BYTEREG(regaddr, writevalue)	WR8(writevalue, regaddr)

#define		IPG_WRITE_WORDREG(regaddr, writevalue)	WR16(writevalue, regaddr)

#define		IPG_WRITE_LONGREG(regaddr, writevalue)	WR32(writevalue, regaddr)

#define		IPG_READ_ASICCTRL(baseaddr)	RD32(baseaddr + IPG_ASICCTRL)

//Jesse20040128EEPROM_VALUE
//#define		IPG_WRITE_ASICCTRL(baseaddr, writevalue)	WR32(IPG_AC_RSVD_MASK & (writevalue), baseaddr + IPG_ASICCTRL)
#define		IPG_WRITE_ASICCTRL(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ASICCTRL)

#define		IPG_READ_EEPROMCTRL(baseaddr)	RD16(baseaddr + IPG_EEPROMCTRL)

#define		IPG_WRITE_EEPROMCTRL(baseaddr, writevalue)	WR16(IPG_EC_RSVD_MASK & (writevalue), baseaddr + IPG_EEPROMCTRL)

#define		IPG_READ_EEPROMDATA(baseaddr)	RD16(baseaddr + IPG_EEPROMDATA)

#define		IPG_WRITE_EEPROMDATA(baseaddr, writevalue)	WR16(writevalue, (baseaddr + IPG_EEPROMDATA)

#define		IPG_READ_PHYSET(baseaddr)	RD8(baseaddr + IPG_PHYSET)//Jesse20040128EEPROM_VALUE

#define		IPG_WRITE_PHYSET(baseaddr, writevalue)	WR8(writevalue, baseaddr + IPG_PHYSET)//Jesse20040128EEPROM_VALUE

#define		IPG_READ_PHYCTRL(baseaddr)	RD8(baseaddr + IPG_PHYCTRL)

#define		IPG_WRITE_PHYCTRL(baseaddr, writevalue)	WR8(IPG_PC_RSVD_MASK & (writevalue), baseaddr + IPG_PHYCTRL)

#define		IPG_READ_RECEIVEMODE(baseaddr)	RD8(baseaddr + IPG_RECEIVEMODE)

#define		IPG_WRITE_RECEIVEMODE(baseaddr, writevalue)	WR8(IPG_RM_RSVD_MASK & (writevalue), baseaddr + IPG_RECEIVEMODE)

#define		IPG_READ_MAXFRAMESIZE(baseaddr)	RD16(baseaddr + IPG_MAXFRAMESIZE)

#define		IPG_WRITE_MAXFRAMESIZE(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_MAXFRAMESIZE)

#define		IPG_READ_MACCTRL(baseaddr)	RD32(baseaddr + IPG_MACCTRL)

#define		IPG_WRITE_MACCTRL(baseaddr, writevalue)	WR32(IPG_MC_RSVD_MASK & (writevalue), baseaddr + IPG_MACCTRL)

#define		IPG_READ_INTSTATUSACK(baseaddr)	RD16(baseaddr + IPG_INTSTATUSACK)

#define		IPG_READ_INTSTATUS(baseaddr)	RD16(baseaddr + IPG_INTSTATUS)

#define		IPG_WRITE_INTSTATUS(baseaddr, writevalue)	WR16(IPG_IS_RSVD_MASK & (writevalue), baseaddr + IPG_INTSTATUS)

#define		IPG_READ_INTENABLE(baseaddr)	RD16(baseaddr + IPG_INTENABLE)

#define		IPG_WRITE_INTENABLE(baseaddr, writevalue)	WR16(IPG_IE_RSVD_MASK & (writevalue), baseaddr + IPG_INTENABLE)

#define		IPG_READ_WAKEEVENT(baseaddr)	RD8(baseaddr + IPG_WAKEEVENT)

#define		IPG_WRITE_WAKEEVENT(baseaddr, writevalue)	WR8(writevalue, baseaddr + IPG_WAKEEVENT)

#define		IPG_READ_RXEARLYTHRESH(baseaddr)	RD16(baseaddr + IPG_RXEARLYTHRESH)

#define		IPG_WRITE_RXEARLYTHRESH(baseaddr, writevalue)	WR16(IPG_RE_RSVD_MASK & (writevalue), baseaddr + IPG_RXEARLYTHRESH)

#define		IPG_READ_TXSTARTTHRESH(baseaddr)	RD32(baseaddr + IPG_TXSTARTTHRESH)

#define		IPG_WRITE_TXSTARTTHRESH(baseaddr, writevalue)	WR32(IPG_TT_RSVD_MASK & (writevalue), baseaddr + IPG_TXSTARTTHRESH)

#define		IPG_READ_FIFOCTRL(baseaddr)	RD16(baseaddr + IPG_FIFOCTRL)

#define		IPG_WRITE_FIFOCTRL(baseaddr, writevalue)	WR16(IPG_FC_RSVD_MASK & (writevalue), baseaddr + IPG_FIFOCTRL)

#define		IPG_READ_RXDMAPOLLPERIOD(baseaddr)	RD8(baseaddr + IPG_RXDMAPOLLPERIOD)

#define		IPG_WRITE_RXDMAPOLLPERIOD(baseaddr, writevalue)	WR8(IPG_RP_RSVD_MASK & (writevalue), baseaddr + IPG_RXDMAPOLLPERIOD)

#define		IPG_READ_RXDMAURGENTTHRESH(baseaddr)	RD8(baseaddr + IPG_RXDMAURGENTTHRESH)

#define		IPG_WRITE_RXDMAURGENTTHRESH(baseaddr, writevalue)	WR8(IPG_RU_RSVD_MASK & (writevalue), baseaddr + IPG_RXDMAURGENTTHRESH)

#define		IPG_READ_RXDMABURSTTHRESH(baseaddr)	RD8(baseaddr + IPG_RXDMABURSTTHRESH)

#define		IPG_WRITE_RXDMABURSTTHRESH(baseaddr, writevalue)	WR8(writevalue, baseaddr + IPG_RXDMABURSTTHRESH)

#define		IPG_READ_RFDLISTPTR0(baseaddr)	RD32(baseaddr + IPG_RFDLISTPTR0)

#define		IPG_WRITE_RFDLISTPTR0(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_RFDLISTPTR0)

#define		IPG_READ_RFDLISTPTR1(baseaddr)	RD32(baseaddr + IPG_RFDLISTPTR1)

#define		IPG_WRITE_RFDLISTPTR1(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_RFDLISTPTR1)

#define		IPG_READ_TXDMAPOLLPERIOD(baseaddr)	RD8(baseaddr + IPG_TXDMAPOLLPERIOD)

#define		IPG_WRITE_TXDMAPOLLPERIOD(baseaddr, writevalue)	WR8(IPG_TP_RSVD_MASK & (writevalue), baseaddr + IPG_TXDMAPOLLPERIOD)

#define		IPG_READ_TXDMAURGENTTHRESH(baseaddr)	RD8(baseaddr + IPG_TXDMAURGENTTHRESH)

#define		IPG_WRITE_TXDMAURGENTTHRESH(baseaddr, writevalue)	WR8(IPG_TU_RSVD_MASK & (writevalue), baseaddr + IPG_TXDMAURGENTTHRESH)

#define		IPG_READ_TXDMABURSTTHRESH(baseaddr)	RD8(baseaddr + IPG_TXDMABURSTTHRESH)

#define		IPG_WRITE_TXDMABURSTTHRESH(baseaddr, writevalue)	WR8(IPG_TB_RSVD_MASK & (writevalue), baseaddr + IPG_TXDMABURSTTHRESH)

#define		IPG_READ_TFDLISTPTR0(baseaddr)	RD32(baseaddr + IPG_TFDLISTPTR0)

#define		IPG_WRITE_TFDLISTPTR0(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_TFDLISTPTR0)

#define		IPG_READ_TFDLISTPTR1(baseaddr)	RD32(baseaddr + IPG_TFDLISTPTR1)

#define		IPG_WRITE_TFDLISTPTR1(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_TFDLISTPTR1)

#define		IPG_READ_DMACTRL(baseaddr)	RD32(baseaddr + IPG_DMACTRL)

#define		IPG_WRITE_DMACTRL(baseaddr, writevalue)	WR32(IPG_DC_RSVD_MASK & (writevalue), baseaddr + IPG_DMACTRL)

#define		IPG_READ_TXSTATUS(baseaddr)	RD32(baseaddr + IPG_TXSTATUS)

#define		IPG_WRITE_TXSTATUS(baseaddr, writevalue)	WR32(IPG_TS_RSVD_MASK & (writevalue), baseaddr + IPG_TXSTATUS)

#define		IPG_READ_STATIONADDRESS0(baseaddr)	RD16(baseaddr + IPG_STATIONADDRESS0)

#define		IPG_READ_STATIONADDRESS1(baseaddr)	RD16(baseaddr + IPG_STATIONADDRESS1)

#define		IPG_READ_STATIONADDRESS2(baseaddr)	RD16(baseaddr + IPG_STATIONADDRESS2)

#define		IPG_WRITE_STATIONADDRESS0(baseaddr,writevalue)	WR16(writevalue,baseaddr + IPG_STATIONADDRESS0)//JES20040127EEPROM

#define		IPG_WRITE_STATIONADDRESS1(baseaddr,writevalue)	WR16(writevalue,baseaddr + IPG_STATIONADDRESS1)//JES20040127EEPROM

#define		IPG_WRITE_STATIONADDRESS2(baseaddr,writevalue)	WR16(writevalue,baseaddr + IPG_STATIONADDRESS2)//JES20040127EEPROM

#define		IPG_READ_COUNTDOWN(baseaddr)	RD32(baseaddr + IPG_COUNTDOWN)

#define		IPG_WRITE_COUNTDOWN(baseaddr, writevalue)	WR32(IPG_CD_RSVD_MASK & (writevalue), baseaddr + IPG_COUNTDOWN)

#define		IPG_READ_RXDMASTATUS(baseaddr)	RD16(baseaddr + IPG_RXDMASTATUS)

#define		IPG_WRITE_HASHTABLE0(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_HASHTABLE0)

#define		IPG_WRITE_HASHTABLE1(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_HASHTABLE1)

#define		IPG_READ_STATISTICSMASK(baseaddr)	RD32(baseaddr + IPG_STATISTICSMASK)

#define		IPG_WRITE_STATISTICSMASK(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_STATISTICSMASK)

#define		IPG_READ_RMONSTATISTICSMASK(baseaddr)	RD32(baseaddr + IPG_RMONSTATISTICSMASK)

#define		IPG_WRITE_RMONSTATISTICSMASK(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_RMONSTATISTICSMASK)

#define		IPG_READ_VLANTAG(baseaddr)	RD32(baseaddr + IPG_VLANTAG)

#define		IPG_WRITE_VLANTAG(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_VLANTAG)

#define		IPG_READ_FLOWONTHRESH(baseaddr)	RD16(baseaddr + IPG_FLOWONTHRESH)

#define		IPG_WRITE_FLOWONTHRESH(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_FLOWONTHRESH)

#define		IPG_READ_FLOWOFFTHRESH(baseaddr)	RD16(baseaddr + IPG_FLOWOFFTHRESH)

#define		IPG_WRITE_FLOWOFFTHRESH(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_FLOWOFFTHRESH)

#define		IPG_READ_DEBUGCTRL(baseaddr)	RD16(baseaddr + IPG_DEBUGCTRL)

#define		IPG_WRITE_DEBUGCTRL(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_DEBUGCTRL)

#define		IPG_READ_RXDMAINTCTRL(baseaddr)	RD32(baseaddr + IPG_RXDMAINTCTRL)

#define		IPG_WRITE_RXDMAINTCTRL(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_RXDMAINTCTRL)

#define		IPG_READ_TXJUMBOFRAMES(baseaddr)	RD16(baseaddr + IPG_TXJUMBOFRAMES)

#define		IPG_WRITE_TXJUMBOFRAMES(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_TXJUMBOFRAMES)

#define		IPG_READ_UDPCHECKSUMERRORS(baseaddr)	RD16(baseaddr + IPG_UDPCHECKSUMERRORS)

#define		IPG_WRITE_UDPCHECKSUMERRORS(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_UDPCHECKSUMERRORS)

#define		IPG_READ_IPCHECKSUMERRORS(baseaddr)	RD16(baseaddr + IPG_IPCHECKSUMERRORS)

#define		IPG_WRITE_IPCHECKSUMERRORS(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_IPCHECKSUMERRORS)

#define		IPG_READ_TCPCHECKSUMERRORS(baseaddr)	RD16(baseaddr + IPG_TCPCHECKSUMERRORS)

#define		IPG_WRITE_TCPCHECKSUMERRORS(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_TCPCHECKSUMERRORS)

#define		IPG_READ_RXJUMBOFRAMES(baseaddr)	RD16(baseaddr + IPG_RXJUMBOFRAMES)

#define		IPG_WRITE_RXJUMBOFRAMES(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_RXJUMBOFRAMES)



/* Statistic register read/write macros. */

#define		IPG_READ_OCTETRCVOK(baseaddr)	RD32(baseaddr + IPG_OCTETRCVOK)

#define		IPG_WRITE_OCTETRCVOK(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_OCTETRCVOK)

#define		IPG_READ_MCSTOCTETRCVDOK(baseaddr)	RD32(baseaddr + IPG_MCSTOCTETRCVDOK)

#define		IPG_WRITE_MCSTOCTETRCVDOK(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_MCSTOCTETRCVDOK)

#define		IPG_READ_BCSTOCTETRCVOK(baseaddr)	RD32(baseaddr + IPG_BCSTOCTETRCVOK)

#define		IPG_WRITE_BCSTOCTETRCVOK(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_BCSTOCTETRCVOK)

#define		IPG_READ_FRAMESRCVDOK(baseaddr)	RD32(baseaddr + IPG_FRAMESRCVDOK)

#define		IPG_WRITE_FRAMESRCVDOK(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_FRAMESRCVDOK)

#define		IPG_READ_MCSTFRAMESRCVDOK(baseaddr)	RD32(baseaddr + IPG_MCSTFRAMESRCVDOK)

#define		IPG_WRITE_MCSTFRAMESRCVDOK(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_MCSTFRAMESRCVDOK)

#define		IPG_READ_BCSTFRAMESRCVDOK(baseaddr)	RD16(baseaddr + IPG_BCSTFRAMESRCVDOK)

#define		IPG_WRITE_BCSTFRAMESRCVDOK(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_BCSTFRAMESRCVDOK)

#define		IPG_READ_MACCONTROLFRAMESRCVD(baseaddr)	RD16(baseaddr + IPG_MACCONTROLFRAMESRCVD)

#define		IPG_WRITE_MACCONTROLFRAMESRCVD(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_MACCONTROLFRAMESRCVD)

#define		IPG_READ_FRAMETOOLONGERRRORS(baseaddr)	RD16(baseaddr + IPG_FRAMETOOLONGERRRORS)

#define		IPG_WRITE_FRAMETOOLONGERRRORS(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_FRAMETOOLONGERRRORS)

#define		IPG_READ_INRANGELENGTHERRORS(baseaddr)	RD16(baseaddr + IPG_INRANGELENGTHERRORS)

#define		IPG_WRITE_INRANGELENGTHERRORS(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_INRANGELENGTHERRORS)

#define		IPG_READ_FRAMECHECKSEQERRORS(baseaddr)	RD16(baseaddr + IPG_FRAMECHECKSEQERRORS)

#define		IPG_WRITE_FRAMECHECKSEQERRORS(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_FRAMECHECKSEQERRORS)

#define		IPG_READ_FRAMESLOSTRXERRORS(baseaddr)	RD16(baseaddr + IPG_FRAMESLOSTRXERRORS)

#define		IPG_WRITE_FRAMESLOSTRXERRORS(baseaddr, writevalue) WR16(writevalue, baseaddr + IPG_FRAMESLOSTRXERRORS)

#define		IPG_READ_FRAMESLOSTRXERRORS(baseaddr)	RD16(baseaddr + IPG_FRAMESLOSTRXERRORS)

#define		IPG_WRITE_FRAMESLOSTRXERRORS(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_FRAMESLOSTRXERRORS)

#define		IPG_READ_OCTETXMTOK(baseaddr)	RD32(baseaddr + IPG_OCTETXMTOK)

#define		IPG_WRITE_OCTETXMTOK(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_OCTETXMTOK)

#define		IPG_READ_MCSTOCTETXMTOK(baseaddr)	RD32(baseaddr + IPG_MCSTOCTETXMTOK)

#define		IPG_WRITE_MCSTOCTETXMTOK(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_MCSTOCTETXMTOK)

#define		IPG_READ_BCSTOCTETXMTOK(baseaddr)	RD32(baseaddr + IPG_BCSTOCTETXMTOK)

#define		IPG_WRITE_BCSTOCTETXMTOK(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_BCSTOCTETXMTOK)

#define		IPG_READ_FRAMESXMTDOK(baseaddr)	RD32(baseaddr + IPG_FRAMESXMTDOK)

#define		IPG_WRITE_FRAMESXMTDOK(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_FRAMESXMTDOK)

#define		IPG_READ_MCSTFRAMESXMTDOK(baseaddr)	RD32(baseaddr + IPG_MCSTFRAMESXMTDOK)

#define		IPG_WRITE_MCSTFRAMESXMTDOK(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_MCSTFRAMESXMTDOK)

#define		IPG_READ_FRAMESWDEFERREDXMT(baseaddr)	RD32(baseaddr + IPG_FRAMESWDEFERREDXMT)

#define		IPG_WRITE_FRAMESWDEFERREDXMT(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_FRAMESWDEFERREDXMT)

#define		IPG_READ_LATECOLLISIONS(baseaddr)	RD32(baseaddr + IPG_LATECOLLISIONS)

#define		IPG_WRITE_LATECOLLISIONS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_LATECOLLISIONS)

#define		IPG_READ_MULTICOLFRAMES(baseaddr)	RD32(baseaddr + IPG_MULTICOLFRAMES)

#define		IPG_WRITE_MULTICOLFRAMES(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_MULTICOLFRAMES)

#define		IPG_READ_SINGLECOLFRAMES(baseaddr)	RD32(baseaddr + IPG_SINGLECOLFRAMES)

#define		IPG_WRITE_SINGLECOLFRAMES(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_SINGLECOLFRAMES)

#define		IPG_READ_BCSTFRAMESXMTDOK(baseaddr)	RD16(baseaddr + IPG_BCSTFRAMESXMTDOK)

#define		IPG_WRITE_BCSTFRAMESXMTDOK(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_BCSTFRAMESXMTDOK)

#define		IPG_READ_CARRIERSENSEERRORS(baseaddr)	RD16(baseaddr + IPG_CARRIERSENSEERRORS)

#define		IPG_WRITE_CARRIERSENSEERRORS(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_CARRIERSENSEERRORS)

#define		IPG_READ_MACCONTROLFRAMESXMTDOK(baseaddr)	RD16(baseaddr + IPG_MACCONTROLFRAMESXMTDOK)

#define		IPG_WRITE_MACCONTROLFRAMESXMTDOK(baseaddr, writevalue) WR16(writevalue, baseaddr + IPG_MACCONTROLFRAMESXMTDOK)

#define		IPG_READ_FRAMESABORTXSCOLLS(baseaddr)	RD16(baseaddr + IPG_FRAMESABORTXSCOLLS)

#define		IPG_WRITE_FRAMESABORTXSCOLLS(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_FRAMESABORTXSCOLLS)

#define		IPG_READ_FRAMESWEXDEFERRAL(baseaddr)	RD16(baseaddr + IPG_FRAMESWEXDEFERRAL)

#define		IPG_WRITE_FRAMESWEXDEFERRAL(baseaddr, writevalue)	WR16(writevalue, baseaddr + IPG_FRAMESWEXDEFERRAL)

/* RMON statistic register read/write macros. */

#define		IPG_READ_ETHERSTATSCOLLISIONS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSCOLLISIONS)

#define		IPG_WRITE_ETHERSTATSCOLLISIONS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSCOLLISIONS)

#define		IPG_READ_ETHERSTATSOCTETSTRANSMIT(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSOCTETSTRANSMIT)

#define		IPG_WRITE_ETHERSTATSOCTETSTRANSMIT(baseaddr, writevalue) WR32(writevalue, baseaddr + IPG_ETHERSTATSOCTETSTRANSMIT)

#define		IPG_READ_ETHERSTATSPKTSTRANSMIT(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSPKTSTRANSMIT)

#define		IPG_WRITE_ETHERSTATSPKTSTRANSMIT(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTSTRANSMIT)

#define		IPG_READ_ETHERSTATSPKTS64OCTESTSTRANSMIT(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSPKTS64OCTESTSTRANSMIT)

#define		IPG_WRITE_ETHERSTATSPKTS64OCTESTSTRANSMIT(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS64OCTESTSTRANSMIT)

#define		IPG_READ_ETHERSTATSPKTS65TO127OCTESTSTRANSMIT(baseaddr) RD32(baseaddr + IPG_ETHERSTATSPKTS65TO127OCTESTSTRANSMIT)

#define		IPG_WRITE_ETHERSTATSPKTS65TO127OCTESTSTRANSMIT(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS65TO127OCTESTSTRANSMIT)

#define		IPG_READ_ETHERSTATSPKTS128TO255OCTESTSTRANSMIT(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSPKTS128TO255OCTESTSTRANSMIT)

#define		IPG_WRITE_ETHERSTATSPKTS128TO255OCTESTSTRANSMIT(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS128TO255OCTESTSTRANSMIT)

#define		IPG_READ_ETHERSTATSPKTS256TO511OCTESTSTRANSMIT(baseaddr) RD32(baseaddr + IPG_ETHERSTATSPKTS256TO511OCTESTSTRANSMIT)

#define		IPG_WRITE_ETHERSTATSPKTS256TO511OCTESTSTRANSMIT(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS256TO511OCTESTSTRANSMIT)

#define		IPG_READ_ETHERSTATSPKTS512TO1023OCTESTSTRANSMIT(baseaddr) RD32(baseaddr + IPG_ETHERSTATSPKTS512TO1023OCTESTSTRANSMIT)

#define		IPG_WRITE_ETHERSTATSPKTS512TO1023OCTESTSTRANSMIT(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS512TO1023OCTESTSTRANSMIT)

#define		IPG_READ_ETHERSTATSPKTS1024TO1518OCTESTSTRANSMIT(baseaddr) RD32(baseaddr + IPG_ETHERSTATSPKTS1024TO1518OCTESTSTRANSMIT)

#define		IPG_WRITE_ETHERSTATSPKTS1024TO1518OCTESTSTRANSMIT(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS1024TO1518OCTESTSTRANSMIT)

#define		IPG_READ_ETHERSTATSCRCALIGNERRORS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSCRCALIGNERRORS)

#define		IPG_WRITE_ETHERSTATSCRCALIGNERRORS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSCRCALIGNERRORS)

#define		IPG_READ_ETHERSTATSUNDERSIZEPKTS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSUNDERSIZEPKTS)

#define		IPG_WRITE_ETHERSTATSUNDERSIZEPKTS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSUNDERSIZEPKTS)

#define		IPG_READ_ETHERSTATSFRAGMENTS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSFRAGMENTS)

#define		IPG_WRITE_ETHERSTATSFRAGMENTS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSFRAGMENTS)

#define		IPG_READ_ETHERSTATSJABBERS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSJABBERS)

#define		IPG_WRITE_ETHERSTATSJABBERS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSJABBERS)

#define		IPG_READ_ETHERSTATSOCTETS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSOCTETS)

#define		IPG_WRITE_ETHERSTATSOCTETS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSOCTETS)

#define		IPG_READ_ETHERSTATSPKTS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSPKTS)

#define		IPG_WRITE_ETHERSTATSPKTS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS)

#define		IPG_READ_ETHERSTATSPKTS64OCTESTS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSPKTS64OCTESTS)

#define		IPG_WRITE_ETHERSTATSPKTS64OCTESTS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS64OCTESTS)

#define		IPG_READ_ETHERSTATSPKTS65TO127OCTESTS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSPKTS65TO127OCTESTS)

#define		IPG_WRITE_ETHERSTATSPKTS65TO127OCTESTS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS65TO127OCTESTS)

#define		IPG_READ_ETHERSTATSPKTS128TO255OCTESTS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSPKTS128TO255OCTESTS)

#define		IPG_WRITE_ETHERSTATSPKTS128TO255OCTESTS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS128TO255OCTESTS)

#define		IPG_READ_ETHERSTATSPKTS256TO511OCTESTS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSPKTS256TO511OCTESTS)

#define		IPG_WRITE_ETHERSTATSPKTS256TO511OCTESTS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS256TO511OCTESTS)

#define		IPG_READ_ETHERSTATSPKTS512TO1023OCTESTS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSPKTS512TO1023OCTESTS)

#define		IPG_WRITE_ETHERSTATSPKTS512TO1023OCTESTS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS512TO1023OCTESTS)

#define		IPG_READ_ETHERSTATSPKTS1024TO1518OCTESTS(baseaddr)	RD32(baseaddr + IPG_ETHERSTATSPKTS1024TO1518OCTESTS)

#define		IPG_WRITE_ETHERSTATSPKTS1024TO1518OCTESTS(baseaddr, writevalue)	WR32(writevalue, baseaddr + IPG_ETHERSTATSPKTS1024TO1518OCTESTS)

/*
 * End register access macros.
 */

/* end ipg_macros.h */

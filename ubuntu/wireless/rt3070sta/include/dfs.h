
/* Plz read readme file for Software License information */

#define RADAR_PULSE 1
#define RADAR_WIDTH 2

#define WIDTH_RD_IDLE 0
#define WIDTH_RD_CHECK 1



/*************************************************************************
  *
  *	DFS Radar related definitions.
  *
  ************************************************************************/  
//#define CARRIER_DETECT_TASK_NUM	6
//#define RADAR_DETECT_TASK_NUM	7

// McuRadarState && McuCarrierState for 2880-SW-MCU
#define FREE_FOR_TX				0
#define WAIT_CTS_BEING_SENT		1
#define DO_DETECTION			2

// McuRadarEvent
#define RADAR_EVENT_CTS_SENT			0x01 // Host signal MCU that CTS has been sent
#define RADAR_EVENT_CTS_CARRIER_SENT	0x02 // Host signal MCU that CTS has been sent (Carrier)
#define RADAR_EVENT_RADAR_DETECTING		0x04 // Radar detection is on going, carrier detection hold back
#define RADAR_EVENT_CARRIER_DETECTING	0x08 // Carrier detection is on going, radar detection hold back
#define RADAR_EVENT_WIDTH_RADAR			0x10 // BBP == 2 radar detected
#define RADAR_EVENT_CTS_KICKED			0x20 // Radar detection need to sent double CTS, first CTS sent

// McuRadarCmd
#define DETECTION_STOP			0
#define RADAR_DETECTION			1
#define CARRIER_DETECTION		2



#ifdef TONE_RADAR_DETECT_SUPPORT
INT Set_CarrierCriteria_Proc(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
int Set_CarrierReCheck_Proc(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
INT Set_CarrierStopCheck_Proc(IN PRTMP_ADAPTER pAd, IN PSTRING arg);
void NewCarrierDetectionStart(PRTMP_ADAPTER pAd);
void RTMPHandleRadarInterrupt(PRTMP_ADAPTER  pAd);
VOID CSAsicDisableSync(IN PRTMP_ADAPTER pAd);
#endif // TONE_RADAR_DETECT_SUPPORT //


VOID BbpRadarDetectionStart(
	IN PRTMP_ADAPTER pAd);

VOID BbpRadarDetectionStop(
	IN PRTMP_ADAPTER pAd);

VOID RadarDetectionStart(
	IN PRTMP_ADAPTER pAd,
	IN BOOLEAN CTS_Protect,
	IN UINT8 CTSPeriod);

VOID RadarDetectionStop(
	IN PRTMP_ADAPTER	pAd);

VOID RadarDetectPeriodic(
	IN PRTMP_ADAPTER	pAd);
	

BOOLEAN RadarChannelCheck(
	IN PRTMP_ADAPTER	pAd,
	IN UCHAR			Ch);

ULONG JapRadarType(
	IN PRTMP_ADAPTER pAd);

ULONG RTMPBbpReadRadarDuration(
	IN PRTMP_ADAPTER	pAd);

ULONG RTMPReadRadarDuration(
	IN PRTMP_ADAPTER	pAd);

VOID RTMPCleanRadarDuration(
	IN PRTMP_ADAPTER	pAd);

VOID RTMPPrepareRDCTSFrame(
	IN	PRTMP_ADAPTER	pAd,
	IN	PUCHAR			pDA,
	IN	ULONG			Duration,
	IN  UCHAR           RTSRate,
	IN  ULONG           CTSBaseAddr,
	IN  UCHAR			FrameGap);

VOID RTMPPrepareRadarDetectParams(
	IN PRTMP_ADAPTER	pAd);


INT Set_ChMovingTime_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN PSTRING arg);

INT Set_LongPulseRadarTh_Proc(
	IN PRTMP_ADAPTER pAd, 
	IN PSTRING arg);



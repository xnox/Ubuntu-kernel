/* Plz read readme file for Software License information */

#ifndef	__ACTION_H__
#define	__ACTION_H__

typedef struct PACKED __HT_INFO_OCTET
{
#ifdef RT_BIG_ENDIAN
	UCHAR	Reserved:5;
	UCHAR 	STA_Channel_Width:1;
	UCHAR	Forty_MHz_Intolerant:1;
	UCHAR	Request:1;
#else
	UCHAR	Request:1;
	UCHAR	Forty_MHz_Intolerant:1;
	UCHAR 	STA_Channel_Width:1;
	UCHAR	Reserved:5;
#endif
} HT_INFORMATION_OCTET;


typedef struct PACKED __FRAME_HT_INFO
{
	HEADER_802_11   		Hdr;
	UCHAR					Category;
	UCHAR					Action;
	HT_INFORMATION_OCTET	HT_Info;
}   FRAME_HT_INFO, *PFRAME_HT_INFO;

#endif /* __ACTION_H__ */



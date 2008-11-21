#ifndef __HSO_H__
#define __HSO_H__

/*****************************************************************************/
/* hso.h                                                                     */
/* declarations of all functions, structs and enumerators, defines as well   */
/*****************************************************************************/

/*****************************************************************************/
/* Includes                                                                  */
/*****************************************************************************/
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/module.h>
#include <linux/ethtool.h>
#include <asm/uaccess.h>
#include <linux/usb.h>
#include <linux/timer.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/kmod.h>
#include <asm/byteorder.h>
#include <linux/version.h>
#include <net/arp.h>
#include <linux/ip.h>
#include <linux/proc_fs.h>

/*****************************************************************************/
/* Defines                                                                   */
/*****************************************************************************/
#define DRIVER_VERSION			"1.2"
#define MOD_AUTHOR			"Option Wireless"
#define MOD_DESCRIPTION			"USB High Speed Option driver"
#define MOD_LICENSE			"GPL"


#define HSO_MAX_NET_DEVICES		10
#define HSO__MAX_MTU			2048
#define DEFAULT_MTU			1500
#define DEFAULT_MRU			1500

#define CTRL_URB_RX_SIZE		1024
#define CTRL_URB_TX_SIZE		64

#define BULK_URB_RX_SIZE		4096
#define BULK_URB_TX_SIZE		8192

#define MUX_INTR_BUF_SIZE		16
#define MUX_BULK_RX_BUF_SIZE		HSO__MAX_MTU
#define MUX_BULK_TX_BUF_SIZE		HSO__MAX_MTU
#define MUX_BULK_RX_BUF_COUNT		4
#define USB_TYPE_OPTION_VENDOR		0x20

/* These definitions are used with the struct hso_net flags element */
/* - use *_bit operations on it. (bit indices not values.) */
#define HSO_NET_RUNNING			0
#define HSO_NET_TX_BUSY			1
#define HSO_TERMINATE			2

#define	HSO_NET_TX_TIMEOUT		(HZ*10)

#define SEND_ENCAPSULATED_COMMAND	0x00
#define GET_ENCAPSULATED_RESPONSE	0x01

/* Serial port defines and structs. */
#define HSO_THRESHOLD_THROTTLE		(7*1024)
#define HSO_THRESHOLD_UNTHROTTLE	(2*1024)

/* These definitions used in the Ethernet Packet Filtering requests */
/* See CDC Spec Table 62 */
#define MODE_FLAG_PROMISCUOUS		(1<<0)
#define MODE_FLAG_ALL_MULTICAST		(1<<1)
#define MODE_FLAG_DIRECTED		(1<<2)
#define MODE_FLAG_BROADCAST		(1<<3)
#define MODE_FLAG_MULTICAST		(1<<4)

/* CDC Spec class requests - CDC Spec Table 46 */
#define SET_ETHERNET_MULTICAST_FILTER	0x40
#define SET_ETHERNET_PACKET_FILTER	0x43

#define HSO_SERIAL_FLAG_THROTTLED	0
#define HSO_SERIAL_FLAG_TX_SENT		1
#define HSO_SERIAL_FLAG_RX_SENT		2
#define HSO_SERIAL_USB_GONE		3

#define HSO_SERIAL_MAGIC		0x48534f31

/* Number of ttys to handle */
#define HSO_SERIAL_TTY_MINORS		256

#define MAX_RX_URBS			2

#if defined(CONFIG_USB_SUSPEND) && ( LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21))
#define USE_SUSPEND
#endif

/* kernel dependent declarations */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20) )

#define TERMIOS				termios
#define GFP_T				int
#define CALLBACK_ARGS			struct urb *urb, struct pt_regs *regs

#else	/* ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20) ) */

#define TERMIOS				ktermios
#define GFP_T				gfp_t
#define CALLBACK_ARGS			struct urb *urb

#endif	/* ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20) ) */

#define get_serial_by_tty( x ) ( x ? (struct hso_serial *)x->driver_data : NULL )

/*****************************************************************************/
/* Debugging functions                                                       */
/*****************************************************************************/
#define D__(lvl_, fmt, arg...) do { printk(lvl_ "[%d:%s]: " fmt "\n", __LINE__, __FUNCTION__, ## arg); } while(0)
#define D_(lvl, args...)  do { if(lvl & debug) NFO( args ); } while(0)
#define QFO(fmt, args...) do { printk( KERN_INFO "hso: " fmt "\n", ##args); } while(0)

#define NFO(args...)			D__( KERN_INFO, ##args)
#define ERR(args...)			D__( KERN_ERR,  ##args)
#define WARN(args...)			D__( KERN_WARNING,  ##args)

#define D1(args...)			D_(0x01, ##args)
#define D2(args...)			D_(0x02, ##args)
#define D3(args...)			D_(0x04, ##args)
#define D4(args...)			D_(0x08, ##args)
#define D5(args...)			D_(0x10, ##args)
#define D				D1

/*****************************************************************************/
/* Enumerators                                                               */
/*****************************************************************************/
enum pkt_parse_state {
	WAIT_IP,
	WAIT_DATA,
	WAIT_SYNC
};

/*****************************************************************************/
/* Structs                                                                   */
/*****************************************************************************/

struct hso_shared_int {
	struct usb_endpoint_descriptor *intr_endp;
	void				*shared_intr_buf;
	struct urb			*shared_intr_urb;
	struct usb_device 		*usb;
	int 				use_count;
	int 				ref_count;
	spinlock_t shared_int_lock;
};

struct hso_net {
	struct hso_device 		*parent;
	struct net_device_stats		stats;
	struct net_device		*net;

	struct usb_endpoint_descriptor	*in_endp;
	struct usb_endpoint_descriptor	*out_endp;

	struct urb			*mux_bulk_rx_urb_pool[MUX_BULK_RX_BUF_COUNT];
	struct urb			*mux_bulk_tx_urb;
	void				*mux_bulk_rx_buf_pool[MUX_BULK_RX_BUF_COUNT];
	void				*mux_bulk_tx_buf;

	struct sk_buff			*skb_rx_buf;
	struct sk_buff			*skb_tx_buf;

	enum pkt_parse_state		rx_parse_state;
	spinlock_t			net_lock;


	unsigned short			rx_buf_size;
	unsigned short			rx_buf_missing;
	struct iphdr			rx_ip_hdr;
	struct ethhdr			dummy_eth_head;

	__u16				bcdCDC;
	__u16				wMaxSegmentSize;
	__u16				wNumberMCFilters;
	__u16				mode_flags;
	unsigned long			flags;

};

struct hso_serial {
	struct hso_device		*parent;
	int				magic;
	u8				minor;

	struct hso_shared_int 		*shared_int;

	/* rx/tx urb could be either a bulk urb or a control urb depending
	   on which serial port it is used on. */
	struct urb			*rx_urb[MAX_RX_URBS];
	u8				num_rx_urbs;
	u8				*rx_data[MAX_RX_URBS];
	u16				rx_data_length;	/* should contain allocated length */

	struct urb			*tx_urb;
	u8				*tx_data;
	u8				*tx_buffer;
	u16				tx_data_length;	/* should contain allocated length */
	u16				tx_data_count;
	u16				tx_buffer_count;
	spinlock_t			buffer_lock;
	struct usb_ctrlrequest		ctrl_req_tx;
	struct usb_ctrlrequest		ctrl_req_rx;
	
	struct usb_endpoint_descriptor 	*in_endp;
	struct usb_endpoint_descriptor	*out_endp;

	unsigned long			flags;
	u8				rts_state;
	u8				dtr_state;

	/* from usb_serial_port */
	struct tty_struct		*tty;
	int				open_count;
	spinlock_t			serial_lock;
	void				*private;

	int (*write_data) (struct hso_serial *serial);
};

struct hso_device {
	union {
		struct hso_serial	*dev_serial;
		struct hso_net		*dev_net;	
	} port_data;

	u32				port_spec;

#ifdef USE_SUSPEND
	u8				is_active;
	u8				suspend_disabled;
	struct work_struct		async_get_intf;
	struct work_struct		async_put_intf;
#endif
	struct usb_device		*usb;
	struct usb_interface		*interface;

#if ( LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19) )
	struct device				*dev;
#endif
	struct kref			ref;

	//TODO: Not sure about the ones below
	struct proc_dir_entry		*ourproc;
};


// Type of interface
#define HSO_INTF_MASK		0xFF00
#define	HSO_INTF_MUX		0x0100
#define	HSO_INTF_BULK   	0x0200

// Type of port
#define HSO_PORT_MASK		0xFF
#define HSO_PORT_NO_PORT	0x0
#define	HSO_PORT_CONTROL	0x1
#define	HSO_PORT_APP		0x2
#define	HSO_PORT_GPS		0x3
#define	HSO_PORT_PCSC		0x4
#define	HSO_PORT_APP2		0x5
#define HSO_PORT_GPS_CONTROL	0x6
#define HSO_PORT_MSD		0x7
#define HSO_PORT_VOICE		0x8
#define HSO_PORT_DIAG2		0x9
#define	HSO_PORT_DIAG		0x10
#define	HSO_PORT_MODEM		0x11
#define	HSO_PORT_NETWORK	0x12

// Additional device info
#define HSO_INFO_MASK		0xFF000000
#define HSO_INFO_CRC_BUG	0x01000000

/*****************************************************************************/
/* Prototypes                                                                */
/*****************************************************************************/
// Network interface functions
static int hso_net_open(struct net_device *net);
static int hso_net_close(struct net_device *net);
static int hso_net_start_xmit(struct sk_buff *skb, struct net_device *net);
static int hso_net_ioctl(struct net_device *net, struct ifreq *rq, int cmd);
static struct net_device_stats *hso_net_get_stats(struct net_device *net);
static void hso_net_tx_timeout(struct net_device *net);
static void hso_net_set_multicast(struct net_device *net);
static void read_bulk_callback(CALLBACK_ARGS);	/* struct urb *urb */
static void packetizeRx(struct hso_net *odev, unsigned char *ip_pkt, unsigned int count, unsigned char is_eop);
static void write_bulk_callback(CALLBACK_ARGS);	/* struct urb *urb */
// Serial driver functions
static int hso_serial_open(struct tty_struct *tty, struct file *filp);
static void hso_serial_close(struct tty_struct *tty, struct file *filp);
static int hso_serial_write(struct tty_struct *tty, const unsigned char *buf, int count);
static int hso_serial_write_room(struct tty_struct *tty);
static void hso_serial_set_termios(struct tty_struct *tty, struct TERMIOS *old);
static int hso_serial_chars_in_buffer(struct tty_struct *tty);
static int hso_serial_tiocmget(struct tty_struct *tty, struct file *file);
static int hso_serial_tiocmset(struct tty_struct *tty, struct file *file, unsigned int set, unsigned int clear);
static int hso_serial_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg);
static void hso_serial_throttle(struct tty_struct *tty);
static void hso_serial_unthrottle(struct tty_struct *tty);
static void hso_serial_break(struct tty_struct *tty, int break_state);
static int hso_serial_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data);
static void hso_serial_hangup(struct tty_struct *tty);
static void intr_callback(CALLBACK_ARGS);	/* struct urb *urb */
static int hso_mux_serial_read(struct hso_serial *serial);
static int hso_mux_serial_write_data(struct hso_serial *serial);
static int hso_std_serial_write_data(struct hso_serial *serial);
static int mux_device_request(struct hso_serial *serial, u8 type, u16 port, struct urb *ctrl_urb, struct usb_ctrlrequest *ctrl_req, u8 *ctrl_urb_data, u32 size);
static void ctrl_callback(CALLBACK_ARGS);	/* struct urb *urb */
static void put_rxbuf_data(struct urb *urb, struct hso_serial *serial);
static void hso_std_serial_read_bulk_callback(CALLBACK_ARGS);	/* struct urb *urb */
static void hso_std_serial_write_bulk_callback(CALLBACK_ARGS);	/* struct urb *urb */
static void _hso_serial_set_termios(struct tty_struct *tty, struct TERMIOS *old);
static void hso_kick_transmit( struct hso_serial *serial );
// Base driver functions
static int hso_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void hso_disconnect(struct usb_interface *interface);
// Helper functions
static int hso_mux_submit_intr_urb(struct hso_shared_int *mux_int,struct usb_device * usb, GFP_T gfp);
static void hso_net_init(struct net_device *net);
static void set_ethernet_addr(struct hso_net *odev);
static struct hso_serial *get_serial_by_index(unsigned index);
static struct hso_serial *get_serial_by_shared_int_and_type( struct hso_shared_int *shared_int, int mux );
static int get_free_serial_index(void);
static void set_serial_by_index(unsigned index,struct hso_serial *serial);
static int remove_net_device( struct hso_device *hso_dev );
static int add_net_device( struct hso_device *hso_dev );
static void log_usb_status(int status, const char* function);
inline struct usb_endpoint_descriptor *hso_get_ep( struct usb_interface *intf, int type, int dir );
int hso_get_mux_ports( struct usb_interface *intf , unsigned char *ports);
void hso_free_interface( struct usb_interface *intf );
int hso_start_serial_device( struct hso_device *hso_dev);
int hso_stop_serial_device( struct hso_device *hso_dev);
int hso_start_net_device( struct hso_device *hso_dev );
void hso_free_shared_int( struct hso_shared_int *shared_int );
int hso_stop_net_device( struct hso_device *hso_dev );
void hso_serial_ref_free( struct kref *ref );
#ifdef USE_SUSPEND
static int hso_suspend( struct usb_interface *iface, pm_message_t message );
static int hso_resume( struct usb_interface *iface );
#endif
void async_get_intf( struct work_struct *data );
void async_put_intf( struct work_struct *data );
int hso_put_activity( struct hso_device *hso_dev );
int hso_get_activity( struct hso_device *hso_dev );

/*****************************************************************************/
/* Helping functions                                                         */
/*****************************************************************************/

/* convert a character representing a hex value to a number */
static inline unsigned char hex2dec(unsigned char digit)
{

	if ((digit >= '0') && (digit <= '9')) {
		return (digit - '0');
	}
	/* Map all characters to 0-15 */
	if ((digit >= 'a') && (digit <= 'z')) {
		return (digit - 'a' + 10) % 16;
	}
	if ((digit >= 'A') && (digit <= 'Z')) {
		return (digit - 'A' + 10) % 16;
	}

	return 16;
}

/* get a string from usb device */
static inline void get_string(u8 * buf, int buf_len, int string_num, struct hso_net *odev)
{
	int len;

	if (!buf) {
		ERR("No buffer?");
		return;
	}

	buf[0] = 0x00;

	if (string_num) {
		/* Put it into its buffer */
		len = usb_string(odev->parent->usb, string_num, buf, buf_len);
		/* Just to be safe */
		buf[len] = 0x00;
	}
}

/*****************************************************************************/
/* Backwards compatibility                                                   */
/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20) )
/* Following functions are copied straight from the 2.6.20 kernel to maintain compatability */
static inline int usb_endpoint_xfer_bulk(const struct usb_endpoint_descriptor *epd)
{ return ((epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK); }
static inline int usb_endpoint_dir_in(const struct usb_endpoint_descriptor *epd)
{ return ((epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN); }
static inline int usb_endpoint_dir_out(const struct usb_endpoint_descriptor *epd)
{ return ((epd->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT); }
static inline int usb_endpoint_is_bulk_in(const struct usb_endpoint_descriptor *epd)
{ return (usb_endpoint_xfer_bulk(epd) && usb_endpoint_dir_in(epd)); }
static inline int usb_endpoint_is_bulk_out(const struct usb_endpoint_descriptor *epd)
{ return (usb_endpoint_xfer_bulk(epd) && usb_endpoint_dir_out(epd)); }
#endif	/* ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20) ) */

#endif	/* __HSO_H__ */

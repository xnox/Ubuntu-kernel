/* 
 * libdefs.h
 * Copyright (C) 2007, AVM GmbH. All rights reserved.
 * 
 * This Software is  free software. You can redistribute and/or
 * modify such free software under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * The free software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this Software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA, or see
 * http://www.opensource.org/licenses/lgpl-license.html
 * 
 * Contact: AVM GmbH, Alt-Moabit 95, 10559 Berlin, Germany, email: info@avm.de
 */

#ifndef __have_libdefs_h__
#define __have_libdefs_h__

//#include <linux/time.h>
#include <stdarg.h>
#include "defs.h"
#include "attr.h"
#include "common.h"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct __lib {

	/*Talk to me*/
	void 	(__attr * report) (char *, va_list);
	void (__attr * dprintf) (char *, ...);
	
	/* Memory functions */
	void*	(__attr * memory_alloc) (void*, unsigned int);
	void*	(__attr * memory_calloc)
		(void*, unsigned int, unsigned int);
	void	(__attr * memory_free)
		(void*, void*, unsigned int);

	/* Timer functions and data structures. */
	void*		(__attr * timer_create)
			(void*, void (__attr2 *routine) (void*), void*);
	void		(__attr * timer_destroy) (void*, void*);
	void		(__attr * timer_start)
			(void*, void*, unsigned int, int);
	void		(__attr* timer_stop) (void*, void*);
	unsigned int 	(__attr * time_stamp_ms) (void*);
	unsigned int 	(__attr * time_stamp_us) (void*);

	/* Protection services */
	void*	(__attr * atomic_create) (void*, int);
	void	(__attr * atomic_destroy) (void*, void*);
	int	(__attr * atomic_inc) (void*, void*);
	int	(__attr * atomic_dec) (void*, void*);
	void*	(__attr * protect_create) (void*);
	void	(__attr * protect_destroy) (void*, void*);
	void	(__attr * protect_lock) (void*, void*);
	void	(__attr * protect_unlock) (void*, void*);

	/* Hardware access functions */
	void*		(__attr * hw_get_registers_addr) (void*);
	void*		(__attr * hw_get_memory_addr) (void*);

	/* Register access functions */
	void	(__attr * stall_u_sec) (void*, unsigned int);
	int	(__attr * receive_packet)
		(void*, void*, unsigned short);
	int	(__attr * get_eeeprom_image)
		(void*, unsigned char**, unsigned int*);
	void	(__attr * power_state_busy) (void*);
	void	(__attr * power_state_idle) (void*);
	void	(__attr * set_wake_on_gpio) (void*);
	void	(__attr * reset_wake_on_gpio) (void*);

	/*USB functions*/
	unsigned int 	(__attr * usb_write) 
			(void*, void*, usb_caller_extension_p,
			 usb_caller_extension_p);
	unsigned char* 	(__attr * usb_rx_buf_get) 
			(void*, unsigned int*, unsigned int*);
	unsigned int 	(__attr * usb_rx_buf_inc_ref) (unsigned int);
	int		(__attr * usb_rx_buf_free) (void*, unsigned int);
	unsigned int 	(__attr * usb_write_cmd_sync) 
			(void*, unsigned char*, unsigned int, 
			 unsigned char*, unsigned int, unsigned int*);
	unsigned int	(__attr * usb_write_cmd_async)
			(void*, unsigned char*, unsigned int, unsigned char*,
			 unsigned int*, usb_caller_extension_p);
	unsigned int	(__attr * usb_write_sync)
			(void*, int, unsigned char*, unsigned int, unsigned int*);
	unsigned int	(__attr * usb_read_sync)
			(void*, int, unsigned char*, unsigned int,
			 unsigned int, unsigned int*);
	int		(__attr * usb_is_alive) (void*);
	int		(__attr * usb_write_available) (void*);
	unsigned int	(__attr * usb_write_check_size) (void*);
	int		(__attr * usb_need_file_padding)
			(void*, unsigned int, unsigned int);
	int		(__attr * usb_start_recovery_worker)
			(void*, int (__attr2 *) (void*, void*), void*);

	/*IPC functions*/
	int		(__attr * ipc_event_send) 
			(void*, fwlanusb_event_t, void*, unsigned int);
	int		(__attr * ipc_unbound_event_send) (void*);
	
	char *		name;

} lib_interface_t, * lib_interface_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct __cb {

	void*	(__attr2 * config_mgr_create) (void*);
	int 	(__attr2 * config_mgr_start) (void*);
	int 	(__attr2 * config_mgr_unload) (void*);
	int	(__attr2 * config_mgr_stop) (void*);
	int 	(__attr2 * config_mgr_set_param) (void*, void*);
	int	(__attr2 * config_mgr_get_param) (void*, void*);
	int	(__attr2 * config_mgr_send_msdu) (void*, void*);
	int	(__attr2 * config_mgr_poll_ap_packets) (void*);
	int	(__attr2 * config_mgr_check_tx_queue_size) (void*, unsigned char);
	int	(__attr2 * config_mgr_mem_mngr_free_msdu) (void*, void*);
	int	(__attr2 * config_mgr_enable_interrupts) (void*);
	int	(__attr2 * config_mgr_handle_interrupts) (void*);
	unsigned long 	(__attr2 * msdu_2_buffer) 
			(unsigned short, unsigned int, void*, unsigned char*);
	int	(__attr2 * send_packet) (void*, void*, int);
	void	(__attr2 * msdu_2_skb) (void*, unsigned char*, unsigned short);
	int	(__attr2 * get_built_in_test_status_param) (void*);
	int	(__attr2 * do_ioctl) (void*, fwlanusb_ioctl_p);
	int	(__attr2 * register_events) (void*);
		
} lib_callback_t, * lib_callback_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#endif


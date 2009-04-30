/*
 * UNOFFICIAL BUT WORKING LTMODEM DRIVER for 2.6.x Linux kernels
 *  based on original code released by Agere Systems Inc.
 */
/*
 * new lt serial code is heavily based on drivers/serial/8250.c
 * from linux-2.6.0-test7. See that file for credits.
 * By Aleksey Kondratenko.
 *
 * no PM support
 */
/*
 * also based on old ltserial code original copyright notice follows
 *  (contains mostly old 2.4 serial.c credits)
 */
/*name and version number:@(#)serial24.c	1.2*/
/*date of get: 		  02/01/01 09:44:51*/
/*date of delta:	  02/01/01 09:43:04*/
/*
 *  linux/drivers/char/serial.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 1992, 1993, 1994, 1995, 1996, 1997,
 * 		1998, 1999  Theodore Ts'o
 *
 *  Extensively rewritten by Theodore Ts'o, 8/16/92 -- 9/14/92.  Now
 *  much more extensible to support other serial cards based on the
 *  16450/16550A UART's.  Added support for the AST FourPort and the
 *  Accent Async board.  
 *
 *  set_serial_info fixed to set the flags, custom divisor, and uart
 * 	type fields.  Fix suggested by Michael K. Johnson 12/12/92.
 *
 *  11/95: TIOCMIWAIT, TIOCGICOUNT by Angelo Haritsis <ah@doc.ic.ac.uk>
 *
 *  03/96: Modularised by Angelo Haritsis <ah@doc.ic.ac.uk>
 *
 *  rs_set_termios fixed to look also for changes of the input
 *      flags INPCK, BRKINT, PARMRK, IGNPAR and IGNBRK.
 *                                            Bernd Anhäupl 05/17/96.
 *
 *  1/97:  Extended dumb serial ports are a config option now.  
 *         Saves 4k.   Michael A. Griffith <grif@acm.org>
 * 
 *  8/97: Fix bug in rs_set_termios with RTS
 *        Stanislav V. Voronyi <stas@uanet.kharkov.ua>
 *
 *  3/98: Change the IRQ detection, use of probe_irq_o*(),
 *	  supress TIOCSERGWILD and TIOCSERSWILD
 *	  Etienne Lorrain <etienne.lorrain@ibm.net>
 *
 *  4/98: Added changes to support the ARM architecture proposed by
 * 	  Russell King
 *
 *  5/99: Updated to include support for the XR16C850 and ST16C654
 *        uarts.  Stuart MacDonald <stuartm@connecttech.com>
 *
 *  8/99: Generalized PCI support added.  Theodore Ts'o
 * 
 *  3/00: Rid circular buffer of redundant xmit_cnt.  Fix a
 *	  few races on freeing buffers too.
 *	  Alan Modra <alan@linuxcare.com>
 *
 *  5/00: Support for the RSA-DV II/S card added.
 *	  Kiyokazu SUTO <suto@ks-and-ks.ne.jp>
 * 
 *  6/00: Remove old-style timer, use timer_list
 *        Andrew Morton <andrewm@uow.edu.au>
 *
 *  7/00: Support Timedia/Sunix/Exsys PCI cards
 *
 *  7/00: fix some returns on failure not using MOD_DEC_USE_COUNT.
 *	  Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * This module exports the following rs232 io functions:
 *
 *	int rs_init(void);
 */
/*
 * Serial Interface for Modem Driver
 *
 * This module is adapted to be a serial interface driver for modem
 * controller drivers.  It provides a generic serial driver like
 * interface to the operating system.  It can work in conjunction
 * with a modem controller driver which supports the required
 * interface. The modem controller driver is expected to perform
 * core modem functions and interact with the modem hardware.
 */
/*
MRS changes
- various compiler warnings removed for -Wall
- Forced parameter to override detected settings
- label fixup errout1. this one is serious.
- change ttyS to ttyLT to avoid confusion and conflicts
- minor device to 64
- disable console and other features not needed
- allow interrupt sharing
- rename register_serial to avoid linkage conflict
*/

#include <linux/version.h>
/* #include <linux/config.h> */
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/serial_reg.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/tty_flip.h>

#include <asm/io.h>
#include <asm/irq.h>

#include <linux/serial_core.h>

#include "linuxif.h"

static struct ltmodem_ops lt_modem_ops;
static struct ltmodem_res lt_modem_res;

asmlinkage
int lt_virtual_rs_interrupt(void);

/*
 * Debugging.
 */
#if 0
#define DEBUG_INTR(fmt...)	printk("<7> " fmt)
#else
#define DEBUG_INTR(fmt...)	do { } while (0)
#endif

#include <asm/serial.h>


static
struct ltmodem_port {
	struct uart_port	port;
	unsigned char		ier;
	unsigned char		lcr;
	unsigned char		mcr_mask;	/* mask of user bits */
	unsigned char		mcr_force;	/* mask of forced bits */
} lt_port = {
	.port = {
		.type = PORT_16550A,
		.uartclk = BASE_BAUD * 16,
		.iotype = UPIO_PORT,
		.fifosize = 64
	},
	.mcr_mask = ~ALPHA_KLUDGE_MCR,
	.mcr_force = ALPHA_KLUDGE_MCR
};

// wrapper for DSP interrupt service routine */
// order of locks is insignificant here, I believe,
// because they are used in independent code parts
// whether taking lt_port.port.lock is necessary I don't know, but this
//  will not hurt
static
irqreturn_t	VMODEM_Hw_Int_Proc (int irq, void *dev_id, struct pt_regs * regs)
{
	spin_lock(&lt_port.port.lock);
	spin_lock(lt_modem_ops.io_lock);
	lt_modem_ops.dsp_isr();
	spin_unlock(lt_modem_ops.io_lock);
	spin_unlock(&lt_port.port.lock);
	return IRQ_HANDLED;
}

static inline
unsigned int serial_in(struct ltmodem_port *up, int offset)
{
	return lt_modem_ops.read_vuart_register(offset);
}

static inline
void serial_out(struct ltmodem_port *up, int offset, int value)
{
	lt_modem_ops.write_vuart_register(offset, value);
}

static void lt_stop_tx(struct uart_port *port)
{
	struct ltmodem_port *up = (struct ltmodem_port *)port;

	if (up->ier & UART_IER_THRI) {
		up->ier &= ~UART_IER_THRI;
		serial_out(up, UART_IER, up->ier);
	}
}

static void lt_start_tx(struct uart_port *port)
{
	struct ltmodem_port *up = (struct ltmodem_port *)port;

	if (!(up->ier & UART_IER_THRI)) {
		up->ier |= UART_IER_THRI;
		serial_out(up, UART_IER, up->ier);
	}
}

static void lt_stop_rx(struct uart_port *port)
{
	struct ltmodem_port *up = (struct ltmodem_port *)port;

	up->ier &= ~UART_IER_RLSI;
	up->port.read_status_mask &= ~UART_LSR_DR;
	serial_out(up, UART_IER, up->ier);
}

static void lt_enable_ms(struct uart_port *port)
{
	struct ltmodem_port *up = (struct ltmodem_port *)port;

	up->ier |= UART_IER_MSI;
	serial_out(up, UART_IER, up->ier);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,12))

static void
uart_insert_char(struct uart_port *port, unsigned int status,
		 unsigned int overrun, unsigned int ch, unsigned int flag)
{
	struct tty_struct *tty = port->info->tty;

	if ((status & port->ignore_status_mask & ~overrun) == 0)
		tty_insert_flip_char(tty, ch, flag);

	/*
	 * Overrun is special.  Since it's reported immediately,
	 * it doesn't affect the current character.
	 */
	if (status & ~port->ignore_status_mask & overrun)
		tty_insert_flip_char(tty, 0, TTY_OVERRUN);
}
#endif

static void
receive_chars(struct ltmodem_port *up, int *status, struct pt_regs *regs)
{
	unsigned char ch, flag;
	int lsr = *status;
	int max_count = 256;

	do {
		ch = serial_in(up, UART_RX);
		flag = TTY_NORMAL;
		up->port.icount.rx++;

		if (unlikely(*status & (UART_LSR_BI | UART_LSR_PE |
				       UART_LSR_FE | UART_LSR_OE))) {
			/*
			 * For statistics only
			 */
			if (*status & UART_LSR_BI) {
				*status &= ~(UART_LSR_FE | UART_LSR_PE);
				up->port.icount.brk++;
			} else if (*status & UART_LSR_PE)
				up->port.icount.parity++;
			else if (*status & UART_LSR_FE)
				up->port.icount.frame++;
			if (*status & UART_LSR_OE)
				up->port.icount.overrun++;

			/*
			 * Mask off conditions which should be ignored.
			 */
			*status &= up->port.read_status_mask;

			if (*status & UART_LSR_BI) {
				DEBUG_INTR("handling break....");
				flag = TTY_BREAK;
			} else if (*status & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (*status & UART_LSR_FE)
				flag = TTY_FRAME;
		}

		uart_insert_char(&up->port, lsr, UART_LSR_OE, ch, flag);

		lsr = serial_in(up, UART_LSR);
	} while ((lsr & UART_LSR_DR) && (max_count-- > 0));
	*status = lsr;
}

static void transmit_chars(struct ltmodem_port *up)
{
	struct circ_buf *xmit = &up->port.info->xmit;
	int count;

	if (up->port.x_char) {
		serial_out(up, UART_TX, up->port.x_char);
		up->port.icount.tx++;
		up->port.x_char = 0;
		return;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(&up->port)) {
		lt_stop_tx(&up->port);
		return;
	}

	count = up->port.fifosize;
	do {
		serial_out(up, UART_TX, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		up->port.icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);

	DEBUG_INTR("THRE...");

	if (uart_circ_empty(xmit))
		lt_stop_tx(&up->port);
}

static void check_modem_status(struct ltmodem_port *up)
{
	int status;

	status = serial_in(up, UART_MSR);

	if ((status & UART_MSR_ANY_DELTA) == 0)
		return;

	if (status & UART_MSR_TERI)
		up->port.icount.rng++;
	if (status & UART_MSR_DDSR)
		up->port.icount.dsr++;
	if (status & UART_MSR_DDCD)
		uart_handle_dcd_change(&up->port, status & UART_MSR_DCD);
	if (status & UART_MSR_DCTS)
		uart_handle_cts_change(&up->port, status & UART_MSR_CTS);

	wake_up_interruptible(&up->port.info->delta_msr_wait);
}

// this is called from timertick_function
asmlinkage
int	lt_virtual_rs_interrupt(void)
{
	unsigned long flags;
	unsigned int status;
	unsigned int iir;
	spin_lock_irqsave(&lt_port.port.lock,flags);
	status = serial_in(&lt_port,UART_LSR);
	iir = serial_in(&lt_port,UART_IIR);
	DEBUG_INTR("status = %x...",status);
	if (iir & UART_IIR_NO_INT)
		goto out;
	if (status & UART_LSR_DR) {
		DEBUG_INTR("ltmodem: dr is set!\n");
		receive_chars(&lt_port,&status,0);

		{
			struct tty_struct *tty = lt_port.port.info->tty;
			spin_unlock_irqrestore(&lt_port.port.lock, flags);
			tty_flip_buffer_push(tty);
			spin_lock_irqsave(&lt_port.port.lock,flags);
		}
	}
	check_modem_status(&lt_port);
	if (status & UART_LSR_THRE)
		transmit_chars(&lt_port);
out:
	spin_unlock_irqrestore(&lt_port.port.lock,flags);
	return 0;
}

static unsigned int lt_tx_empty(struct uart_port *port)
{
	struct ltmodem_port *up = (struct ltmodem_port *)port;
	unsigned long flags;
	unsigned int ret;

	spin_lock_irqsave(&up->port.lock, flags);
	ret = serial_in(up, UART_LSR) & UART_LSR_TEMT ? TIOCSER_TEMT : 0;
	spin_unlock_irqrestore(&up->port.lock, flags);

	return ret;
}

static unsigned int lt_get_mctrl(struct uart_port *port)
{
	struct ltmodem_port *up = (struct ltmodem_port *)port;
	unsigned long flags;
	unsigned char status;
	unsigned int ret;

	spin_lock_irqsave(&up->port.lock, flags);
	status = serial_in(up, UART_MSR);
	spin_unlock_irqrestore(&up->port.lock, flags);

	ret = 0;
	if (status & UART_MSR_DCD)
		ret |= TIOCM_CAR;
	if (status & UART_MSR_RI)
		ret |= TIOCM_RNG;
	if (status & UART_MSR_DSR)
		ret |= TIOCM_DSR;
	if (status & UART_MSR_CTS)
		ret |= TIOCM_CTS;
	return ret;
}

static void lt_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct ltmodem_port *up = (struct ltmodem_port *)port;
	unsigned char mcr = 0;

	if (mctrl & TIOCM_RTS)
		mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		mcr |= UART_MCR_LOOP;

	mcr = (mcr & up->mcr_mask) | up->mcr_force;

	serial_out(up, UART_MCR, mcr);
}

static void lt_break_ctl(struct uart_port *port, int break_state)
{
	struct ltmodem_port *up = (struct ltmodem_port *)port;
	unsigned long flags;

	spin_lock_irqsave(&up->port.lock, flags);
	if (break_state == -1)
		up->lcr |= UART_LCR_SBC;
	else
		up->lcr &= ~UART_LCR_SBC;
	serial_out(up, UART_LCR, up->lcr);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static void
lt_set_termios(struct uart_port *port, struct termios *termios,
		       struct termios *old)
{
	struct ltmodem_port *up = (struct ltmodem_port *)port;
	unsigned char cval, fcr = 0;
	unsigned long flags;
	unsigned int baud, quot;

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		cval = 0x00;
		break;
	case CS6:
		cval = 0x01;
		break;
	case CS7:
		cval = 0x02;
		break;
	default:
	case CS8:
		cval = 0x03;
		break;
	}

	if (termios->c_cflag & CSTOPB)
		cval |= 0x04;
	if (termios->c_cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(termios->c_cflag & PARODD))
		cval |= UART_LCR_EPAR;
#ifdef CMSPAR
	if (termios->c_cflag & CMSPAR)
		cval |= UART_LCR_SPAR;
#endif

	/*
	 * Ask the core to calculate the divisor for us.
	 */
	baud = uart_get_baud_rate(port, termios, old, 0, port->uartclk/16); 
	quot = uart_get_divisor(port, baud);

#ifdef LT_USE_FIFO
	if (baud < 2400)
		fcr = UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_1;
	else
		fcr = UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_8;
#endif /* LT_USE_FIFO */
	/*
	 * Ok, we're now changing the port state.  Do it with
	 * interrupts disabled.
	 */
	spin_lock_irqsave(&up->port.lock, flags);

	/* 
         * /\*
	 *  * Update the per-port timeout.
	 *  *\/
	 * uart_update_timeout(port, termios->c_cflag, baud);
         */

	up->port.read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		up->port.read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		up->port.read_status_mask |= UART_LSR_BI;

	/*
	 * Characteres to ignore
	 */
	up->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		up->port.ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		up->port.ignore_status_mask |= UART_LSR_BI;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			up->port.ignore_status_mask |= UART_LSR_OE;
	}

	/*
	 * ignore all characters if CREAD is not set
	 */
	if ((termios->c_cflag & CREAD) == 0)
		up->port.ignore_status_mask |= UART_LSR_DR;

	/*
	 * CTS flow control flag and modem status interrupts
	 */
	up->ier &= ~UART_IER_MSI;
	if (UART_ENABLE_MS(&up->port, termios->c_cflag))
		up->ier |= UART_IER_MSI;

	serial_out(up, UART_IER, up->ier);

	serial_out(up, UART_LCR, cval | UART_LCR_DLAB);/* set DLAB */

	serial_out(up, UART_DLL, quot & 0xff);		/* LS of divisor */
	serial_out(up, UART_DLM, quot >> 8);		/* MS of divisor */
	serial_out(up, UART_LCR, cval);		/* reset DLAB */
	up->lcr = cval;					/* Save LCR */
#ifdef LT_USE_FIFO
	if (fcr & UART_FCR_ENABLE_FIFO) {
		/* emulated UARTs (Lucent Venus 167x) need two steps */
		serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO);
	}
#endif /* LT_USE_FIFO */
	serial_out(up, UART_FCR, fcr);		/* set fcr */
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static int lt_startup(struct uart_port *port)
{
	struct ltmodem_port *up = (struct ltmodem_port *)port;
	unsigned long flags;

	// force low latency mode on tty.
	// We will push data from buttom half, no need to delay it further.
	port->info->tty->low_latency = 1;

	lt_modem_ops.PortOpen();

#ifdef LT_USE_FIFO
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
			UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);
#endif /* LT_USE_FIFO */

	/*
	 * Clear the interrupt registers.
	 */
	(void) serial_in(up, UART_LSR);
	(void) serial_in(up, UART_RX);
	(void) serial_in(up, UART_IIR);
	(void) serial_in(up, UART_MSR);

	// irq_link_chain was here

	/*
	 * Now, initialize the UART
	 */
	serial_out(up, UART_LCR, UART_LCR_WLEN8);

	spin_lock_irqsave(&up->port.lock, flags);
	up->port.mctrl |= TIOCM_OUT2;
	lt_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, flags);

	/*
	 * Finally, enable interrupts.  Note: Modem status interrupts
	 * are set via set_termios(), which will be occurring imminently
	 * anyway, so we don't enable them here.
	 */
	up->ier = UART_IER_RLSI | UART_IER_RDI;
	serial_out(up, UART_IER, up->ier);

	/*
	 * And clear the interrupt registers again for luck.
	 */
	(void) serial_in(up, UART_LSR);
	(void) serial_in(up, UART_RX);
	(void) serial_in(up, UART_IIR);
	(void) serial_in(up, UART_MSR);

	return 0;
}

static void lt_shutdown(struct uart_port *port)
{
	struct ltmodem_port *up = (struct ltmodem_port *)port;
	unsigned long flags;

	/*
	 * Disable interrupts from this port
	 */
	up->ier = 0;
	serial_out(up, UART_IER, 0);

	spin_lock_irqsave(&up->port.lock, flags);
	up->port.mctrl &= ~TIOCM_OUT2;

	lt_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, flags);

	/*
	 * Disable break condition and FIFOs
	 */
	serial_out(up, UART_LCR, serial_in(up, UART_LCR) & ~UART_LCR_SBC);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
				  UART_FCR_CLEAR_RCVR |
				  UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);

	/*
	 * Read data port to reset things, and then unlink from
	 * the IRQ chain.
	 */
	(void) serial_in(up, UART_RX);

	lt_modem_ops.PortClose();
}

static int lt_request_port(struct uart_port *port)
{
	return 0;
}

// port settings cannot be changed by setserial
// but we work-around broken wvdial which fails if baud rate cannot be changed
static int
lt_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	// disallow changing important port parameters (see serial_core.c:uart_set_info)
	if (ser->irq != port->irq
	    || ser->port != port->iobase
	    || (unsigned long)ser->iomem_base != port->mapbase
	    || ser->hub6 != port->hub6
	    || ser->io_type != port->iotype
	    || ser->iomem_reg_shift != port->regshift
	    || ser->type != port->type)
		return -EINVAL;
	return 0;
}

static const char *
lt_type(struct uart_port *port)
{
	return "Lucent/Agere Modem";
}

/*
 * Those magic Lucent IOCTLs
 */
static
int	lt_ioctl(struct uart_port *port,unsigned int cmd,unsigned long arg)
{
	switch (cmd) {
		case IOCTL_MODEM_APP_1:
		case IOCTL_MODEM_APP_2:
		case IOCTL_MODEM_APP_3:
		case IOCTL_MODEM_APP_4:
		case IOCTL_MODEM_APP_5:
		case IOCTL_MODEM_APP_6:
		case IOCTL_MODEM_APP_7:
		case IOCTL_MODEM_APP_8:
			lt_modem_ops.app_ioctl_handler(cmd,arg);
			return 0;
	}
	return -ENOIOCTLCMD;
}


static struct uart_ops lt_pops = {
	.tx_empty	= lt_tx_empty,
	.set_mctrl	= lt_set_mctrl,
	.get_mctrl	= lt_get_mctrl,
	.stop_tx	= lt_stop_tx,
	.start_tx	= lt_start_tx,
	.stop_rx	= lt_stop_rx,
	.enable_ms	= lt_enable_ms,
	.break_ctl	= lt_break_ctl,
	.startup	= lt_startup,
	.shutdown	= lt_shutdown,
	.set_termios	= lt_set_termios,
	.type		= lt_type,
	.release_port	= (void (*)(struct uart_port *)) lt_request_port,
	// same as lt_request_port (empty)
	.request_port	= (int (*)(struct uart_port *)) lt_request_port,
	// same as lt_request_port (empty)
	.config_port	= (void (*)(struct uart_port *,int)) lt_request_port,
	.verify_port	= lt_verify_port,
	.ioctl		= lt_ioctl,
};

static struct uart_driver lt_serial_reg = {
	.owner			= THIS_MODULE,
	.driver_name		= "ltserial",
#ifdef LT_USE_OLD_NAMES
	.dev_name		= "ttySLT",
#else
	.dev_name		= "ttySLTM",
#endif
	.major			= 62,
	.minor			= 64,
	.nr			= 1,
	.cons			= 0,
};

// breaks some isapnp modems
/* 
 * static
 * void	lt_release_resources(int max)
 * {
 * 	struct pci_dev *dev = lt_get_dev();
 * 	struct resource *res;
 * 
 * 	if (!dev)
 * 		return;
 * 	if (!max)
 * 		max = DEVICE_COUNT_RESOURCE;
 * 	res = dev->resource+max-1;
 * 	for (;max-->0;res--) {
 * 		if (!res->start)
 * 			continue;
 * 		if (res->flags & IORESOURCE_IO)
 * 			release_region(res->start,res->end-res->start);
 * 		else if (res->flags & IORESOURCE_MEM)
 * 			release_mem_region(res->start,res->end-res->start);
 * 	}
 * }
 */

// ltmodem uart is virtual. Lets claim resources (DSP resources) here
//  and get rid of config_port
// breaks some isapnp modems
/* 
 * static
 * int	lt_request_resources(void)
 * {
 * 	struct pci_dev *dev = lt_get_dev();
 * 	struct resource *res = dev->resource;
 * 	int i;
 * 
 * 	if (!dev) {
 * 		printk(KERN_INFO"ltserial: Hm, pci device struct addr for ltmodem is 0\n");
 * 		printk(KERN_INFO"ltserial: Assuming ISA modem\n");
 * 		return 0;
 * 	}
 * 	for (i=0;i<DEVICE_COUNT_RESOURCE;i++,res++) {
 * 		if (!res->start)
 * 			continue;
 * 		if (res->flags & IORESOURCE_IO) {
 * 			if (!request_region(res->start,res->end-res->start,ltserialstring))
 * 				break;
 * 		} else if (res->flags & IORESOURCE_MEM)
 * 			if (!request_mem_region(res->start,res->end-res->start,ltserialstring))
 * 				break;
 * 	}
 * 	if (i<DEVICE_COUNT_RESOURCE) {
 * 		printk(KERN_ERR"lt_request_resources failed\n");
 * 		lt_release_resources(i);
 * 		return -EBUSY;
 * 	}
 * 	return 0;
 * }
 */

static int __init lt_init(void)
{
	int ret;

	lt_get_modem_interface(&lt_modem_ops);
	if (lt_modem_ops.detect_modem(&lt_modem_res)) {
		printk(KERN_ERR"ltserial: No device detected\n");
		return -ENODEV;
	}

	if ((ret = uart_register_driver(&lt_serial_reg)) < 0) {
		printk(KERN_ERR"ltserial: unable to register driver\n");
		goto out_pci_dev;
	}

	lt_port.port.irq	= irq_canonicalize(lt_modem_res.Irq);
	lt_port.port.iobase	= lt_modem_res.BaseAddress;
	lt_port.port.ops	= &lt_pops;

	if ((ret = uart_add_one_port(&lt_serial_reg, &lt_port.port)) < 0) {
		printk(KERN_ERR"ltserial: unable to register port\n");
		goto out_driver;
	}

	lt_modem_ops.init_modem();
	*lt_modem_ops.virtual_isr_ptr = lt_virtual_rs_interrupt;

	/* lets try to grap dsp interrupt here */
	ret = request_irq(lt_modem_res.Irq, VMODEM_Hw_Int_Proc, IRQF_DISABLED | IRQF_SHARED,
				ltserialstring, &lt_modem_res.BaseAddress);
	if (ret<0)
		goto out_port;
/*	ret = lt_request_resources();
	if (ret<0)
		goto out_irq;*/

	return 0;
/*out_irq:
	free_irq(lt_modem_res.Irq,&lt_modem_res.BaseAddress);*/
out_port:
	*lt_modem_ops.virtual_isr_ptr = 0;
	uart_remove_one_port(&lt_serial_reg,&lt_port.port);
out_driver:
	uart_unregister_driver(&lt_serial_reg);
out_pci_dev:
	lt_modem_ops.put_pci_dev();
	return ret;
}

static void __exit lt_exit(void)
{
	*lt_modem_ops.virtual_isr_ptr = 0;
//	lt_release_resources(0);
	free_irq(lt_modem_res.Irq, &lt_modem_res.BaseAddress);
	uart_remove_one_port(&lt_serial_reg, &lt_port.port);
	uart_unregister_driver(&lt_serial_reg);
	lt_modem_ops.put_pci_dev();
}

module_init(lt_init);
module_exit(lt_exit);

// this must (?) be GPL since it is based on GPL code from kernel
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Lucent/Agere linmodem serial port driver");

/* which is what binary core searches for */
static struct pci_device_id ltmodem_pci_ids[] = {
	{PCI_VENDOR_ID_ATT, 0x440, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x441, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x442, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x443, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x444, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x445, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x446, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x447, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x448, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x445, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x446, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x447, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x448, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x449, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x44a, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x44b, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x44c, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x44d, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x44e, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x44f, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x450, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x451, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x452, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x453, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x454, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x455, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x456, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x457, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x458, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x459, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x45a, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x45b, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x45c, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x45d, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x45e, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_ATT, 0x45f, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},

	{PCI_VENDOR_ID_XIRCOM, 0x440, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x441, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x442, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x443, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x444, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x445, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x446, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x447, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x448, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x445, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x446, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x447, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x448, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x449, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x44a, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x44b, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x44c, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x44d, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x44e, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x44f, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x450, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x451, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x452, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x453, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x454, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x455, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x456, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x457, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x458, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x459, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x45a, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x45b, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_XIRCOM, 0x45c, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},

	/*
	 * core also searches for PCI_VENDOR_ID_XIRCOM and devices within
	 *  range 0x0000-0x00d4.
	 * And lt_modem.c also searches PCI_VENDOR_ID_XIRCOR with devices within
	 *  range 0x0010-0x03ff.
	 */
	{0,}
};

MODULE_DEVICE_TABLE(pci, ltmodem_pci_ids);

/*
 * Intel Wireless WiMAX Connection 2400m
 * Error Density Count: cheapo error density (over time) counter
 *
 *
 *
 * Copyright (C) 2005-2007 Intel Corporation <linux-wimax@intel.com>
 * Reinette Chatre <reinette.chatre@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * Embed an 'struct edc' somewhere. Each time there is a soft or
 * retryable error, call edc_inc() and check if there error top
 * watermark has been reached.
 */

#ifndef __EDC_H__
#define __EDC_H__

enum {
	EDC_MAX_ERRORS = 10,
	EDC_ERROR_TIMEFRAME = HZ,
};

/* error density counter */
struct edc {
	unsigned long timestart;
	u16 errorcount;
};

static inline void edc_init(struct edc *edc)
{
	edc->timestart = jiffies;
}

/**
 * edc_inc - report a soft error and check if we are over the watermark
 *
 * @edc: pointer to error density counter.
 * @max_err: maximum number of errors we can accept over the timeframe
 * @timeframe: lenght of the timeframe (in jiffies).
 *
 * Returns: !0 1 if maximum acceptable errors per timeframe has been
 *     exceeded. 0 otherwise.
 *
 * This is way to determine if the number of acceptable errors per time
 * period has been exceeded. It is not accurate as there are cases in which
 * this scheme will not work, for example if there are periodic occurences
 * of errors that straddle updates to the start time. This scheme is
 * sufficient for our usage.
 *
 * To use, embed a 'struct edc' somewhere, initialize it with
 * edc_init() and when an error hits:
 *
 * if (do_something_fails_with_a_soft_error) {
 *        if (edc_inc(&my->edc, MAX_ERRORS, MAX_TIMEFRAME))
 * 	           Ops, hard error, do something about it
 *        else
 *                 Retry or ignore, depending on whatever
 * }
 */
static inline int edc_inc(struct edc *edc, u16 max_err, u16 timeframe)
{
	unsigned long now;

	now = jiffies;
	if (now - edc->timestart > timeframe) {
		edc->errorcount = 1;
		edc->timestart = now;
	} else if (++edc->errorcount > max_err) {
		edc->errorcount = 0;
		edc->timestart = now;
		return 1;
	}
	return 0;
}

#endif /* #ifndef __EDC_H__ */

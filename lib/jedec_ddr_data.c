/*
 * DDR addressing details and AC timing parameters from JEDEC specs
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 *
 * Aneesh V <aneesh@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <memory/jedec_ddr.h>
#include <linux/module.h>

const struct lpddr2_min_tck lpddr2_jedec_min_tck = {
	.tRPab		= 3,
	.tRCD		= 3,
	.tWR		= 3,
	.tRASmin	= 3,
	.tRRD		= 2,
	.tWTR		= 2,
	.tXP		= 2,
	.tRTP		= 2,
	.tCKE		= 3,
	.tCKESR		= 3,
	.tFAW		= 8
};
EXPORT_SYMBOL_GPL(lpddr2_jedec_min_tck);

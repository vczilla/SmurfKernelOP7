/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DSI_HW_H_
#define _DSI_HW_H_
#include <linux/io.h>

#define DSI_R32(dsi_hw, off) readl_relaxed_no_log((dsi_hw)->base + (off))
#define DSI_W32(dsi_hw, off, val) \
	do {\
		pr_debug("[DSI_%d][%s] - [0x%08x]\n", \
			(dsi_hw)->index, #off, (uint32_t)(val)); \
		writel_relaxed_no_log((val), (dsi_hw)->base + (off)); \
	} while (0)

#define DSI_MMSS_MISC_R32(dsi_hw, off) \
	readl_relaxed_no_log((dsi_hw)->mmss_misc_base + (off))
#define DSI_MMSS_MISC_W32(dsi_hw, off, val) \
	do {\
		pr_debug("[DSI_%d][%s] - [0x%08x]\n", \
			(dsi_hw)->index, #off, val); \
		writel_relaxed_no_log((val), (dsi_hw)->mmss_misc_base + (off)); \
	} while (0)

#define DSI_MISC_R32(dsi_hw, off) \
	readl_relaxed_no_log((dsi_hw)->phy_clamp_base + (off))
#define DSI_MISC_W32(dsi_hw, off, val) \
	do {\
		pr_debug("[DSI_%d][%s] - [0x%08x]\n", \
			(dsi_hw)->index, #off, val); \
		writel_relaxed_no_log((val), (dsi_hw)->phy_clamp_base + (off)); \
	} while (0)
#define DSI_DISP_CC_R32(dsi_hw, off) \
	readl_relaxed_no_log((dsi_hw)->disp_cc_base + (off))
#define DSI_DISP_CC_W32(dsi_hw, off, val) \
	do {\
		pr_debug("[DSI_%d][%s] - [0x%08x]\n", \
			(dsi_hw)->index, #off, val); \
		writel_relaxed_no_log((val), (dsi_hw)->disp_cc_base + (off)); \
	} while (0)

#define DSI_R64(dsi_hw, off) readq_relaxed((dsi_hw)->base + (off))
#define DSI_W64(dsi_hw, off, val) writeq_relaxed((val), (dsi_hw)->base + (off))

#define PLL_CALC_DATA(addr0, addr1, data0, data1)      \
	(((data1) << 24) | ((((addr1)/4) & 0xFF) << 16) | \
	 ((data0) << 8) | (((addr0)/4) & 0xFF))

#define DSI_DYN_REF_REG_W(base, offset, addr0, addr1, data0, data1)   \
	writel_relaxed_no_log(PLL_CALC_DATA(addr0, addr1, data0, data1), \
			(base) + (offset))

#define DSI_GEN_R32(base, offset) readl_relaxed_no_log(base + (offset))
#define DSI_GEN_W32(base, offset, val) writel_relaxed_no_log((val), base + (offset))
#endif /* _DSI_HW_H_ */

/*
 * ***************************************************************************
 * Copyright (C) 2015 Marvell International Ltd.
 * ***************************************************************************
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 2 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ***************************************************************************
*/

#ifndef _CLOCK_H_
#define _CLOCK_H_

#include "types.h"

enum clk_preset {
	CLK_PRESET_CPU600_DDR600  = 0,
	CLK_PRESET_CPU800_DDR800,
	CLK_PRESET_CPU1000_DDR800,
	CLK_PRESET_CPU1200_DDR750,
	CLK_PRESET_MAX,
};

int set_clock_preset(enum clk_preset idx);
int get_cpu_clock(void);
int get_ddr_clock(void);
int setup_clock_tree(void);

u32 get_ref_clk(void);
u32 get_cm3_clk(void);
void wait_ns(u32 wait_ns);
void udelay(u32 us);

#endif /* _CLOCK_H_ */

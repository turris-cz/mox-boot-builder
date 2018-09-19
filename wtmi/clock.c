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

#include "types.h"
#include "io.h"
#include "clock.h"

#define NB_TBG_CFG		0xc0013200
#define NB_TBG_CTRL0		0xc0013204
#define NB_TBG_CTRL1		0xc0013208
#define NB_TBG_CTRL2		0xc001320c
#define NB_TBG_CTRL3		0xc0013210
#define NB_TBG_CTRL4		0xc0013214
#define NB_TBG_CTRL5		0xc0013218
#define NB_TBG_CTRL6		0xc001321c
#define NB_TBG_CTRL7		0xc0013220
#define NB_TBG_CTRL8		0xc0013230

#define NB_TBG_SEL		0xc0013000
#define NB_DIV_SEL0		0xc0013004
#define NB_DIV_SEL1		0xc0013008
#define NB_DIV_SEL2		0xc001300c
#define NB_CLK_SEL		0xc0013010

#define TEST_PIN_LATCH_N	0xc0013808
#define XTAL_CLK_40MHZ		BIT(9)

#define CYCLES_PER_LOOP		3	/* see wait_ns() implementation for loop content */
static u32 clk_ns, loop_ns;

/***************************************************************************************************
  * get_ref_clk
  *
  * return: reference clock in MHz (25 or 40)
 ***************************************************************************************************/
u32 get_ref_clk(void)
{
	if (readl(TEST_PIN_LATCH_N) & XTAL_CLK_40MHZ)
		return 40;
	else
		return 25;
}

/***************************************************************************************************
  * get_tbg_clk
  *
  * return: TBG-X clock in MHz
 ***************************************************************************************************/
static u32 get_tbg_clk(enum clock_line tbg_typ)
{
	u32 tbg_M, tbg_N, vco_div;
	u32 ref, reg_val;

	/* TBG frequency is calculated according to formula:
		Ftbg = Fkvco / vco_div
	   where
		Fkvco = N * Fref * 4 / M

	   N - TBG_A_FBDIV or TBG_B_FBDIV
	   M - TBG_A_REFDIV or TBG_B_REFDIV
	   vco_div - TBG_A_VCODIV_SEL or TBG_B_VCODIV_SEL
	   Fref - reference clock 25MHz or 40 MHz
	*/

	/* get ref clock */
	ref = get_ref_clk();

	/* get M, N */
	reg_val = readl(NB_TBG_CTRL7);
	tbg_M = ((tbg_typ == TBG_A_S) || (tbg_typ == TBG_A_P)) ?
		(reg_val & 0x1FFUL) : ((reg_val >> 16) & 0x1FFUL);
	tbg_M = (tbg_M == 0) ? 1 : tbg_M;

	reg_val = readl(NB_TBG_CTRL0);
	tbg_N = ((tbg_typ == TBG_A_S) || (tbg_typ == TBG_A_P)) ?
		((reg_val >> 2) & 0x1FFUL) : ((reg_val >> 18) & 0x1FFUL);

	if ((tbg_typ == TBG_A_S) || (tbg_typ == TBG_B_S)) {
		/* get SE VCODIV */
		reg_val = readl(NB_TBG_CTRL1);
		reg_val = (tbg_typ == TBG_A_S) ?
			  (reg_val & 0x1FFUL) : ((reg_val >> 16) & 0x1FFUL);
	} else {
		/* get DIFF VCODIV */
		reg_val = readl(NB_TBG_CTRL8);
		reg_val = (tbg_typ == TBG_A_P) ?
			  ((reg_val >> 1) & 0x1FFUL) : ((reg_val >> 17) & 0x1FFUL);
	}
	if (reg_val > 7)
		return 0;

	vco_div = 0x1 << reg_val;

	return ((tbg_N * ref) << 2)/(tbg_M * vco_div);
}

/***************************************************************************************************
  * get_cm3_clk
  *
  * return: CM3 CPU clock in MHz
 ***************************************************************************************************/
u32 get_cm3_clk(void)
{
	u32 tbg, cm3_pre_scl1, cm3_pre_scl2;
	enum clock_line tbg_typ;

	/* 1. check CM3 clock select */
	if (!((readl(NB_CLK_SEL) >> 4) & 0x1))
		return get_ref_clk(); /* CPU clock is using XTAL output*/

	/* 2. get TSECM TBG select */
	tbg_typ = (readl(NB_TBG_SEL) >> 8) & 0x3;

	/* 3. get TBG clock */
	tbg = get_tbg_clk(tbg_typ);
	if (tbg == 0)
		return 0;

	/* 4. get CM3 clk dividers */
	cm3_pre_scl1 = (readl(NB_DIV_SEL1) >> 15) & 0x7;
	if (cm3_pre_scl1 == 7 || cm3_pre_scl1 == 0)
		return 0; /* divider value error */

	cm3_pre_scl2 = (readl(NB_DIV_SEL1) >> 12) & 0x7;
	if (cm3_pre_scl2 == 7 || cm3_pre_scl2 == 0)
		return 0; /* divider value error */

	return tbg/(cm3_pre_scl1 * cm3_pre_scl2);
}

/***************************************************************************************************
  * wait_ns
  *
  * return: None
 ***************************************************************************************************/
void wait_ns(u32 wait_ns)
{
	u32 loop = wait_ns / loop_ns;

	asm volatile("0:" "subs %[count], 1;" "bne 0b;" :[count]"+r"(loop));
}

/***************************************************************************************************
  * clock_init
  *
  * return: None
 ***************************************************************************************************/
u32 clock_init(void)
{
	u32 cm3_clk = get_cm3_clk();
	u32 status = ERR_BAD_CONFIGURATION;

	if (cm3_clk != 0) {
		clk_ns = 1000 / cm3_clk;
		loop_ns = clk_ns / CYCLES_PER_LOOP;
		status = NO_ERROR;
	}

	return status;
}

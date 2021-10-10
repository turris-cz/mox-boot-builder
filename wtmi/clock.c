/*
 * ***************************************************************************
 * Copyright (C) 2017 Marvell International Ltd.
 * ***************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of Marvell nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************
 */

#include "io.h"
#include "clock.h"
#include "stdio.h"
#include "errno.h"
#include "debug.h"

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
#define NB_TBG_SEL_MASK		(0x3F << 26)
#define NB_DIV_SEL0		0xc0013004
#define NB_DIV_SEL0_MASK	(BIT(31) | (0x3 << 26) | BIT(19) | (0x3F << 6))
#define NB_DIV_SEL1		0xc0013008
#define NB_DIV_SEL1_MASK	((0x3 << 30) | (0x7 << 21))
#define NB_DIV_SEL2		0xc001300c
#define NB_DIV_SEL2_MASK	(BIT(31) | (0xF << 3))
#define NB_CLK_SEL		0xc0013010
#define NB_CLK_EN		0xc0013014
#define NB_ALL_CLK_ENABLE	0

#define SB_TBG_SEL		0xc0018000
#define SB_TBG_SEL_MASK		((0x3FF << 22) | (0x3 << 12) | 0x3F)
#define SB_DIV_SEL0		0xc0018004
#define SB_DIV_SEL0_MASK	((0x1F << 27) | 0x7)
#define SB_DIV_SEL1		0xc0018008
#define SB_DIV_SEL1_MASK	((0xFF << 24) | (0x7 << 15) | 0x3F)
#define SB_DIV_SEL2		0xc001800c
#define SB_DIV_SEL2_MASK	((0xFFFFF << 12) | 0x3F)
#define SB_CLK_SEL		0xc0018010
#define SB_CLK_EN		0xc0018014
/* (GBE0 and GBE1 have reversed logic) */
#define SB_ALL_CLK_ENABLE	(BIT(19) | BIT(20))

#define TEST_PIN_LATCH_N	0xc0013808
#define XTAL_CLK_40MHZ		BIT(9)

/* settings for SSC clock */
#define SSC_MODULATION_FREQ 32500
#define SSC_AMPLITUDE_FREQ 14000000

/* Init values for the static clock configurations array */
/*
*************************************************************************************************************************************
Configuration 0 	CPU 600, DDR 600

		SE DIV	DIFF DIV
KVCO-A	2400	1	1
KVCO-B	2000	1	1
=====================================================================================================================================
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-A-S 1200	3	1	sdio_pclk_sel		400	SB					DIV0[8:6]	DIV0[5:3]
		1	NA	ddr_phy_mck_div_sel	600	NB	DIV0[18]	NA
		3	1	mmc_pclk_sel		400	NB	DIV2[15:13]	DIV2[18:16]
-------------------------------------------------------------------------------------------------------------------------------------
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-A-P 1200	2	NA	a53_cpu_pclk_sel		600	NB	DIV0[30:28]	NA
		2	NA	wc_ahb_div_sel		300	NB	DIV2[2:0]	NA
		0	NA	atb_clk_div_sel		600	NB	DIV2[26:5]	NA
		1	NA	plkdbg_clk_div_sel	300	NB	DIV2[30:29]	NA
		2	NA	setm_tmx_pclk_sel	600	NB	DIV1[20:18]	NA
		2	NA	trace_pclk_sel		600	NB	DIV0[22:20]	NA
		3	2	tsecm_pclk_sel		200	NB	DIV1[14:12]	DIV1[17:15]
		2	3	sqf_pclk_sel		200	NB	DIV1[26:24]	DIV1[29:27]
-------------------------------------------------------------------------------------------------------------------------------------
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-B_S 1000	1	2	eip97_pclk_sel		500	NB	DIV2[24:22]	DIV2[21:19]
		5	2	pwm_pclk_sel		100	NB	DIV0[5:3]	DIV0[2:0]
		5	1	sec_at_pclk_sel		200	NB	DIV1[5:3]	DIV1[2:0]
		5	2	sec_dap_pclk_sel	100	NB	DIV1[11:9]	DIV1[8:6]
		5	1	sata_host_pclk_sel	200	NB	DIV2[9:7]	DIV2[12:10]
		4	2	ddr_fclk_pclk_sel	125	NB	DIV0[17:15]	DIV0[14:12]
		6	NA	counter_pclk_sel	167	NB	DIV0[25:23]	NA
		1	NA	cpu_cnt_clk_div_sel	83	NB	DIV2[28:27]	NA
		4	1	gbe_core_pclk_sel	250	SB					DIV1[20:18]	DIV1[23:21]
		1	NA	gbe_bm_core_clk_div2_sel250	SB					DIV1[12]	NA
		1	NA	gbe1_core_clk_div2_sel	250	SB					DIV1[13]	NA
		1	NA	gbe0_core_clk_div2_sel	250	SB					DIV1[14]	NA
		4	2	gbe_125_pclk_sel	125	SB					DIV1[11:9]	DIV1[8:6]
		5	1	sb_axi_pclk_sel		200	SB					DIV0[20:18]	DIV0[23:21]
		4	2	usb32_ss_sys_pclk_sel	125	SB					DIV0[17:15]	DIV0[20:18]
		5	2	usb32_usb2_sys_pclk_sel	100	SB					DIV0[14:12]	DIV0[11:9]
-------------------------------------------------------------------------------------------------------------------------------------
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-B-P 1000	4	5	gbe_50_pclk_sel		50	SB					DIV2[11:9]	DIV2[8:6]
*************************************************************************************************************************************
*************************************************************************************************************************************
Configuration 1	CPU 800, DDR 800

		SE DIV	DIFF DIV
KVCO-A	1600	0	1
KVCO-B	2000	1	2
=====================================================================================================================================
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-A-S 1600	4	1	mmc_pclk_sel		400	NB	DIV2[15:13]	DIV2[18:16]
		1	NA	ddr_phy_mck_div_sel	800	NB	DIV0[18]	NA
		4	1	sdio_pclk_sel		400	SB					DIV0[8:6]	DIV0[5:3]
-------------------------------------------------------------------------------------------------------------------------------------
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-A-P 800	5	NA	counter_pclk_sel	160	NB	DIV0[25:23]	NA
		1	NA	cpu_cnt_clk_div_sel	80	NB	DIV2[28:27]	NA
		4	1	sqf_pclk_sel		200	NB	DIV1[26:24]	DIV1[29:27]
		4	1	sata_host_pclk_sel	200	NB	DIV2[9:7]	DIV2[12:10]
		4	1	tsecm_pclk_sel		200	NB	DIV1[14:12]	DIV1[17:15]
		2	1	eip97_pclk_sel		400	NB	DIV2[24:22]	DIV2[21:19]
		1	NA	setm_tmx_pclk_sel	800	NB	DIV1[20:18]	NA
		1	NA	trace_pclk_sel		800	NB	DIV0[22:20]	NA
		1	NA	a53_cpu_pclk_sel	800	NB	DIV0[30:28]	NA
		4	NA	wc_ahb_div_sel		200	NB	DIV2[2:0]	NA
		0	NA	atb_clk_div_sel		800	NB	DIV2[26:5]	NA
		1	NA	plkdbg_clk_div_sel	400	NB	DIV2[30:29]	NA
-------------------------------------------------------------------------------------------------------------------------------------
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-B_S 1000	5	1	sec_at_pclk_sel		200	NB	DIV1[5:3]	DIV1[2:0]
		4	2	ddr_fclk_pclk_sel	125	NB	DIV0[17:15]	DIV0[14:12]
		5	2	sec_dap_pclk_sel	100	NB	DIV1[11:9]	DIV1[8:6]
		5	2	pwm_pclk_sel		100	NB	DIV0[5:3]	DIV0[2:0]
		4	1	gbe_core_pclk_sel	250	SB					DIV1[20:18]	DIV1[23:21]
		1	NA	gbe_bm_core_clk_div2_sel250	SB					DIV1[12]	NA
		1	NA	gbe1_core_clk_div2_sel	250	SB					DIV1[13]	NA
		1	NA	gbe0_core_clk_div2_sel	250	SB					DIV1[14]	NA
		4	2	gbe_125_pclk_sel	125	SB					DIV1[11:9]	DIV1[8:6]
		2	2	sb_axi_pclk_sel		250	SB					DIV0[20:18]	DIV0[23:21]
		4	2	usb32_ss_sys_pclk_sel	125	SB					DIV0[17:15]	DIV0[20:18]
		5	2	usb32_usb2_sys_pclk_sel	100	SB					DIV0[14:12]	DIV0[11:9]
-------------------------------------------------------------------------------------------------------------------------------------
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-B-P 500	2	5	gbe_50_pclk_sel		50	SB					DIV2[11:9]	DIV2[8:6]
*************************************************************************************************************************************

*************************************************************************************************************************************
Configuration 2	CPU 1000, DDR 800

		SE DIV	DIFF DIV
KVCO-A	1600	0	1
KVCO-B	2000	1	2
=====================================================================================================================================
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-A-S 1600	4	1	mmc_pclk_sel		400	NB	DIV2[15:13]	DIV2[18:16]
		1	NA	ddr_phy_mck_div_sel	800	NB	DIV0[18]	NA
		4	1	sdio_pclk_sel		400	SB					DIV0[8:6]	DIV0[5:3]
-------------------------------------------------------------------------------------------------------------------------------------
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-A-P 800	5	NA	counter_pclk_sel	160	NB	DIV0[25:23]	NA
		1	NA	cpu_cnt_clk_div_sel	80	NB	DIV2[28:27]	NA
		4	1	sqf_pclk_sel		200	NB	DIV1[26:24]	DIV1[29:27]
		4	1	sata_host_pclk_sel	200	NB	DIV2[9:7]	DIV2[12:10]
		4	1	tsecm_pclk_sel		200	NB	DIV1[14:12]	DIV1[17:15]
-------------------------------------------------------------------------------------------------------------------------------------
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-B_S 1000	5	1	sec_at_pclk_sel		200	NB	DIV1[5:3]	DIV1[2:0]
		2	1	eip97_pclk_sel		500	NB	DIV2[24:22]	DIV2[21:19]
		1	NA	setm_tmx_pclk_sel	1000	NB	DIV1[20:18]	NA
		1	NA	trace_pclk_sel		1000	NB	DIV0[22:20]	NA
		1	NA	a53_cpu_pclk_sel	1000	NB	DIV0[30:28]	NA
		4	NA	wc_ahb_div_sel		250	NB	DIV2[2:0]	NA
		0	NA	atb_clk_div_sel		1000	NB	DIV2[26:5]	NA
		1	NA	plkdbg_clk_div_sel	500	NB	DIV2[30:29]	NA
		4	2	ddr_fclk_pclk_sel	125	NB	DIV0[17:15]	DIV0[14:12]
		5	2	sec_dap_pclk_sel	100	NB	DIV1[11:9]	DIV1[8:6]
		5	2	pwm_pclk_sel		100	NB	DIV0[5:3]	DIV0[2:0]
		4	1	gbe_core_pclk_sel	250	SB					DIV1[20:18]	DIV1[23:21]
		1	NA	gbe_bm_core_clk_div2_sel250	SB					DIV1[12]	NA
		1	NA	gbe1_core_clk_div2_sel	250	SB					DIV1[13]	NA
		1	NA	gbe0_core_clk_div2_sel	250	SB					DIV1[14]	NA
		4	2	gbe_125_pclk_sel	125	SB					DIV1[11:9]	DIV1[8:6]
		2	2	sb_axi_pclk_sel		250	SB					DIV0[20:18]	DIV0[23:21]
		4	2	usb32_ss_sys_pclk_sel	125	SB					DIV0[17:15]	DIV0[20:18]
		5	2	usb32_usb2_sys_pclk_sel	100	SB					DIV0[14:12]	DIV0[11:9]
-------------------------------------------------------------------------------------------------------------------------------------
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-B-P 500	2	5	gbe_50_pclk_sel		50	SB					DIV2[11:9]	DIV2[8:6]
*************************************************************************************************************************************
*************************************************************************************************************************************
Configuration 3 	CPU 1200, DDR 750

		SE DIV	DIFF DIV
KVCO-A	1500	0	1
KVCO-B	2400	1	1
=====================================================================================================================================
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-A-S 1500	4	3	ddr_fclk_pclk_sel	125	NB	DIV0[17:15]	DIV0[14:12]
		1	NA	ddr_phy_mck_div_sel	800	NB	DIV0[18]	NA
		3	1	eip97_pclk_sel		400	NB	DIV2[24:22]	DIV2[21:19]
		4	3	gbe_125_pclk_sel	125	SB					DIV1[11:9]	DIV1[8:6]
		3	2	gbe_core_pclk_sel	250	SB					DIV1[20:18]	DIV1[23:21]
		4	3	usb32_ss_sys_pclk_sel	125	SB					DIV0[17:15]	DIV0[20:18]
		5	3	usb32_usb2_sys_pclk_sel	100	SB					DIV0[14:12]	DIV0[11:9]
		2	2	sb_axi_pclk_sel		250	SB					DIV0[20:18]	DIV0[23:21]
-------------------------------------------------------------------------------------------------------------------------------------
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-A-P 750	5	NA	counter_pclk_sel	160	NB	DIV0[25:23]	NA
		1	NA	cpu_cnt_clk_div_sel	80	NB	DIV2[28:27]	NA
		1	NA	trace_pclk_sel		800	NB	DIV0[22:20]	NA
		4	NA	wc_ahb_div_sel		200	NB	DIV2[2:0]	NA
		0	NA	atb_clk_div_sel		800	NB	DIV2[26:5]	NA
		1	NA	plkdbg_clk_div_sel	400	NB	DIV2[30:29]	NA
-------------------------------------------------------------------------------------------------------------------------------------
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-B_S 1200	3	1	mmc_pclk_sel		400	NB	DIV2[15:13]	DIV2[18:16]
		3	1	sdio_pclk_sel		400	SB					DIV0[8:6]	DIV0[5:3]
		6	1	sata_host_pclk_sel	200200	NB	DIV2[9:7]	DIV2[12:10]
		6	1	sec_at_pclk_sel		200	NB	DIV1[5:3]	DIV1[2:0]
		6	1	sqf_pclk_sel		200	NB	DIV1[26:24]	DIV1[29:27]
		6	1	tsecm_pclk_sel		200	NB	DIV1[14:12]	DIV1[17:15]
		1	NA	a53_cpu_pclk_sel	1200	NB	DIV0[30:28]	NA
		4	3	sec_dap_pclk_sel	100	NB	DIV1[11:9]	DIV1[8:6]
		5	2	pwm_pclk_sel		100	NB	DIV0[5:3]	DIV0[2:0]
		1	NA	setm_tmx_pclk_sel	1200	NB	DIV1[20:18]	NA
		1	NA	gbe_bm_core_clk_div2_sel250	SB					DIV1[12]	NA
		1	NA	gbe1_core_clk_div2_sel	250	SB					DIV1[13]	NA
		1	NA	gbe0_core_clk_div2_sel	250	SB					DIV1[14]	NA
-------------------------------------------------------------------------------------------------------------------------------------
		PRE-1	PRE-2	TARGET			FREQ	BR	PRE-1		PRE-2		PRE-1		PRE-2
-------------------------------------------------------------------------------------------------------------------------------------
TBG-B-P 1500	6	4	gbe_50_pclk_sel		50	SB					DIV2[11:9]	DIV2[8:6]
*************************************************************************************************************************************
*/

enum clock_src {
       TBG_A,
       TBG_B
};

enum clock_line {
       TBG_A_P = 0,
       TBG_B_P = 1,
       TBG_A_S = 2,
       TBG_B_S = 3
};

/*
 * North Bridge Clocks
 */
/* North Bridge clock line selectors (0x13000) */
struct nb_clock_sel {
	enum clock_line mmc_pclk_sel;		/* [1:0] */
	enum clock_line sata_host_pclk_sel;	/* [3:2] */
	enum clock_line sec_at_pclk_sel;	/* [5:4] */
	enum clock_line sec_dap_pclk_sel;	/* [7:6] */
	enum clock_line tsecm_pclk_sel;		/* [9:8] */
	enum clock_line setm_tmx_pclk_sel;	/* [11:10] */
	enum clock_line sqf_pclk_sel;		/* [13:12] */
	enum clock_line pwm_pclk_sel;		/* [15:14] */
	enum clock_line ddr_fclk_pclk_sel;	/* [17:16] */
	enum clock_line trace_pclk_sel;		/* [19:18] */
	enum clock_line counter_pclk_sel;	/* [21:20] */
	enum clock_line a53_cpu_pclk_sel;	/* [23:22] */
	enum clock_line eip97_pclk_sel;		/* [25:24] */
};

/* North Bridge clock divider 0 (0x13004) */
struct nb_clock_div_0 {
	u8 pwm_clk_prscl2;	/* [2:0] */
	u8 pwm_clk_prscl1;	/* [5:3] */
	u8 ddr_fclk_clk_prscl2;	/* [14:12] */
	u8 ddr_fclk_clk_prscl1;	/* [17:15] */
	u8 ddr_phy_mck_div_sel;	/* [18] */
	u8 trace_div_sel;	/* [22:20] */
	u8 counter_clk_prscl;	/* [25:23] */
	u8 a53_cpu_clk_prscl;	/* [30:28] */
};

/* North Bridge clock divider 1 (0x13008) */
struct nb_clock_div_1 {
	u8 sec_at_clk_prscl2;	/* [2:0] */
	u8 sec_at_clk_prscl1;	/* [5:3] */
	u8 sec_dap_clk_prscl2;	/* [8:6] */
	u8 sec_dap_clk_prscl1;	/* [11:9] */
	u8 tsecm_clk_prscl2;	/* [14:12] */
	u8 tsecm_clk_prscl1;	/* [17:15] */
	u8 setm_tmx_clk_prscl;	/* [20:18] */
	u8 sqf_clk_prscl2;	/* [26:24] */
	u8 sqf_clk_prscl1;	/* [29:27] */
};

/* North Bridge clock divider 2 (0x1300C) */
struct nb_clock_div_2 {
	u8 wc_ahb_div_sel;	/* [2:0] */
	u8 sata_h_clk_prscl2;	/* [9:7] */
	u8 sata_h_clk_prscl1;	/* [12:10] */
	u8 mmc_clk_prscl2;	/* [15:13] */
	u8 mmc_clk_prscl1;	/* [18:16] */
	u8 eip97_clk_prscl2;	/* [21:19] */
	u8 eip97_clk_prscl1;	/* [24:22] */
	u8 atb_clk_div_sel;	/* [26:25] */
	u8 cpu_cnt_clk_div_sel;	/* [28:27] */
	u8 plkdbg_clk_div_sel;	/* [30:29] */
};

/* North Bridge clock configuration */
struct nb_clock_cfg {
	struct nb_clock_sel	clock_sel;
	struct nb_clock_div_0	div0;
	struct nb_clock_div_1	div1;
	struct nb_clock_div_2	div2;
};

/*
 * South Bridge Clocks
 */
/* South Bridge clock line selectors (0x18000) */
struct sb_clock_sel {
	enum clock_line gbe_50_pclk_sel;		/* [7:6] */
	enum clock_line gbe_core_pclk_sel;		/* [9:8] */
	enum clock_line gbe_125_pclk_sel;		/* [11:10] */
	enum clock_line sdio_pclk_sel;			/* [15:14] */
	enum clock_line usb32_usb2_sys_pclk_sel;	/* [17:16] */
	enum clock_line usb32_ss_sys_pclk_sel;		/* [19:18] */
	enum clock_line sb_axi_pclk_sel;		/* [21:20] */
};

/* South Bridge clock divider 0 (0x18004) */
struct sb_clock_div_0 {
	u8 sdio_clk_prscl2;		/* [5:3] */
	u8 sdio_clk_prscl1;		/* [8:6] */
	u8 usb32_usb2_sys_clk_prscl2;	/* [11:9] */
	u8 usb32_usb2_sys_clk_prscl1;	/* [14:12] */
	u8 usb32_ss_sys_clk_prscl2;	/* [17:15] */
	u8 usb32_ss_sys_clk_prscl1;	/* [20:18] */
	u8 sb_axi_clk_prscl2;		/* [23:21] */
	u8 sb_axi_clk_prscl1;		/* [24:24] */
};

/* South Bridge clock divider 1 (0x18008) */
struct sb_clock_div_1 {
	u8 gbe_125_clk_prscl2;		/* [8:6] */
	u8 gbe_125_clk_prscl1;		/* [11:9] */
	u8 gbe_bm_core_clk_div2_sel;	/* [12] */
	u8 gbe1_core_clk_div2_sel;	/* [13] */
	u8 gbe0_core_clk_div2_sel;	/* [14] */
	u8 gbe_core_clk_prscl2;		/* [20:18] */
	u8 gbe_core_clk_prscl1;		/* [23:21] */
};

/* South Bridge clock divider 2 (0x1800C) */
struct sb_clock_div_2 {
	u8 gbe_50_clk_prscl2;		/* [8:6] */
	u8 gbe_50_clk_prscl1;		/* [11:9] */
};

/* South Bridge clock configuration */
struct sb_clock_cfg {
	struct sb_clock_sel	clock_sel;
	struct sb_clock_div_0	div0;
	struct sb_clock_div_1	div1;
	struct sb_clock_div_2	div2;
};

struct tbg_cfg {
	u32	kvco_mhz;
	u32	se_vcodiv;
	u32	diff_vcodiv;
};

/* System-wide clock configuration */
struct clock_cfg {
	u32			cpu_freq_mhz;
	u32			ddr_freq_mhz;
	struct tbg_cfg		tbg_a;
	struct tbg_cfg		tbg_b;
	struct nb_clock_cfg	nb_clk_cfg;
	struct sb_clock_cfg	sb_clk_cfg;
};

enum ssc_mode {
	CENTER_SPREAD = 0,
	DOWN_SPREAD
};

enum ssc_offset_mode {
	freq_down = 0,
	freq_up
};

struct ssc_cfg {
	enum ssc_mode		mode;
	u32			mod_freq; /* hz */
	u32			amp_freq; /* hz */
	u32			offset;
	enum ssc_offset_mode	offset_mode;
};

/* Init values for the static clock configurations array */
static struct clock_cfg clk_cfg_all[] = {\
	 /* Clock tree configuration entry 0 (testing, not yet fully functional) */\
	{ 600, 600, /* CPU, DDR */\
		{2400, 1, 1}, /* TBG-A: KVCO Freq, SE_VCODIV, DIFF_VCODIV */\
		{2000, 1, 1}, /* TBG-B: KVCO Freq, SE_VCODIV, DIFF_VCODIV */\
		/* NorthBridge */\
		{{TBG_A_S, TBG_B_S, TBG_B_S, TBG_B_S, TBG_A_P, TBG_A_P, TBG_A_P,\
		  TBG_B_S, TBG_B_S, TBG_A_P, TBG_B_S, TBG_A_P, TBG_B_S},\
		 {4, 5, 2, 4, 1, 2, 6, 2},		/* DIV0 */\
		 {1, 5, 2, 5, 2, 3, 2, 2, 3},		/* DIV1 */\
		 {4, 1, 5, 1, 3, 1, 2, 0, 0, 1} },	/* DIV2 */\
		/* SouthBridge */\
		{{TBG_B_P, TBG_B_S, TBG_B_S, TBG_A_S, TBG_B_S, TBG_B_S, TBG_B_S},\
		 {1, 3, 2, 5, 2, 4, 1, 5},	/* DIV0 */\
		 {2, 4, 0, 0, 0, 4, 1},		/* DIV1 */\
		 {5, 4} }			/* DIV2 */\
	},\
	/* Clock tree configuration entry 1 (default working setup) */\
	{ 800, 800, /* CPU, DDR */\
		{1600, 0, 1}, /* TBG-A: KVCO Freq, SE_VCODIV, DIFF_VCODIV */\
		{2000, 1, 2}, /* TBG-B: KVCO Freq, SE_VCODIV, DIFF_VCODIV */\
		/* NorthBridge */\
		{{TBG_A_S, TBG_A_P, TBG_B_S, TBG_B_S, TBG_A_P, TBG_A_P, TBG_A_P,\
		  TBG_B_S, TBG_B_S, TBG_A_P, TBG_A_P, TBG_A_P, TBG_B_S},\
		{4, 5, 2, 4, 1, 1, 5, 1},		/* DIV0 */\
		{1, 5, 2, 5, 1, 4, 1, 1, 4},		/* DIV1 */\
		{4, 1, 4, 1, 4, 2, 1, 0, 0, 1} },	/* DIV2 */\
		/* SouthBridge */\
		{{TBG_B_P, TBG_B_S, TBG_B_S, TBG_A_S, TBG_B_S, TBG_B_S, TBG_B_S},\
		{1, 4, 2, 5, 2, 4, 2, 2},	/* DIV0 */\
		{2, 4, 0, 0, 0, 4, 1},		/* DIV1 */\
		{5, 2} }			/* DIV2 */\
	},\
	/* Clock tree configuration entry 2 */\
	{ 1000, 800, /* CPU, DDR */\
		{1600, 0, 1}, /* TBG-A: KVCO Freq, SE_VCODIV, DIFF_VCODIV */\
		{2000, 1, 2}, /* TBG-B: KVCO Freq, SE_VCODIV, DIFF_VCODIV */\
		/* NorthBridge */\
		{{TBG_A_S, TBG_A_P, TBG_B_S, TBG_B_S, TBG_A_P, TBG_B_S, TBG_A_P,\
		  TBG_B_S, TBG_B_S, TBG_B_S, TBG_A_P, TBG_B_S, TBG_B_S},\
		{4, 5, 2, 4, 1, 1, 5, 1},		/* DIV0 */\
		{1, 5, 2, 5, 1, 4, 1, 1, 4},		/* DIV1 */\
		{4, 1, 4, 1, 4, 2, 1, 0, 0, 1} },	/* DIV2 */\
		/* SouthBridge */\
		{{TBG_B_P, TBG_B_S, TBG_B_S, TBG_A_S, TBG_B_S, TBG_B_S, TBG_B_S},\
		{1, 4, 2, 5, 2, 4, 2, 2},	/* DIV0 */\
		{2, 4, 0, 0, 0, 4, 1},		/* DIV1 */\
		{5, 2} }			/* DIV2 */\
	},\
	/* Clock tree configuration entry 3 */\
	{ 1200, 750, /* CPU, DDR */\
		{1500, 0, 1}, /* TBG-A: KVCO Freq, SE_VCODIV, DIFF_VCODIV */\
		{2400, 1, 1}, /* TBG-B: KVCO Freq, SE_VCODIV, DIFF_VCODIV */\
		/* NorthBridge */\
		{{TBG_B_S, TBG_B_S, TBG_B_S, TBG_B_S, TBG_B_S, TBG_B_S, TBG_B_S,\
		  TBG_B_S, TBG_A_S, TBG_B_S, TBG_A_P, TBG_B_S, TBG_A_S},\
		{6, 4, 3, 4, 1, 1, 5, 1},		/* DIV0 */\
		{1, 6, 3, 4, 1, 6, 1, 1, 6},		/* DIV1 */\
		{4, 1, 6, 1, 3, 1, 3, 0, 0, 1} },	/* DIV2 */\
		/* SouthBridge */\
		{{TBG_B_P, TBG_A_S, TBG_A_S, TBG_B_S, TBG_A_S, TBG_A_S, TBG_A_S},\
		{1, 3, 3, 5, 3, 4, 2, 3},	/* DIV0 */\
		{3, 4, 0, 0, 0, 2, 3},		/* DIV1 */\
		{4, 6} }			/* DIV2 */\
	},\
};
static struct clock_cfg *clk_cfg;
static u32 preset_flag;

/* check whether the TARGET clock comes from TBG-A or not */
#define CLOCK_SOURCE_FROM_TBG_A(TARGET_CLOCK) ((TARGET_CLOCK == TBG_A_P) || (TARGET_CLOCK == TBG_A_S))

/*****************************************************************************
 * set_clock_preset
 *
 * return: 0: success; -1: invalid parameters
 *****************************************************************************/
int set_clock_preset(enum clk_preset idx)
{
	if (idx < 0 || idx >= CLK_PRESET_MAX) {
		preset_flag = 0;
		return -1;
	}

	clk_cfg = &clk_cfg_all[idx];
	preset_flag = 1;
	return 0;
}

/******************************************************************************
 * Name: get_tbg_vco_sel
 *
 * Description:	calculate the vco parameters for TBG based on the target clock.
 *
 * Input:	kvco_mhz - KVCO clock freq in MHz
 *
 * Output:	vco_intpi - Phase Interpolator Bias Current
 *		vco_range - VCO frequency range (0x8 - 0xF)
 *
 * Return: 0 - OK
 *****************************************************************************/
static u32 get_tbg_vco_sel(u32 kvco_mhz,
			   u32 *vco_intpi,
			   u32 *vco_range)
{
	u8  index;
	u16 vco_top_ranges_mhz[8] = {1350, 1500, 1750, 2000, 2200, 2400, 2600, 3000};

	/* Select the range the KVCO fits to.
	   Valid values are 0x8 through 0xF.
	   See TBG_A_KVCO and TBG_B_KVCO fields for details
	 */
	for (index = 0; index < 8; index++) {
		if (kvco_mhz <= vco_top_ranges_mhz[index]) {
			*vco_range = index + 8;
			break;
		}
	}

	/* INTPI - Phase Interpolator Bias Current
	  8 <= KVCO is 2.5 ~ 3.0 GHz,
	  6 <= KVCO is 2.0 ~ 2.5 GHz,
	  5 <= KVCO is 1.5 ~ 2.0 GHz
	 */
	if (kvco_mhz > 2500)
		*vco_intpi = 8;
	else if (kvco_mhz < 2000)
		*vco_intpi = 5;
	else
		*vco_intpi = 6;

	return 0;
}

/******************************************************************************
 * Name: set_tbg_clock
 *
 * Description:	Change the TBG(A/B) clock
 *		All affected North/South bridge clock sources should be
 *		switched to XTAL mode prior to calling this function!
 *
 * Input:	kvco_mhz:     AKVCO clock for TBG-X in MHz
 *		se_vco_div:   single-ended clock VCO divider (TBG_X_S)
 *		diff_vco_div: differential clock VCO post divider (TBG_X_P)
 *		tbg_typ:      TBG type (A/B)
 * Output:	None
 * Return:	0 if OK
 *****************************************************************************/
static u32 set_tbg_clock(u32 kvco_mhz,
			       u32 se_vco_div,
			       u32 diff_vco_div,
			       enum clock_src tbg_typ)
{
	u32 vco_range = 0, vco_intpi = 0;
	u32 tbg_M, tbg_N;
	u32 icp;
	u32 regval;
	int ret;

	/* Try to keep Fref/M as close as possible to 10 MHz */
	if (get_ref_clk() == 40) {
		/* 40MHz */
		tbg_M = 4;	/* 10MHz */
		icp = 5;	/* 8 */
	} else	{
		/* 25MHz */
		tbg_M = 3;	/* 8.33MHz */
		icp = 6;	/* 9 */
	}

	ret = get_tbg_vco_sel(kvco_mhz, &vco_intpi, &vco_range);
	if (ret != 0) {
		printf("Failed to obtain VCO divider selection\n");
		udelay(10);
		return ret;
	}

	/* TBG frequency is calculated according to formula:
		Ftbg = Fkvco / se_vco_div
	   where
		Fkvco = N * Fref * 4 / M

	   N - TBG_A_FBDIV or TBG_B_FBDIV
	   M - TBG_A_REFDIV or TBG_B_REFDIV
	   se_vco_div - TBG_A_VCODIV_SEL_SE or TBG_B_VCODIV_SEL_SE
	   Fref - reference clock 25MHz or 40 MHz
	  */
	tbg_N = (kvco_mhz * tbg_M / get_ref_clk()) >> 2;

	/* 1. Set TBG-A (bit[0]) or TBG-B (bit[16]) to reset state  */
	regval = readl(NB_TBG_CTRL0);
	regval |= (tbg_typ == TBG_A) ? BIT(0) : BIT(16);
	writel(regval, NB_TBG_CTRL0);

	/* 2. Set TBG-A (bit[10:2]) or TBG-B (bit[26:18]) FBDIV (N) value */
	regval = readl(NB_TBG_CTRL0);
	regval &= (tbg_typ == TBG_A) ? ~(0xFFUL << 2) : ~(0xFFUL << 18);
	regval |= (tbg_typ == TBG_A) ? (tbg_N << 2) : (tbg_N << 18);
	writel(regval, NB_TBG_CTRL0);

	/* 3. Set TBG-A (bit[8:0]) or TBG-B (bit[24:16]) REFDIV (M) value */
	regval = readl(NB_TBG_CTRL7);
	regval &= (tbg_typ == TBG_A) ? ~(0x1FFUL << 0) : ~(0x1FFUL << 16);
	regval |= (tbg_typ == TBG_A) ? (tbg_M << 0) : (tbg_M << 16);
	writel(regval, NB_TBG_CTRL7);

	/* 4a. Set TBG-A (bit[8:0]) or TBG-B (bit[24:16]) SE VCODIV value  */
	regval = readl(NB_TBG_CTRL1);
	regval &= (tbg_typ == TBG_A) ? ~(0x1FFUL << 0) : ~(0x1FFUL << 16);
	regval |= (tbg_typ == TBG_A) ? (se_vco_div << 0) : (se_vco_div << 16);
	writel(regval, NB_TBG_CTRL1);

	/* 4b. Set TBG-A (bit[9:1]) or TBG-B (bit[25:17]) DIFF VCODIV value  */
	regval = readl(NB_TBG_CTRL8);
	regval &= (tbg_typ == TBG_A) ? ~(0x1FFUL << 1) : ~(0x1FFUL << 17);
	regval |= (tbg_typ == TBG_A) ? (diff_vco_div << 1) : (diff_vco_div << 17);
	writel(regval, NB_TBG_CTRL8);

	/* 5. Set TBG-A (bit[11]) or TBG-B (bit[27]) PLL Bandwidth to normal (0x0) */
	regval = readl(NB_TBG_CTRL1);
	regval &= (tbg_typ == TBG_A) ? ~(0x1UL << 11) : ~(0x1UL << 27);
	writel(regval, NB_TBG_CTRL1);

	/* 6. Set TBG-A (bit[11:8]) or TBG-B (bit[27:24]) ICP Charge Pump Current value */
	regval = readl(NB_TBG_CTRL2);
	regval &= (tbg_typ == TBG_A) ? ~(0xFUL << 8) : ~(0xFUL << 24);
	regval |= (tbg_typ == TBG_A) ? (icp << 8) : (icp << 24);
	writel(regval, NB_TBG_CTRL2);

	/* 7. Set TBG-A (bit[3:0]) or TBG-B (bit[19:16]) KVCO range value  */
	regval = readl(NB_TBG_CTRL3);
	regval &= (tbg_typ == TBG_A) ? ~(0xFUL << 0) : ~(0xFUL << 16);
	regval |= (tbg_typ == TBG_A) ? (vco_range << 0) : (vco_range << 16);
	writel(regval, NB_TBG_CTRL3);

	/* 8. Set TBG-A (bit[3:0]) or TBG-B (bit[19:16]) INTPI value  */
	regval = readl(NB_TBG_CTRL2);
	regval &= (tbg_typ == TBG_A) ? ~(0xFUL << 0) : ~(0xFUL << 16);
	regval |= (tbg_typ == TBG_A) ? (vco_intpi << 0) : (vco_intpi << 16);
	writel(regval, NB_TBG_CTRL2);

	/* 9. Release TBG-A (bit[0]) or TBG-B (bit[16]) from reset */
	regval = readl(NB_TBG_CTRL0);
	regval &= (tbg_typ == TBG_A) ? ~BIT(0) : ~BIT(16);
	writel(regval, NB_TBG_CTRL0);

	/* 10. Set TBG-A (bit[9]) or TBG-B (bit[25]) Bypass to disable - get CLKOUT from PLL */
	regval = readl(NB_TBG_CTRL1);
	regval &= (tbg_typ == TBG_A) ? ~BIT(9) : ~BIT(25);
	writel(regval, NB_TBG_CTRL1);

	/* 11. Wait for TBG-A (bit[15] and TBG-B (bit[31]) PLL lock */
	do {
		regval = readl(NB_TBG_CTRL1);
		regval &= BIT(15) | BIT(31);
	} while (regval != (BIT(15) | BIT(31)));

	/* Wait for stable clock output, suggested 40us */
	udelay(40);

	return 0;
}

static u32 set_ssc_clock(u32 kvco_mhz,
					u32 se_vco_div,
					enum clock_src tbg_typ,
					struct ssc_cfg *ssc)
{
	u32 regval;
	u32 freq_div, tval;

	/* SSC can not be enabled if kvco < 1500mhz */
	if (kvco_mhz < 1500)
		return -1;

	/* 1. Toggle TBG_X_SSC_RESET_EXT  */
	regval = readl(NB_TBG_CTRL5);
	regval |= (tbg_typ == TBG_A) ? BIT(11) : BIT(27);
	writel(regval, NB_TBG_CTRL5);

	regval &= (tbg_typ == TBG_A) ? ~BIT(11) : ~BIT(27);
	writel(regval, NB_TBG_CTRL5);

	/* 2. Set frequency offset */
	if (ssc->offset) { /* Use frequency offset */
		/* Program TBG_X_FREQ_OFFSET */
		regval = readl(NB_TBG_CTRL4);
		regval &= (tbg_typ == TBG_A) ? ~(0xFFFFUL << 16) : ~0xFFFFUL;
		regval |= (tbg_typ == TBG_A) ? (ssc->offset << 16) : ssc->offset;
		writel(regval, NB_TBG_CTRL4);

		/* Program TBG_X_FREQ_OFFSET_16 */
		regval = readl(NB_TBG_CTRL3);
		regval &= (tbg_typ == TBG_A) ? ~BIT(4) : ~BIT(20);
		regval |= (tbg_typ == TBG_A) ? (ssc->offset_mode << 4) : (ssc->offset_mode << 20);
		writel(regval, NB_TBG_CTRL3);

		/* Toggle TBG_X_FREQ_OFFSET_VALID */
		regval = readl(NB_TBG_CTRL3);
		regval |= (tbg_typ == TBG_A) ? BIT(11) : BIT(27);
		writel(regval, NB_TBG_CTRL3);

		regval &= (tbg_typ == TBG_A) ? ~BIT(11) : ~BIT(27);
		writel(regval, NB_TBG_CTRL3);
	} else { /* Don't use frequency offset */
		regval = readl(NB_TBG_CTRL3);
		regval &= (tbg_typ == TBG_A) ? ~(BIT(11) | BIT(12)) : ~(BIT(27) | BIT(28));
		writel(regval, NB_TBG_CTRL3);
	}

	/* 3. Program TBG_X_SSC_FREQ_DIV */
	if (ssc->mode == DOWN_SPREAD)
		freq_div = (kvco_mhz * 125000 * (1 << se_vco_div)) / ssc->mod_freq;
	else
		freq_div = (kvco_mhz * 62500 * (1 << se_vco_div)) / ssc->mod_freq;

	/* According to the test result, 600Mhz need special settings */
	if (get_ddr_clock() == 600)
		freq_div = 0x2800;

	regval = readl(NB_TBG_CTRL6);
	regval &= (tbg_typ == TBG_A) ? ~0xFFUL : ~(0xFFUL << 16);
	regval |= (tbg_typ == TBG_A) ? freq_div : freq_div << 16;
	writel(regval, NB_TBG_CTRL6);

	/* 4. Program TBG_X_SSC_RNGE to achieve disired SSC amplitude */
	tval = ((((ssc->amp_freq) / 1000000) << 26)/(kvco_mhz * freq_div)) & 0x7FFUL;
	/* According to the test result, 600Mhz need special settings */
	if (get_ddr_clock() == 600)
		tval = 0x3c;
	regval = readl(NB_TBG_CTRL5);
	regval &= (tbg_typ == TBG_A) ? ~0x7FFUL : ~(0x7FFUL << 16);
	regval |= (tbg_typ == TBG_A) ? tval : tval << 16;
	writel(regval, NB_TBG_CTRL5);

	/* 5. Program TBG_X_SSC_MDOE */
	regval = readl(NB_TBG_CTRL5);
	regval &= (tbg_typ == TBG_A) ? ~BIT(12) : ~BIT(28);
	regval |= (tbg_typ == TBG_A) ? (ssc->mode << 12) : (ssc->mode << 28);
	writel(regval, NB_TBG_CTRL5);

	/* 6. (Optional) Program TBG_X_FREQ_OFFSET_EN */

	/* 7. Program TBG_X_ICP, same value with set_tbg_clock() */

	/* 8. Program TBG_X_CTUNE = 0 */
	regval = readl(NB_TBG_CTRL1);
	regval &= (tbg_typ == TBG_A) ? ~(BIT(12) | BIT(13)) : ~(BIT(28) | BIT(29));
	writel(regval, NB_TBG_CTRL1);

	/* 9. Program TBG_X_INTPR = 4 */
	regval = readl(NB_TBG_CTRL2);
	regval &= (tbg_typ == TBG_A) ? ~(BIT(4) | BIT(5) | BIT(6)) : ~(BIT(20) | BIT(21) | BIT(22));
	regval |= (tbg_typ == TBG_A) ? 4 << 4 : 4 << 20;
	writel(regval, NB_TBG_CTRL2);

	/* 10. Program TBG_X_INTPI, same value with set_tbg_clock() */

	/* 11. Set TBG_X_PI_EN */
	regval = readl(NB_TBG_CTRL3);
	regval |= (tbg_typ == TBG_A) ? BIT(5) : BIT(21);
	writel(regval, NB_TBG_CTRL3);

	/* 12. Set TBG_X_SEL_VCO_CLK_EN */
	regval = readl(NB_TBG_CTRL1);
	regval |= (tbg_typ == TBG_A) ? BIT(10) : BIT(26);
	writel(regval, NB_TBG_CTRL1);

	/* 13. Set TBG_X_SSC_CLK_EN */
	regval = readl(NB_TBG_CTRL3);
	regval |= (tbg_typ == TBG_A) ? BIT(10) : BIT(26);
	writel(regval, NB_TBG_CTRL3);

	/* 14. wait 5us */
	udelay(5);

	/* 15. Set TBG_X_PI_LOOP_MODE = 1 */
	regval = readl(NB_TBG_CTRL2);
	regval |= (tbg_typ == TBG_A) ? BIT(13) : BIT(29);
	writel(regval, NB_TBG_CTRL2);

	return 0;
}

/*****************************************************************************
 * get_cpu_clock
 *
 * return: A53 CPU clock in Mhz; -1: fail
 *****************************************************************************/
int get_cpu_clock(void)
{
	if (preset_flag == 0)
		return -1;
	return clk_cfg->cpu_freq_mhz;
}

/*****************************************************************************
 * get_ddr_clock
 *
 * return: A53 DDR clock in Mhz; -1: fail
 *****************************************************************************/
int get_ddr_clock(void)
{
	if (preset_flag == 0)
		return -1;
	return clk_cfg->ddr_freq_mhz;
}

/*****************************************************************************
 * setup_clock_tree
 *
 * return: 0: success; -1: fail
 *****************************************************************************/
int setup_clock_tree(void)
{
	u32 rval, reg_val;
	struct ssc_cfg ssc;

	if (preset_flag == 0)
		return -1;

	/* Switch all North/South Bridge clock sources to XTAL
	   prior to make any change to the clock configuration */
	writel(0x00000000, NB_CLK_SEL);
	writel(0x00000000, SB_CLK_SEL);

	/* set TGB-A clock frequency */
	rval = set_tbg_clock(clk_cfg->tbg_a.kvco_mhz,
			     clk_cfg->tbg_a.se_vcodiv,
			     clk_cfg->tbg_a.diff_vcodiv,
			     TBG_A);
	if (rval) {
		printf("Failed to set TBG-A clock to %dMHz\n",
		       clk_cfg->tbg_a.kvco_mhz);
		return rval;
	}

	/* Disable SSC for TBG-A (bit[10]) and TBG-B (bit[26]) */
	reg_val = readl(NB_TBG_CTRL3);
	reg_val &= ~(BIT(10) | BIT(26));
	writel(reg_val, NB_TBG_CTRL3);

	/* Don't set TBG-A SSC clock if anyone of gbe clocks comes from TBG_A */
	if (!CLOCK_SOURCE_FROM_TBG_A(clk_cfg->sb_clk_cfg.clock_sel.gbe_125_pclk_sel) &&
		!CLOCK_SOURCE_FROM_TBG_A(clk_cfg->sb_clk_cfg.clock_sel.gbe_50_pclk_sel) &&
		!CLOCK_SOURCE_FROM_TBG_A(clk_cfg->sb_clk_cfg.clock_sel.gbe_core_pclk_sel)) {
		ssc.mode = DOWN_SPREAD;
		ssc.mod_freq = SSC_MODULATION_FREQ;
		ssc.amp_freq = SSC_AMPLITUDE_FREQ;
		ssc.offset = 0;
		rval = set_ssc_clock(clk_cfg->tbg_a.kvco_mhz,
				     clk_cfg->tbg_a.se_vcodiv,
				     TBG_A,
				     &ssc);
	}

	/* set TGB-B clock frequency */
	rval = set_tbg_clock(clk_cfg->tbg_b.kvco_mhz,
			     clk_cfg->tbg_b.se_vcodiv,
			     clk_cfg->tbg_b.diff_vcodiv,
			     TBG_B);
	if (rval) {
		printf("Failed to set TBG-B clock to %dMHz\n",
		       clk_cfg->tbg_b.kvco_mhz);
		return rval;
	}

	/* North Bridge clock tree configuration */
	/* Enable all clocks */
	writel(NB_ALL_CLK_ENABLE, NB_CLK_EN);

	/* Setup dividers */
	reg_val = readl(NB_DIV_SEL0);
	reg_val &= NB_DIV_SEL0_MASK;
	reg_val |= clk_cfg->nb_clk_cfg.div0.pwm_clk_prscl2 & 0x7;
	reg_val |= (clk_cfg->nb_clk_cfg.div0.pwm_clk_prscl1 & 0x7) << 3;
	reg_val |= (clk_cfg->nb_clk_cfg.div0.ddr_fclk_clk_prscl2 & 0x7) << 12;
	reg_val |= (clk_cfg->nb_clk_cfg.div0.ddr_fclk_clk_prscl1 & 0x7) << 15;
	reg_val |= (clk_cfg->nb_clk_cfg.div0.ddr_phy_mck_div_sel & 0x1) << 18;
	reg_val |= (clk_cfg->nb_clk_cfg.div0.trace_div_sel & 0x7) << 20;
	reg_val |= (clk_cfg->nb_clk_cfg.div0.counter_clk_prscl & 0x7) << 23;
	reg_val |= (clk_cfg->nb_clk_cfg.div0.a53_cpu_clk_prscl & 0x7) << 28;
	writel(reg_val, NB_DIV_SEL0);

	reg_val = readl(NB_DIV_SEL1);
	reg_val &= NB_DIV_SEL1_MASK;
	reg_val |= clk_cfg->nb_clk_cfg.div1.sec_at_clk_prscl2 & 0x7;
	reg_val |= (clk_cfg->nb_clk_cfg.div1.sec_at_clk_prscl1 & 0x7) << 3;
	reg_val |= (clk_cfg->nb_clk_cfg.div1.sec_dap_clk_prscl2 & 0x7) << 6;
	reg_val |= (clk_cfg->nb_clk_cfg.div1.sec_dap_clk_prscl1 & 0x7) << 9;
	reg_val |= (clk_cfg->nb_clk_cfg.div1.tsecm_clk_prscl2 & 0x7) << 12;
	reg_val |= (clk_cfg->nb_clk_cfg.div1.tsecm_clk_prscl1 & 0x7) << 15;
	reg_val |= (clk_cfg->nb_clk_cfg.div1.setm_tmx_clk_prscl & 0x7) << 18;
	reg_val |= (clk_cfg->nb_clk_cfg.div1.sqf_clk_prscl2 & 0x7) << 24;
	reg_val |= (clk_cfg->nb_clk_cfg.div1.sqf_clk_prscl1 & 0x7) << 27;
	writel(reg_val, NB_DIV_SEL1);

	reg_val = readl(NB_DIV_SEL2);
	reg_val &= NB_DIV_SEL2_MASK;
	reg_val |= clk_cfg->nb_clk_cfg.div2.wc_ahb_div_sel & 0x7;
	reg_val |= (clk_cfg->nb_clk_cfg.div2.sata_h_clk_prscl2 & 0x7) << 7;
	reg_val |= (clk_cfg->nb_clk_cfg.div2.sata_h_clk_prscl1 & 0x7) << 10;
	reg_val |= (clk_cfg->nb_clk_cfg.div2.mmc_clk_prscl2 & 0x7) << 13;
	reg_val |= (clk_cfg->nb_clk_cfg.div2.mmc_clk_prscl1 & 0x7) << 16;
	reg_val |= (clk_cfg->nb_clk_cfg.div2.eip97_clk_prscl2 & 0x7) << 19;
	reg_val |= (clk_cfg->nb_clk_cfg.div2.eip97_clk_prscl1 & 0x7) << 22;
	reg_val |= (clk_cfg->nb_clk_cfg.div2.atb_clk_div_sel & 0x3) << 25;
	reg_val |= (clk_cfg->nb_clk_cfg.div2.plkdbg_clk_div_sel & 0x3) << 29;
	writel(reg_val, NB_DIV_SEL2);

	/* Setup connection to clock lines */
	reg_val = readl(NB_TBG_SEL);
	reg_val &= NB_TBG_SEL_MASK;
	reg_val |= clk_cfg->nb_clk_cfg.clock_sel.mmc_pclk_sel & 0x3;
	reg_val |= (clk_cfg->nb_clk_cfg.clock_sel.sata_host_pclk_sel & 0x3) << 2;
	reg_val |= (clk_cfg->nb_clk_cfg.clock_sel.sec_at_pclk_sel & 0x3) << 4;
	reg_val |= (clk_cfg->nb_clk_cfg.clock_sel.sec_dap_pclk_sel & 0x3) << 6;
	reg_val |= (clk_cfg->nb_clk_cfg.clock_sel.tsecm_pclk_sel & 0x3) << 8;
	reg_val |= (clk_cfg->nb_clk_cfg.clock_sel.setm_tmx_pclk_sel & 0x3) << 10;
	reg_val |= (clk_cfg->nb_clk_cfg.clock_sel.sqf_pclk_sel & 0x3) << 12;
	reg_val |= (clk_cfg->nb_clk_cfg.clock_sel.pwm_pclk_sel & 0x3) << 14;
	reg_val |= (clk_cfg->nb_clk_cfg.clock_sel.ddr_fclk_pclk_sel & 0x3) << 16;
	reg_val |= (clk_cfg->nb_clk_cfg.clock_sel.trace_pclk_sel & 0x3) << 18;
	reg_val |= (clk_cfg->nb_clk_cfg.clock_sel.counter_pclk_sel & 0x3) << 20;
	reg_val |= (clk_cfg->nb_clk_cfg.clock_sel.a53_cpu_pclk_sel & 0x3) << 22;
	reg_val |= (clk_cfg->nb_clk_cfg.clock_sel.eip97_pclk_sel & 0x3) << 24;
	writel(reg_val, NB_TBG_SEL);

	/* South Bridge clock tree configuration */
	/* Enable all clocks */
	writel(SB_ALL_CLK_ENABLE, SB_CLK_EN);

	/* Setup dividers */
	reg_val = readl(SB_DIV_SEL0);
	reg_val &= SB_DIV_SEL0_MASK;
	reg_val |= (clk_cfg->sb_clk_cfg.div0.sdio_clk_prscl2 & 0x7) << 3;
	reg_val |= (clk_cfg->sb_clk_cfg.div0.sdio_clk_prscl1 & 0x7) << 6;
	reg_val |= (clk_cfg->sb_clk_cfg.div0.usb32_usb2_sys_clk_prscl2 & 0x7) << 9;
	reg_val |= (clk_cfg->sb_clk_cfg.div0.usb32_usb2_sys_clk_prscl1 & 0x7) << 12;
	reg_val |= (clk_cfg->sb_clk_cfg.div0.usb32_ss_sys_clk_prscl2 & 0x7) << 15;
	reg_val |= (clk_cfg->sb_clk_cfg.div0.usb32_ss_sys_clk_prscl1 & 0x7) << 18;
	reg_val |= (clk_cfg->sb_clk_cfg.div0.sb_axi_clk_prscl2 & 0x7) << 21;
	reg_val |= (clk_cfg->sb_clk_cfg.div0.sb_axi_clk_prscl1 & 0x7) << 24;
	writel(reg_val, SB_DIV_SEL0);

	reg_val = readl(SB_DIV_SEL1);
	reg_val &= SB_DIV_SEL1_MASK;
	reg_val |= (clk_cfg->sb_clk_cfg.div1.gbe_125_clk_prscl2 & 0x7) << 6;
	reg_val |= (clk_cfg->sb_clk_cfg.div1.gbe_125_clk_prscl1 & 0x7) << 9;
	reg_val |= (clk_cfg->sb_clk_cfg.div1.gbe_bm_core_clk_div2_sel & 0x1) << 12;
	reg_val |= (clk_cfg->sb_clk_cfg.div1.gbe1_core_clk_div2_sel & 0x1) << 13;
	reg_val |= (clk_cfg->sb_clk_cfg.div1.gbe0_core_clk_div2_sel & 0x1) << 14;
	reg_val |= (clk_cfg->sb_clk_cfg.div1.gbe_core_clk_prscl2 & 0x7) << 18;
	reg_val |= (clk_cfg->sb_clk_cfg.div1.gbe_core_clk_prscl1 & 0x7) << 21;
	writel(reg_val, SB_DIV_SEL1);

	reg_val = readl(SB_DIV_SEL2);
	reg_val &= SB_DIV_SEL2_MASK;
	reg_val |= (clk_cfg->sb_clk_cfg.div2.gbe_50_clk_prscl2 & 0x7) << 6;
	reg_val |= (clk_cfg->sb_clk_cfg.div2.gbe_50_clk_prscl1 & 0x7) << 9;
	writel(reg_val, SB_DIV_SEL2);

	/* Setup connection to clock lines */
	reg_val = readl(SB_TBG_SEL);
	reg_val &= SB_TBG_SEL_MASK;
	reg_val |= (clk_cfg->sb_clk_cfg.clock_sel.gbe_50_pclk_sel & 0x3) << 6;
	reg_val |= (clk_cfg->sb_clk_cfg.clock_sel.gbe_core_pclk_sel & 0x3) << 8;
	reg_val |= (clk_cfg->sb_clk_cfg.clock_sel.gbe_125_pclk_sel & 0x3) << 10;
	reg_val |= (clk_cfg->sb_clk_cfg.clock_sel.sdio_pclk_sel & 0x3) << 14;
	reg_val |= (clk_cfg->sb_clk_cfg.clock_sel.usb32_usb2_sys_pclk_sel & 0x3) << 16;
	reg_val |= (clk_cfg->sb_clk_cfg.clock_sel.usb32_ss_sys_pclk_sel & 0x3) << 18;
	reg_val |= (clk_cfg->sb_clk_cfg.clock_sel.sb_axi_pclk_sel & 0x3) << 20;
	writel(reg_val, SB_TBG_SEL);

	/*
	 * Switch all North/South Bridge clock sources from XTAL to clock
	 * divider excepting counter clock, which remains to be connected
	 * to XTAL
	 */
	writel(0x00009FFF, NB_CLK_SEL);
	writel(0x000007AA, SB_CLK_SEL);

	return 0;
}

u32 get_ref_clk(void)
{
	if (readl(TEST_PIN_LATCH_N) & XTAL_CLK_40MHZ)
		return 40;
	else
		return 25;
}

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

/*
 * CM3 Secure Processor runs at 200 MHz at every setting.
 * It could theoretically run at other frequency, but since this firmware
 * does not change it's clock, we are goint to assume constant frequency
 * of 200 MHz.
 *
 * The assembly loop in ndelay() and udelay() functions takes 3 clock ticks.
 * At 200 MHz, 3 clock ticks amount to 15 ns. That is why NS_TO_LOOPS()
 * divides the desired wait delay by 15.
 * US_TO_LOOPS(us) = NS_TO_LOOPS(1000 * us) = 1000 * us / 15 = 200 * us / 3.
 */
#define NS_TO_LOOPS(ns)		((ns) / 15)
#define US_TO_LOOPS(us)		(200 * (us) / 3)

void ndelay(u32 ns)
{
	u32 loop = NS_TO_LOOPS(ns);

	if (!ns)
		return;

	asm volatile(
		"0:\n"
		"subs %[count], 1\n"
		"bne 0b\n"
		: [count] "+r" (loop)
	);
}

void udelay(u32 us)
{
	u32 loop = US_TO_LOOPS(us);

	if (!us)
		return;

	asm volatile(
		"0:\n"
		"subs %[count], 1\n"
		"bne 0b\n"
		: [count] "+r" (loop)
	);
}

DECL_DEBUG_CMD(cmd_ndelay)
{
	u32 ns, start, stop;

	if (argc < 2)
		goto usage;

	if (decnumber(argv[1], &ns))
		goto usage;

	start = readl(0xc0008324);
	ndelay(ns);
	stop = readl(0xc0008324);
	printf("diff = %u us\n", start - stop);

	return;
usage:
	printf("usage: ndelay <ns>\n");
}

DEBUG_CMD("ndelay", "Delay in nanoseconds", cmd_ndelay);

DECL_DEBUG_CMD(cmd_udelay)
{
	u32 us, start, stop;

	if (argc < 2)
		goto usage;

	if (decnumber(argv[1], &us))
		goto usage;

	start = readl(0xc0008324);
	udelay(us);
	stop = readl(0xc0008324);
	printf("diff = %u us\n", start - stop);

	return;
usage:
	printf("usage: udelay <us>\n");
}

DEBUG_CMD("udelay", "Delay in microseconds", cmd_udelay);

DECL_DEBUG_CMD(cmd_clk)
{
	printf("xtal\t\t%u MHz\n"
	       "TBG_A_P\t\t%u MHz\n"
	       "TBG_B_P\t\t%u MHz\n"
	       "TBG_A_S\t\t%u MHz\n"
	       "TBG_B_S\t\t%u MHz\n", get_ref_clk(), get_tbg_clk(TBG_A_P),
	       get_tbg_clk(TBG_B_P), get_tbg_clk(TBG_A_S),
	       get_tbg_clk(TBG_B_S));
}

DEBUG_CMD("clk", "Clock information", cmd_clk);

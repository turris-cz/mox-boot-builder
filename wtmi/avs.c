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
#include "avs.h"

static int otp_nb_read_parallel(u32 *data)
{
	u32 regval;

	/* 1. Clear OTP_MODE_NB to parallel mode */
	regval = readl(MVEBU_NORTH_OTP_CTRL);
	regval &= ~OTP_MODE_BIT;
	writel(regval, MVEBU_NORTH_OTP_CTRL);

	/* 2. Set OTP_POR_B_NB enter normal operation */
	regval = readl(MVEBU_NORTH_OTP_CTRL);
	regval |= OTP_POR_B_BIT;
	writel(regval, MVEBU_NORTH_OTP_CTRL);

	/* 3. Set OTP_PTR_INC_NB to auto-increment pointer after each read */
	regval = readl(MVEBU_NORTH_OTP_RD_POINTER);
	regval |= OTP_PTR_INC_BIT;
	writel(regval, MVEBU_NORTH_OTP_RD_POINTER);

	/* 4. Set OTP_RPTR_RST_NB, then clear the same field */
	regval = readl(MVEBU_NORTH_OTP_CTRL);
	regval |= OTP_RPTR_RST_BIT;
	writel(regval, MVEBU_NORTH_OTP_CTRL);

	regval = readl(MVEBU_NORTH_OTP_CTRL);
	regval &= ~OTP_RPTR_RST_BIT;
	writel(regval, MVEBU_NORTH_OTP_CTRL);

	/* 5. Toggle OTP_PRDT_NB
	 * a. Set OTP_PRDT_NB to 1.
	 * b. Clear OTP_PRDT_NB to 0.
	 * c. Wait for a minimum of 100 ns.
	 * d. Set OTP_PRDT_NB to 1
	 */
	regval = readl(MVEBU_NORTH_OTP_CTRL);
	regval |= OTP_PRDT_BIT;
	writel(regval, MVEBU_NORTH_OTP_CTRL);

	regval = readl(MVEBU_NORTH_OTP_CTRL);
	regval &= ~OTP_PRDT_BIT;
	writel(regval, MVEBU_NORTH_OTP_CTRL);

	ndelay(100);

	regval = readl(MVEBU_NORTH_OTP_CTRL);
	regval |= OTP_PRDT_BIT;
	writel(regval, MVEBU_NORTH_OTP_CTRL);

	udelay(100);

	/*6. Read the content of OTP 32-bits at a time */
	data[0] = readl(MVEBU_NORTH_OTP_RD_PORT);
	udelay(100);
	data[1] = readl(MVEBU_NORTH_OTP_RD_PORT);
	udelay(100);
	data[2] = readl(MVEBU_NORTH_OTP_RD_PORT);

	return 0;
}

/***************************************************************************************************
  * init_avs
  * Read AVS settings in OTP, then write the correct value into CPU registers
  * according to the CPU clock
 ***************************************************************************************************/
int init_avs(u32 speed)
{
	u32 otp_data[OTP_DATA_MAX];
	u32 vdd_otp, shift, svc_rev, regval, vdd_default;

	/* Read OTP data */
	if (otp_nb_read_parallel(otp_data))
		return -1;

	regval = readl(MVEBU_AVS_CONTROL0);
	regval &= ~((AVS_VDD_MASK << HIGH_VDD_LIMIT_OFF) |
		    (AVS_VDD_MASK << LOW_VDD_LIMIT_OFF));

	/* Get SVC revision */
	svc_rev = (otp_data[OTP_DATA_SVC_REV_ID] >> OTP_SVC_REV_OFFSET) &
		  OTP_SVC_REV_MASK;

	switch (speed) {
	case OTP_SVC_SPEED_600:
		shift = OTP_SVC_SPEED_600_OFF;
		vdd_default = VAS_600M_DEFAULT_VALUE;
		break;
	case OTP_SVC_SPEED_800:
		shift = OTP_SVC_SPEED_800_OFF;
		vdd_default = VAS_800M_DEFAULT_VALUE;
		break;
	case OTP_SVC_SPEED_1000:
		shift = OTP_SVC_SPEED_1000_OFF;
		vdd_default = VAS_1000M_DEFAULT_VALUE;
		break;
	case OTP_SVC_SPEED_1200:
		shift = OTP_SVC_SPEED_1200_OFF;
		vdd_default = VAS_1200M_DEFAULT_VALUE;
		break;
	default:
		return -1;
	}

	if (svc_rev >= SVC_REVISION_2) {
		vdd_otp = (otp_data[OTP_DATA_SVC_SPEED_ID] >> shift) &
			  AVS_VDD_MASK;
		if (!vdd_otp || vdd_otp + AVS_VDD_BASE > AVS_VDD_MASK) {
			regval |= (vdd_default << HIGH_VDD_LIMIT_OFF);
			regval |= (vdd_default << LOW_VDD_LIMIT_OFF);
		} else {
			vdd_otp += AVS_VDD_BASE;
			regval |= (vdd_otp << HIGH_VDD_LIMIT_OFF);
			regval |= (vdd_otp << LOW_VDD_LIMIT_OFF);
		}
	} else {
		regval |= (vdd_default << HIGH_VDD_LIMIT_OFF);
		regval |= (vdd_default << LOW_VDD_LIMIT_OFF);
	}

	/* Set high&low VDD limit */
	writel(regval, MVEBU_AVS_CONTROL0);
	/* Release AVS reset */
	regval = readl(MVEBU_AVS_CONTROL0);
	regval &= ~(AVS_SOFT_RST_BIT | AVS_ENABLE_BIT | AVS_PAUSE_BIT);
	writel(regval, MVEBU_AVS_CONTROL0);
	/* Enable AVS */
	regval = readl(MVEBU_AVS_CONTROL0);
	regval |= (AVS_ENABLE_BIT | SEL_VSENSE0_BIT);
	writel(regval, MVEBU_AVS_CONTROL0);
	/* Delay 100ms */
	udelay(100000);

	return 0;
}


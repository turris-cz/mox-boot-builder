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

#include "ddr.h"

unsigned int tc_cs_num;

int set_ddr_topology_parameters(struct ddr_topology top_map)
{
	tc_cs_num = top_map.cs_num;
	return 0;
}

int init_ddr(struct ddr_init_para init_para, struct ddr_init_result *result)
{

	unsigned int cs;
	unsigned int zp_zn_trm = 0x0;
	unsigned int orig_log_level = 0;
	unsigned int dll_res;
	int ret_val = 0;
	unsigned int qs_res[MAX_CS_NUM];
	unsigned int disable_rl;

	/* Write patterns at slow speed before going into Self Refresh */
	//TODO: ddr_test_dma(0x1000) - size, base addr - Is it needed if I do self_refresh_test(0)

	/*
	 * The self-refresh test should applied to the code boot only.
	 * In a warm boot, DRAM holds the user data and Linux content
	 * of the previous boot. The test may corrupt the existing
	 * data unexpectly.
	 */
	if (!init_para.warm_boot)
		/*
		 * The CM3's address map to DRAM is supplied, which is
		 * available for memory test. But the boot image is pre-
		 * loaded and located at 0x0041.0000. Avoiding wripping
		 * out the image data, only fill the test pattern to the
		 * first 1KB memory per each chip select.
		 */
		for (cs = 0; cs < tc_cs_num; cs++)
			self_refresh_test(0, init_para.cs_wins[cs].base, 1024);

	/* 1. enter self refresh */
	self_refresh_entry();

	/* 2. setup clock */
	init_para.clock_init();

	/* 3. DDRPHY sync2 and DLL reset */
	phyinit_sequence_sync2(1, 3, 2, 0);

	/* 4. update CL/CWL *nd other timing parameters as per lookup table */
	/* Skip it, use the current settings in register */

	/* 5. enable DDRPHY termination */
	zp_zn_trm = 0xC4477889 & 0x0FF00000;	//PHY_Control_2[0xC0001004]: copied from TIM
	set_clear_trm(1, zp_zn_trm);

	/* Step 6 and 7 - Not needed as per Suresh */
	/* 6. enable DRAM termination , set DRAM DLL */
	//dll_on_dram();
	/* 7. enable DLL on for ddrphy and DLL reset */
	//dll_on_ddrphy();

	/* 8. exit self refresh */
	self_refresh_exit();

	/* 9. do MR command */
	send_mr_commands();

	if (!init_para.warm_boot)
		for (cs=0; cs<tc_cs_num; cs++)
			self_refresh_test(1, init_para.cs_wins[cs].base, 1024);

	if (init_para.warm_boot)
	{
		debug("\nWarm boot is set");

		//qs gate
		writel(result->ddr3.wl_rl_ctl, CH0_PHY_WL_RL_Control);
		writel(result->ddr3.cs0_b0, CH0_PHY_RL_Control_CS0_B0);
		writel(result->ddr3.cs0_b1, CH0_PHY_RL_Control_CS0_B1);
		writel(result->ddr3.cs1_b0, CH0_PHY_RL_Control_CS1_B0);
		writel(result->ddr3.cs1_b1, CH0_PHY_RL_Control_CS1_B1);
		disable_rl = result->ddr3.wl_rl_ctl & (~0x3);
		writel(disable_rl, CH0_PHY_WL_RL_Control);

		//dll tuning
		writel(result->dll_tune.dll_ctrl_b0, CH0_PHY_DLL_control_B0);
		writel(result->dll_tune.dll_ctrl_b1, CH0_PHY_DLL_control_B1);
		writel(result->dll_tune.dll_ctrl_adcm, CH0_PHY_DLL_control_ADCM);

		debug("\nWarm boot completed");
		return 0;
	}
	
	/* QS Gate Training/ Read Leveling - for DDR3 only*/
	//Capture settings from TIM, update only if training passes.
	result->ddr3.wl_rl_ctl = readl(CH0_PHY_WL_RL_Control);
	result->ddr3.cs0_b0 = readl(CH0_PHY_RL_Control_CS0_B0);
	result->ddr3.cs0_b1 = readl(CH0_PHY_RL_Control_CS0_B1);
	result->ddr3.cs1_b0 = readl(CH0_PHY_RL_Control_CS1_B0);
	result->ddr3.cs1_b1 = readl(CH0_PHY_RL_Control_CS1_B1);

	for (cs = 0; cs < tc_cs_num; cs++)
	{
		qs_res[cs] = qs_gating(init_para.cs_wins[cs].base, cs, init_para.log_level, result);
		if (qs_res[cs])		//if qs gate training passed, update res and dump register
		{
			debug("CH0_PHY_RL_Control_CS%d_B0[0x%08X]: 0x%08X\n", cs, (CH0_PHY_RL_Control_CS0_B0 + cs*0x24), readl(CH0_PHY_RL_Control_CS0_B0 + cs*0x24));
			debug("CH0_PHY_RL_Control_CS%d_B1[0x%08X]: 0x%08X\n", cs, (CH0_PHY_RL_Control_CS0_B1 + cs*0x24), readl(CH0_PHY_RL_Control_CS0_B1 + cs*0x24));
		}
		else			//qs gating fails
		{
			debug("\nCS%d: QS GATE TRAINING FAILED\n", cs);
			ret_val = -3;
		}
	}

	/* 10. DLL tuning */
	//enable logs for DLL tuning
	orig_log_level = init_para.log_level;
	init_para.log_level = 1;
	result->dll_tune.dll_ctrl_b0 = readl(CH0_PHY_DLL_control_B0);
	result->dll_tune.dll_ctrl_b1 = readl(CH0_PHY_DLL_control_B1);
	result->dll_tune.dll_ctrl_adcm = readl(CH0_PHY_DLL_control_ADCM);
	dll_res = DLL_tuning(2, tc_cs_num, init_para, 0, 0);	//use long DLL method, mpr mode disabled
	init_para.log_level = orig_log_level;
	if (dll_res)						//training passed
	{
		result->dll_tune.dll_ctrl_b0 = readl(CH0_PHY_DLL_control_B0);
		result->dll_tune.dll_ctrl_b1 = readl(CH0_PHY_DLL_control_B1);
		result->dll_tune.dll_ctrl_adcm = readl(CH0_PHY_DLL_control_ADCM);
	}
	else
	{
		debug("\nDLL TUNING FAILED\n");
		ret_val = -3;
	}

	/* Test DRAM */
	//ddr_test_dma(0x1000); 	//TODO: size, base addr

	return ret_val;
}


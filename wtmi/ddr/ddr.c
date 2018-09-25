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

void mc6_init_timing_selfrefresh(unsigned int speed)
{
	unsigned int wrval = 0, rdval = 0;
	debug("\nRestore CAS Read and Write Latency");
	rdval = readl(CH0_Dram_Config_1);

	if (speed == 800)
		wrval = (rdval & ~(0x00003F3F)) | (0x80B);      //for 800MHz
	else if (speed == 600)
		wrval = (rdval & ~(0x00003F3F)) | (0x80B);      //for 600MHz
	else if (speed == 750)
		wrval = (rdval & ~(0x00003F3F)) | (0x80B);      //for 750MHz

	writel(wrval, CH0_Dram_Config_1);
	//debug("\nCH0_DRAM_Config_1 \t0x%08X\n", readl(CH0_Dram_Config_1));
}

void send_mr_commands(void)
{
	writel(0x13000400, USER_COMMAND_2); //send MRS command to MR2
	writel(0x13000800, USER_COMMAND_2); //send MRS command to MR3
	writel(0x13000200, USER_COMMAND_2); //send MRS command to MR1
	writel(0x13000100, USER_COMMAND_2); //send MRS command to MR0

	wait_ns(10000);
}

void set_clear_trm(int set, unsigned int orig_val)
{
	unsigned int wrval = 0, rdval = 0;
	rdval = readl(CH0_PHY_Control_2);
	if (set)
	{
		debug("\nRestore termination values to original values");
		writel(rdval | orig_val, CH0_PHY_Control_2);
	}
	else
	{
		debug("\nSet termination values to 0");
		wrval = (rdval & ~(0x0FF00000));		//set trm to 0
		writel(wrval, CH0_PHY_Control_2);
	}
}

void self_refresh_entry()
{
	writel(0x13000040, USER_COMMAND_0);   // Enter self-refresh
	debug("\nNow in Self-refresh Mode");
}

void self_refresh_exit()
{
	writel(0x13000080, USER_COMMAND_0);   // Exit self-refresh
	while (readl(DRAM_STATUS) & BIT(2)) {};
	debug("\nExited self-refresh ...\n");
}

void self_refresh_test(int verify, unsigned int base_addr, unsigned int size)
{
	unsigned int end, temp;
	unsigned int *waddr, refresh_error = 0;

	end = base_addr + size;

	if (!verify)
	{
		// Write pattern
		debug("\nFill memory before self refresh...");
		for (waddr = (unsigned int *)base_addr; waddr  < (unsigned int *)end ; waddr++)
		{
			*waddr = (unsigned int)waddr;
		}
		debug("done\n");
	}
	else
	{
		// Check data after exit self refresh
		for (waddr = (unsigned int *)base_addr; waddr  < (unsigned int *)end ; waddr++)
		{
			temp = *waddr;
			if (temp != (unsigned int)waddr)
			{
				debug("\nAt 0x%08x, expect 0x%08x, read back 0x%08x", (unsigned int)waddr, (unsigned int)waddr, temp);
				refresh_error++;
			}
		}
		if (refresh_error)
			debug("\n\nSelf refresh fail !!!!!!!!!!!!!!!!!!!. error cnt = 0x%x", refresh_error);
		else
			debug("\n\nSelf refresh Pass.");

		debug("\nDDR self test mode test done!!");
	}
}

void phyinit_sequence_sync2(volatile unsigned short ld_phase,
		volatile  unsigned short wrst_sel, volatile  unsigned short wckg_dly,
		volatile  unsigned short int wck_en)
{
	// unsigned int tmp= read(0xf1000438);

	// sync2 procedure
	setbitsl(CH0_PHY_Control_6, 1 << 19, 0x00080000);		//MC_SYNC2_EN
	setbitsl(CH0_PHY_Control_6, ld_phase << 9, 0x00000200);//ld_phase update
	setbitsl(PHY_Control_15, wrst_sel, 0x00000003);
	setbitsl(PHY_Control_16, wckg_dly << 4, 0x000000F0);		//set wckg_dly
	setbitsl(PHY_Control_16, wck_en, 0x00000001);		//wck_en

	writel(0x80000000, PHY_CONTROL_9);
	writel(0x20000000, PHY_CONTROL_9);
	writel(0x40000000, PHY_CONTROL_9);
	wait_ns(10000);
	writel(0x80000000, PHY_CONTROL_9);
	wait_ns(10000);
}

#ifndef _DDR_H_
#define _DDR_H_

#include "clock.h"

enum ddr_type {
	DDR3 = 0,
	DDR4,
	DDR_TYPE_MAX,
};

extern void rwtm_win_remap(u32 win, u32 remap_addr);
extern int ddr_main(enum clk_preset WTMI_CLOCK, int DDR_TYPE, int BUS_WIDTH,
		    int SPEED_BIN, int CS_NUM, int DEV_CAP);

#endif /* _DDR_H_ */

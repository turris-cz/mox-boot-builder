#ifndef _DDR_H_
#define _DDR_H_

#include "clock.h"

extern int ddr_main(enum clk_preset WTMI_CLOCK, int BUS_WIDTH, int SPEED_BIN,
		    int CS_NUM, int DEV_CAP);

#endif /* _DDR_H_ */

#ifndef _ENGINE_H_
#define _ENGINE_H_

#include "io.h"
#include "clock.h"

#define ENGINE_CTRL	0x40000400

enum engine {
	Engine_REG = 0,
	Engine_DMA,
	Engine_ABUS,
	Engine_CRYPTO,
	Engine_ECP,
	Engine_HASH,
	Engine_SPAD,
	Engine_ZMODP,
	Engine_MCT,
	Engine_APB,
	Engine_EBG,
};

static inline void engine_reset(enum engine e)
{
	setbitsl(ENGINE_CTRL, BIT(e), BIT(e));
	udelay(10);
	setbitsl(ENGINE_CTRL, 0, BIT(e));
}

static inline void engine_clk_enable(enum engine e)
{
	setbitsl(ENGINE_CTRL, BIT(e + 16), BIT(e + 16));
}

static inline void engine_clk_disable(enum engine e)
{
	setbitsl(ENGINE_CTRL, 0, BIT(e + 16));
}

#endif /* _ENGINE_H_ */

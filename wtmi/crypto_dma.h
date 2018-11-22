#ifndef _CRYPTO_DMA_H_
#define _CRYPTO_DMA_H_

#include "types.h"
#include "io.h"

#define DMA_IN_CTRL		0x40000800
#define DMA_IN_CTRL_ENABLE	BIT(0)
#define DMA_IN_STATUS		0x40000804
#define DMA_IN_SRC		0x40000808
#define DMA_IN_COUNT		0x4000080c
#define DMA_IN_NEXT		0x40000810
#define DMA_IN_INT		0x40000814
#define DMA_IN_INT_MASK		0x40000818
#define DMA_INT_DONE		BIT(0)
#define DMA_INT_BUS_ERR		BIT(1)
#define DMA_INT_LL_ERR		BIT(2)
#define DMA_INT_PAR_ERR		BIT(3)
#define DMA_INT_PAUSE_CMPL	BIT(4)
#define DMA_INT_AXI_PAR_ERR	BIT(5)

static inline void dma_input_enable(const void *data, u32 size)
{
	setbitsl(DMA_IN_INT, DMA_INT_DONE, DMA_INT_DONE);
	writel((u32) data, DMA_IN_SRC);
	writel((size + 3) / 4, DMA_IN_COUNT);
	setbitsl(DMA_IN_CTRL, DMA_IN_CTRL_ENABLE, DMA_IN_CTRL_ENABLE);
}

static inline void dma_input_disable(void)
{
	setbitsl(DMA_IN_CTRL, 0, DMA_IN_CTRL_ENABLE);
}

#endif /* _CRYPTO_DMA_H_ */

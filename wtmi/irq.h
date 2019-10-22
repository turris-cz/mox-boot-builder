#ifndef _IRQ_H_
#define _IRQ_H_

#include "types.h"
#include "io.h"

#define NVIC_SET_ENABLE		0xe000e100
#define NVIC_CLR_ENABLE		0xe000e180
#define NVIC_SET_PENDING	0xe000e200
#define NVIC_CLR_PENDING	0xe000e280
#define NVIC_ACTIVE		0xe000e300
#define NVIC_PRIORITY		0xe000e400
#define SCB_AIRCR		0xe000ed0c

#define __irq			__attribute__((interrupt))

enum {
	IRQn_AP = 0,
	IRQn_USB,
	IRQn_SP,
	IRQn_DMA,
	IRQn_MCT,
	IRQn_BCM_ACC_ERR,
	IRQn_EC_ZMODP,
	IRQn_HASH,
	IRQn_SATA,
	IRQn_CRYPTO,
	IRQn_EMMC_SDIO,
	IRQn_SPAD,
	IRQn_SPI,
	IRQn_UART_RX,
	IRQn_CTITRIGOUT2,
	IRQn_CTITRIGOUT3,
};

typedef void (*irq_handler_t)(int);

extern void register_irq_handler(int irq, irq_handler_t handler);
extern void enable_systick(void);
extern void disable_systick(void);
extern volatile u64 jiffies;

static inline void enable_irq(void)
{
	asm("cpsie i");
}

static inline void disable_irq(void)
{
	asm("cpsid i");
}

static inline void _nvic_set(u32 base, int irq)
{
	u32 reg, addr;

	addr = base + 4 * (irq >> 5);
	reg = readl(addr);
	reg |= 1 << (irq & 0x1f);
	writel(reg, addr);
}

static inline void nvic_enable(int irq)
{
	_nvic_set(NVIC_SET_ENABLE, irq);
}

static inline void nvic_disable(int irq)
{
	_nvic_set(NVIC_CLR_ENABLE, irq);
}

static inline void nvic_set_pending(int irq)
{
	_nvic_set(NVIC_SET_PENDING, irq);
}

static inline void nvic_clr_pending(int irq)
{
	_nvic_set(NVIC_CLR_PENDING, irq);
}

static inline void nvic_set_priority(int irq, u8 priority)
{
	u32 reg, addr;
	int pos;

	priority <<= 1;

	addr = NVIC_PRIORITY + (irq & ~3);
	pos = (irq & 3) << 3;

	reg = readl(addr) & ~(0xff << pos);
	reg |= priority << pos;
	writel(reg, addr);
}

static inline int nvic_is_active(int irq)
{
	return (readl(NVIC_ACTIVE + 4 * (irq >> 5)) >> (irq & 0x1f)) & 1;
}

static inline void save_ctx(void)
{
	u32 t;
	asm("mrs %0, msp\n\t"
	    "stmdb %0!, {r4-r11}\n\t"
	    "msr msp, %0" : "=r" (t));
}

static inline void load_ctx(void)
{
	u32 t;
	asm("mrs %0, msp\n\t"
	    "ldmfd %0!, {r4-r11}\n\t"
	    "msr msp, %0" : "=r" (t));
}

static inline int exception_number(void)
{
	int res;
	asm("mrs %0, psr" : "=r" (res));
	return res & 0x1ff;
}

static inline void wait_for_irq(void)
{
	asm("wfi");
}

#endif /* _IRQ_H_ */

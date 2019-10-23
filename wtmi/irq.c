#include "types.h"
#include "clock.h"
#include "ebg.h"
#include "irq.h"

#define SYSTICK_CTRL	0xe000e010
#define SYSTICK_RELOAD	0xe000e014

void enable_systick(void)
{
	u32 tenms;

	/* generate systick every 10 ms */
	if (get_ref_clk() == 40)
		tenms = 0x186a0;
	else
		tenms = 0xf424;

	writel(tenms, SYSTICK_RELOAD);
	setbitsl(SYSTICK_CTRL, 0x3, 0x3);
}

void disable_systick(void)
{
	setbitsl(SYSTICK_CTRL, 0x0, 0x3);
}

volatile u32 jiffies;

void __irq systick_handler(void)
{
	save_ctx();
	++jiffies;
	ebg_systick();
	load_ctx();
}

static irq_handler_t irq_handlers[16];

void register_irq_handler(int irq, irq_handler_t handler)
{
	irq_handlers[irq] = handler;
}

static void do_external_irq(void)
{
	int irq;

	irq = exception_number() - 16;

	if (irq_handlers[irq])
		irq_handlers[irq](irq);
}

void __irq external_irq(void)
{
	save_ctx();
	do_external_irq();
	load_ctx();
}

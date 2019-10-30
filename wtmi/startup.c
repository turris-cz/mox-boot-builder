#include "types.h"
#include "irq.h"
#include "printf.h"

extern u32 stack_top;
extern void main(void);
extern void reset_handler(void);

/* compressed wtmi does not implement this function */
void __attribute((weak)) disable_systick(void)
{}

void __irq default_handler(void)
{
	disable_systick();
	while (1)
		wait_for_irq();
}

void nmi_handler(void) __attribute((weak, alias("default_handler")));
void hardfault_handler(void) __attribute((weak, alias("default_handler")));
void memmanage_handler(void) __attribute((weak, alias("default_handler")));
void busfault_handler(void) __attribute((weak, alias("default_handler")));
void usagefault_handler(void) __attribute((weak, alias("default_handler")));
void svc_handler(void) __attribute((weak, alias("default_handler")));
void pendsv_handler(void) __attribute((weak, alias("default_handler")));
void systick_handler(void) __attribute((weak, alias("default_handler")));
void external_irq(void) __attribute((weak, alias("default_handler")));

__attribute((section(".isr_vector")))
u32 *isr_vectors[] = {
	(u32 *) &stack_top,		/* stack pointer */
	(u32 *) reset_handler,		/* code entry point */
	(u32 *) nmi_handler,		/* NMI handler */
	(u32 *) hardfault_handler,	/* hard fault handler */
	(u32 *) memmanage_handler,	/* mem manage handler */
	(u32 *) busfault_handler,	/* bus fault handler */
	(u32 *) usagefault_handler,	/* usage fault handler */
	0,
	0,
	0,
	0,
	(u32 *) svc_handler,		/* svc handler */
	0,
	0,
	(u32 *) pendsv_handler,		/* pendsv handler */
	(u32 *) systick_handler,	/* systick handler */
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq,
	(u32 *) external_irq
};

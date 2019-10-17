#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "uart.h"

#ifdef DEBUG_UART2

static inline const struct uart_info *get_debug_uart(void)
{
	return &uart2_info;
}

void debug_init(void);
void debug_process(void);

#else /* !DEBUG_UART2 */

static inline const struct uart_info *get_debug_uart(void)
{
	return &uart1_info;
}

static inline void debug_init(void)
{
}

static inline void debug_process(void)
{
}

#endif /* !DEBUG_UART2 */

#endif /* _DEBUG_H_ */

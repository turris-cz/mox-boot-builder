#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "uart.h"

#ifdef DEBUG_UART2

struct debug_cmd {
	char name[16];
	char help[64];
	void (*cb)(int argc, char **argv);
};

#define CONCAT_(a,b) a##b
#define CONCAT(a,b) CONCAT_(a,b)
#define DEBUG_CMD(n,h,f)						\
	static struct debug_cmd CONCAT(__debug_cmd_info, __COUNTER__)	\
		__attribute__((section(".debug_cmds"), used)) = {	\
		.name = (n),						\
		.help = (h),						\
		.cb = (f),						\
	};

static inline const struct uart_info *get_debug_uart(void)
{
	return &uart2_info;
}

void debug_init(void);
void debug_process(void);

int _number(const char *str, u32 *pres, int base);

#else /* !DEBUG_UART2 */

#define DEBUG_CMD(n,h,f)

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

static inline int _number(const char *str, u32 *pres, int base)
{
	return -1;
}

#endif /* !DEBUG_UART2 */

#define DECL_DEBUG_CMD(f) \
	static void __attribute__((unused)) f(int argc, char **argv)

static inline int number(const char *str, u32 *pres)
{
	return _number(str, pres, 16);
}

static inline int decnumber(const char *str, u32 *pres)
{
	return _number(str, pres, 10);
}

static inline void putc(u8 c)
{
	uart_putc((void *)get_debug_uart(), c);
}

static inline int getc(void)
{
	return uart_getc(get_debug_uart());
}

static inline void puts(const char *s)
{
	while (*s)
		putc(*s++);
}

#endif /* _DEBUG_H_ */

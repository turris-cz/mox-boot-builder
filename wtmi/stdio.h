#ifndef _STDIO_H_
#define _STDIO_H_

#include <stdarg.h>

#define EOF	(-1)

typedef struct {
	int (*putc)(int c, void *data);
	void *data;
} FILE;

extern FILE *stdout;

extern int putc(int c, FILE *stream);
extern int putchar(int c);
extern int fputs(const char *str, FILE *stream);
extern int puts(const char *str);
extern int vfprintf(FILE *stream, const char *fmt, va_list ap);
extern int fprintf(FILE *stream, const char *fmt, ...);
extern int vprintf(const char *fmt, va_list ap);
extern int printf(const char *fmt, ...);

#define __extern_always_inline extern inline __attribute__((__gnu_inline__))

__extern_always_inline int putc(int c, FILE *stream)
{
	return __builtin_putc(c, stream);
}

__extern_always_inline int putchar(int c)
{
	return __builtin_putchar(c);
}

__extern_always_inline int fputs(const char *str, FILE *stream)
{
	return __builtin_fputs(str, stream);
}

__extern_always_inline int puts(const char *str)
{
	return __builtin_puts(str);
}

__extern_always_inline int vfprintf(FILE *stream, const char *fmt, va_list ap)
{
	return __builtin_vfprintf(stream, fmt, ap);
}

__extern_always_inline int fprintf(FILE *stream, const char *fmt, ...)
{
	return __builtin_fprintf(stream, fmt, __builtin_va_arg_pack());
}

__extern_always_inline int vprintf(const char *fmt, va_list ap)
{
	return __builtin_vprintf(fmt, ap);
}

__extern_always_inline int printf(const char *fmt, ...)
{
	return __builtin_printf(fmt, __builtin_va_arg_pack());
}

#define debug(...)

#endif /* !_STDIO_H */

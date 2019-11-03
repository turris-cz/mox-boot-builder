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

#define debug(...)

#endif /* !_STDIO_H */

#include <stdarg.h>
#include "io.h"
#include "stdio.h"
#include "uart.h"
#include "string.h"

FILE *stdout = NULL;

enum flags {
	FLAG_NONE	= 0,
	FLAG_ALT	= BIT(0),
	FLAG_ZEROPAD	= BIT(1),
	FLAG_ADJUST	= BIT(2),
	FLAG_BLANK	= BIT(3),
	FLAG_SIGN	= BIT(4),
};

enum mod {
	MOD_NONE,
	MOD_HH,
	MOD_H,
	MOD_L,
	MOD_LL,
};

int putc(int c, FILE *stream)
{
	if (stream && stream->putc)
		return stream->putc(c, stream->data);
	else
		return EOF;
}

int putchar(int c)
{
	return putc(c, stdout);
}

int fputs(const char *str, FILE *stream)
{
	while (*str)
		if (putc(*str++, stream) < 0)
			return -1;
	return 0;
}

int puts(const char *str)
{
	if (fputs(str, stdout) < 0)
		return -1;
	return putchar('\n');
}

static inline enum flags get_flags(const char **fmt)
{
	enum flags flags = FLAG_NONE;

	while (**fmt) {
		switch (**fmt) {
		case '#':
			flags |= FLAG_ALT;
			break;
		case '0':
			flags |= FLAG_ZEROPAD;
			break;
		case '-':
			flags |= FLAG_ADJUST;
			break;
		case ' ':
			flags |= FLAG_BLANK;
			break;
		case '+':
			flags |= FLAG_SIGN;
			break;
		default:
			return flags;
		}

		++(*fmt);
	}

	return flags;
}

static inline int isdigit(int c)
{
	return c >= '0' && c <= '9';
}

static inline int get_number(const char **fmt, va_list ap)
{
	int res = 0;

	if (**fmt == '*') {
		++(*fmt);
		return va_arg(ap, int);
	}

	if (!isdigit(**fmt))
		return -1;

	while (isdigit(**fmt)) {
		res *= 10;
		res += *(*fmt)++ - '0';
	}

	return res;
}

static inline enum mod get_modifier(const char **fmt)
{
	enum mod mod = MOD_NONE;

	switch (**fmt) {
	case 'h':
		++(*fmt);
		if (**fmt == 'h')
			mod = MOD_HH, ++(*fmt);
		else
			mod = MOD_H;
		break;
	case 'l':
		++(*fmt);
		if (**fmt == 'l')
			mod = MOD_LL, ++(*fmt);
		else
			mod = MOD_L;
		break;
	case 'q':
		++(*fmt);
		mod = MOD_LL;
		break;
	}

	return mod;
}

#define PUT(c) putc((c), stream), ++res

static int do_justify(FILE *stream, int left, int width, const char *str,
		      size_t len)
{
	int res, i;

	res = 0;

	if (!left && width > 0) {
		if (len == -1)
			len = strlen(str);

		if (width > len) {
			for (i = 0; i < width - len; ++i)
				PUT(' ');
		}
	}

	while (*str)
		PUT(*str++);

	if (left && width > res) {
		for (i = res; i < width; ++i)
			PUT(' ');
	}

	return res;
}

static inline int do_char(FILE *stream, int left, int width, unsigned char c)
{
	const char str[2] = { (char) c, '\0' };
	return do_justify(stream, left, width, str, 1);
}

static const char digitsL[16] = "0123456789abcdef";
static const char digitsH[16] = "0123456789ABCDEF";

static int do_number(FILE *stream, enum flags flags, int width, int prec,
		     char spec, u64 number, int negative)
{
	const char *digits = digitsL;
	const char *pfx = "";
	char *p;
	int len, pfxlen, res, sign, zeros, spaces, minlen;
	u32 base;
	char buf[65];

	switch (spec) {
	case 'X':
		digits = digitsH;
		if (flags & FLAG_ALT)
			pfx = "0X";
		base = 16;
		break;
	case 'p':
		pfx = "0x";
		base = 16;
	case 'x':
		if (flags & FLAG_ALT)
			pfx = "0x";
		base = 16;
		break;
	case 'o':
		if (flags & FLAG_ALT)
			pfx = "0";
		base = 8;
		break;
	case 'b':
		if (flags & FLAG_ALT)
			pfx = "0b";
		base = 2;
		break;
	default:
		base = 10;
	}

	p = &buf[64];
	*p = '\0';

	len = 0;
	while (number) {
		*--p = digits[number % base];
		number /= base;
		++len;
	}

	pfxlen = strlen(pfx);
	sign = (flags & (FLAG_SIGN | FLAG_BLANK)) || negative;

	if (prec > -1) {
		zeros = MAX(prec - len, 0);
		minlen = len + zeros + pfxlen + sign;
		width = MAX(minlen, width);
		spaces = width - minlen;
	} else {
		if (!len) {
			*--p = '0';
			++len;
		}
		minlen = len + pfxlen + sign;
		width = MAX(width, minlen);
		if (flags & FLAG_ZEROPAD) {
			zeros = width - minlen;
			spaces = 0;
		} else {
			zeros = 0;
			spaces = width - minlen;
		}
	}

	res = 0;
	if (!(flags & FLAG_ADJUST)) {
		while (spaces--)
			PUT(' ');
	}

	if (sign)
		PUT(negative ? '-' : (flags & FLAG_SIGN) ? '+' : ' ');

	while (*pfx)
		PUT(*pfx++);

	while (zeros--)
		PUT('0');

	while (*p)
		PUT(*p++);

	if (flags & FLAG_ADJUST) {
		while (spaces--)
			PUT(' ');
	}

	if (flags & FLAG_ZEROPAD)
		prec = width - pfxlen - sign;

	return res;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap)
{
	int res = 0;

	while (*fmt) {
		enum flags flags;
		enum mod mod;
		int width, prec = -1;

		if (*fmt != '%') {
			PUT(*fmt++);
			continue;
		}

		++fmt;
		flags = get_flags(&fmt);
		width = get_number(&fmt, ap);

		if (*fmt == '.') {
			++fmt;
			prec = get_number(&fmt, ap);
		}

		mod = get_modifier(&fmt);

		switch (*fmt) {
			u64 x, sx;
		case '\0':
			return -1;
		case '%':
			PUT('%');
			break;
		case 'd':
		case 'i':
			if (mod == MOD_LL)
				sx = va_arg(ap, long long);
			else if (mod == MOD_L)
				sx = va_arg(ap, long);
			else
				sx = va_arg(ap, int);
			res += do_number(stream, flags, width, prec, *fmt,
					 sx < 0 ? -sx : sx, sx < 0);
			break;
		case 'u':
		case 'x':
		case 'X':
		case 'b':
		case 'o':
			if (mod == MOD_LL)
				x = va_arg(ap, unsigned long long);
			else if (mod == MOD_L)
				x = va_arg(ap, unsigned long);
			else
				x = va_arg(ap, unsigned int);
			res += do_number(stream, flags, width, prec, *fmt, x,
					 0);
			break;
		case 'p':
			x = (unsigned long) va_arg(ap, void *);
			res += do_number(stream, flags, width, prec, *fmt, x,
					 0);
			break;
		case 'c':
			res += do_char(stream, flags & FLAG_ADJUST, width,
				       va_arg(ap, int));
			break;
		case 's':
			res += do_justify(stream, flags & FLAG_ADJUST, width,
					  va_arg(ap, const char *), -1);
			break;
		default:
			break;
		}

		++fmt;
	}

	return res;
}

int fprintf(FILE *stream, const char *fmt, ...)
{
	int res;
	va_list ap;

	va_start(ap, fmt);
	res = vfprintf(stream, fmt, ap);
	va_end(ap);

	return res;
}

int vprintf(const char *fmt, va_list ap)
{
	return vfprintf(stdout, fmt, ap);
}

int printf(const char *fmt, ...)
{
	int res;
	va_list ap;

	va_start(ap, fmt);
	res = vfprintf(stdout, fmt, ap);
	va_end(ap);

	return res;
}
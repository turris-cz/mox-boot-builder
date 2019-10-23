#include "errno.h"
#include "types.h"
#include "io.h"
#include "clock.h"
#include "ebg.h"
#include "crypto.h"
#include "crypto_hash.h"
#include "string.h"
#include "printf.h"
#include "engine.h"
#include "debug.h"

#define EBG_CTRL	0x40002c00
#define EBG_ENTROPY	0x40002c04

static u16 ebg_buffer[512];
static int ebg_fill;
static u32 reg;

static int ebg_next(int wait, u16 *res)
{
	u32 val;

	if (wait) {
		while (!((val = readl(EBG_ENTROPY)) & BIT(31)))
			wait_ns(10);
	} else {
		val = readl(EBG_ENTROPY);
		if (!(val & BIT(31)))
			return -ETIMEDOUT;
	}

	writel(reg | 0x1000, EBG_CTRL);

	if (res)
		*res = val & 0xffff;

	return 0;
}

void ebg_init(void)
{
	u16 x;
	int i;

	engine_clk_disable(Engine_EBG);
	engine_clk_enable(Engine_EBG);
	engine_reset(Engine_EBG);

	reg = readl(EBG_CTRL);
	reg &= 0x901f;
	writel(reg, EBG_CTRL);

	reg |= 0x200;
	writel(reg, EBG_CTRL);

	wait_ns(400000);

	reg |= 0x80;
	writel(reg, EBG_CTRL);

	reg |= 0x400;
	writel(reg, EBG_CTRL);

	reg |= 0x03104840;
	writel(reg, EBG_CTRL);

	wait_ns(100000);
	writel(reg | 0x1000, EBG_CTRL);

	/* throw away first 16 values */
	for (i = 0; i < 16; ++i)
		ebg_next(1, NULL);

	/* now throw away another random number of values */
	ebg_next(1, &x);
	x &= 0x1f;
	for (i = 0; i < x; ++i)
		ebg_next(1, NULL);
}

void ebg_systick(void) {
	u16 val;

	if (ebg_fill >= 512)
		return;

	if (ebg_next(0, &val) < 0)
		return;

	ebg_buffer[ebg_fill++] = val;
}

int ebg_rand(void *buffer, int size)
{
	int has = ebg_fill * sizeof(u16);

	if (has < size)
		size = has;

	ebg_fill -= (size + 1) / 2;
	memcpy(buffer, (void *) ebg_buffer + has - size, size);

	return size;
}

void ebg_rand_sync(void *buffer, int size)
{
	while (size >= 2) {
		ebg_next(1, (u16 *) buffer);
		size -= 2;
		buffer += 2;
	}

	if (size == 1) {
		u16 x;
		ebg_next(1, &x);
		*(u8 *) buffer = x & 0xff;
	}
}

static inline void xor(u32 *d, const u32 *s)
{
	int i;

	for (i = 0; i < 16; ++i)
		*d++ ^= *s++;
}

/*
 * This function collects 512 bytes from the Entropy Bit Generator, appends to
 * it the digest computed in the previous call to this function (if there was no
 * previous call it appends zeros), and hashes the resultin 576 bytes with
 * sha512. The resulting digest will be used in the next call, and is also
 * combined with the output of EBG to generate the result.
 *              call 0               call 1
 * prev digest  0000                 H(ebg1|0000)
 * EBG output   ebg1                 ebg2                         ...
 * cur digest   H(ebg1|0000)         H(ebg2|H(ebg1|0000))
 * result       C(ebg1,H(ebg1|0000)) C(ebg2,H(ebg2|H(ebg1|0000)))
 */
static void paranoid_rand_64(u32 *buffer)
{
	static u32 ebgbuf[128 + 16] __attribute__((aligned(16)));
	static u32 dgst[16];
	static int c;
	int i;

	ebg_rand_sync(ebgbuf, sizeof(ebgbuf) - sizeof(dgst));
	hw_sha512(ebgbuf, sizeof(ebgbuf), dgst);
	memcpy(&ebgbuf[128], dgst, sizeof(dgst));

	for (i = 0; i < 128; i += 16) {
		if (c)
			xor(dgst, ebgbuf + i);
		else
			c ^= bn_add(dgst, ebgbuf + i, 16);
		c ^= (dgst[0] >> (i >> 4)) & 1;
	}

	for (i = 0; i < 16; ++i)
		buffer[i] = dgst[i];
}

void paranoid_rand(void *buffer, int size)
{
	u32 tmp[16];
	u32 addr = (u32) buffer;

	if (size < 4) {
		paranoid_rand_64(tmp);
		memcpy(buffer, tmp, size);
		return;
	}

	if (addr & 3) {
		int s = 4 - (addr & 3);
		paranoid_rand_64(tmp);
		memcpy(buffer, tmp, s);
		buffer += s;
		size -= s;
	}

	while (size >= 64) {
		paranoid_rand_64(buffer);
		buffer += 64;
		size -= 64;
	}

	if (size > 0) {
		paranoid_rand_64(tmp);
		memcpy(buffer, tmp, size);
	}
}

#if 0
DECL_DEBUG_CMD(cmd_rand)
{
	u32 len = 0, i;

	if (argc < 2)
		goto usage;

	if (argc == 3 && number(argv[2], &len))
		return;

	if (!strcmp(argv[1], "raw")) {
		u16 val;

		if (!len)
			len = 1;

		for (i = 0; i < len; ++i) {
			ebg_next(1, &val);
			printf("%04x", val);
		}
		putc('\n');
	} else if (!strcmp(argv[1], "strong")) {
		u32 tmp[16];

		paranoid_rand_64(tmp);
		for (i = 0; i < 16; ++i)
			printf("%08x", __builtin_bswap32(tmp[i]));
		putc('\n');
	} else {
		goto usage;
	}

	return;
usage:
	puts("usage: rand raw [n]\n");
	puts("       rand strong\n");
}

DEBUG_CMD("rand", "TRNG testing utility", cmd_rand);
#endif

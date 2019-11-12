#include "errno.h"
#include "types.h"
#include "io.h"
#include "clock.h"
#include "ebg.h"
#include "crypto.h"
#include "crypto_hash.h"
#include "string.h"
#include "engine.h"
#include "debug.h"

#define EBG_CTRL	0x40002c00
#define EBG_ENTROPY	0x40002c04

typedef struct {
	u8 *data;
	const u32 size;
	u32 len, pos;
} cbuf_t;

extern u8 ebg_buffer[4096];
extern u8 paranoid_rand_buffer[4096];

static cbuf_t ebg_cbuf = {
	.data = ebg_buffer,
	.size = sizeof(ebg_buffer),
};

static cbuf_t paranoid_rand_cbuf = {
	.data = paranoid_rand_buffer,
	.size = sizeof(paranoid_rand_buffer),
};

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

static u32 cbuf_pull(cbuf_t *cbuf, void *dest, u32 len)
{
	u32 res;

	len = res = MIN(cbuf->len, len);

	if (cbuf->pos + len > cbuf->size) {
		u32 tail = cbuf->size - cbuf->pos;

		memcpy(dest, &cbuf->data[cbuf->pos], tail);
		bzero(&cbuf->data[cbuf->pos], tail);
		dest += tail;
		len -= tail;
		cbuf->len -= tail;
		cbuf->pos = 0;
	}

	memcpy(dest, &cbuf->data[cbuf->pos], len);
	bzero(&cbuf->data[cbuf->pos], len);
	cbuf->len -= len;
	cbuf->pos = (cbuf->pos + len) % cbuf->size;

	return res;
}

static inline u32 cbuf_free_space(const cbuf_t *cbuf)
{
	return cbuf->size - cbuf->len;
}

static u32 cbuf_push(cbuf_t *cbuf, const void *src, u32 len)
{
	u32 res, pos;
	u8 *dest;

	len = res = MIN(cbuf_free_space(cbuf), len);
	pos = (cbuf->pos + cbuf->len) % cbuf->size;
	dest = &cbuf->data[pos];

	if (pos + len > cbuf->size) {
		u32 tail = cbuf->size - pos;

		memcpy(dest, src, tail);
		src += tail;
		len -= tail;
		cbuf->len += tail;
		dest = &cbuf->data[0];
	}

	memcpy(dest, src, len);
	cbuf->len += len;

	return res;
}

static const void *paranoid_rand_64(void);

void ebg_process(void) {
	u16 val;

	if (ebg_cbuf.len >= 512 && cbuf_free_space(&paranoid_rand_cbuf) >= 64)
		cbuf_push(&paranoid_rand_cbuf, paranoid_rand_64(), 64);

	if (ebg_cbuf.len > ebg_cbuf.size - 2)
		return;

	if (ebg_next(0, &val) < 0)
		return;

	cbuf_push(&ebg_cbuf, &val, 2);
}

void ebg_rand_sync(void *buffer, u32 size)
{
	u32 pull;
	u16 val;

	pull = cbuf_pull(&ebg_cbuf, buffer, size);
	buffer += pull;
	size -= pull;

	if (size && (((u32)buffer) & 1)) {
		ebg_next(1, &val);
		*(u8 *)buffer = val & 0xff;
		++buffer;
		--size;
	}

	while (size > 1) {
		ebg_next(1, &val);
		*(u16 *)buffer = val;
		buffer += 2;
		size -= 2;
	}

	if (size) {
		ebg_next(1, &val);
		*(u8 *)buffer = val & 0xff;
	}
}

u32 ebg_rand(void *buffer, u32 size)
{
	return cbuf_pull(&ebg_cbuf, buffer, MIN(ebg_cbuf.len, size));
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
 *              call 0             call 1           call 2
 * prev digest  0000               H0               H1
 * EBG output   EBG0               EBG1             EBG2             ...
 * cur digest   H0 = H(EBG0|0000)  H1 = H(EBG1|H0)  H2 = H(EBG2|H1)
 * result       C(EBG0,H0)         C(EBG1,H1)       C(EBG2|H2)
 */
static const void *paranoid_rand_64(void)
{
	extern u32 ebgbuf[128 + 16] asm("paranoid_rand_tmp");
	extern u32 dgst[16] asm("paranoid_rand_dgst");
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

	return dgst;
}

void paranoid_rand(void *buffer, u32 size)
{
	u32 pull;

	pull = cbuf_pull(&paranoid_rand_cbuf, buffer, size);
	buffer += pull;
	size -= pull;

	while (size >= 64) {
		memcpy(buffer, paranoid_rand_64(), 64);
		buffer += 64;
		size -= 64;
	}

	if (size) {
		const void *dgst = paranoid_rand_64();
		memcpy(buffer, dgst, size);
		cbuf_push(&paranoid_rand_cbuf, dgst + size, 64 - size);
	}
}

DECL_DEBUG_CMD(cmd_rand)
{
	u32 len = 0, i, val;

	if (argc < 2)
		goto usage;

	if (argc == 3 && number(argv[2], &len))
		return;

	if (!len)
		len = 1;

	if (!strcmp(argv[1], "raw")) {
		for (i = 0; i < len; ++i) {
			ebg_rand_sync(&val, sizeof(val));
			printf("%08x\n", val);
		}
	} else if (!strcmp(argv[1], "strong")) {
		for (i = 0; i < len; ++i) {
			paranoid_rand(&val, sizeof(val));
			printf("%08x\n", val);
		}
	} else if (!strcmp(argv[1], "state")) {
		printf("EBG buffer: %u/%u filled, start at %u\n", ebg_cbuf.len,
		       ebg_cbuf.size, ebg_cbuf.pos);
		printf("Paranoid rand buffer: %u/%u filled, start at %u\n",
		       paranoid_rand_cbuf.len, paranoid_rand_cbuf.size,
		       paranoid_rand_cbuf.pos);
	} else {
		goto usage;
	}

	return;
usage:
	printf("usage: rand raw [n]\n");
	printf("       rand strong [n]\n");
	printf("       rand state\n");
}

DEBUG_CMD("rand", "TRNG testing utility", cmd_rand);

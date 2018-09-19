#include "errno.h"
#include "types.h"
#include "io.h"
#include "clock.h"
#include "ebg.h"
#include "mbox.h"

#define EBG_CTRL	0x40002c00
#define EBG_ENTROPY	0x40002c04

static u16 ebg_buffer[512];
static int ebg_fill;
static u32 reg;

void ebg_init(void)
{
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

	writel(reg | 0x1000, EBG_CTRL);
}

static int ebg_next(int wait, u16 *res)
{
	u32 val;

	if (wait) {
		while (!((val = readl(EBG_ENTROPY)) & BIT(31)))
			wait_ns(51300);
	} else {
		val = readl(EBG_ENTROPY);
		if (!(val & BIT(31)))
			return -ETIMEDOUT;
	}

	writel(reg | 0x1000, EBG_CTRL);

	*res = val & 0xffff;
	return 0;
}

void ebg_systick(void) {
	u16 val;

	if (ebg_fill >= 512)
		return;

	if (ebg_next(0, &val) < 0)
		return;

	ebg_buffer[ebg_fill++] = val;
}

void *memcpy(void *dest, const void *src, u32 n)
{
	u8 *d = dest, *e = d + n;
	const u8 *s = src;

	while (d < e)
		*d++ = *s++;

	return dest;
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
		buffer -= 1;
		ebg_next(1, (u16 *) buffer);
	}
}

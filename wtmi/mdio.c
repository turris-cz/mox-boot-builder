#include "types.h"
#include "io.h"
#include "clock.h"
#include "mdio.h"
#include "debug.h"

#define SMI		0xc0032004
#define SMI_BUSY	BIT(28)
#define SMI_READ_VALID	BIT(27)
#define SMI_READ	BIT(26)
#define SMI_DEV(x)	(((x) & 0x1f) << 16)
#define SMI_REG(x)	(((x) & 0x1f) << 21)

static inline void mdio_wait(void)
{
	while (readl(SMI) & SMI_BUSY)
		ndelay(100);
}

static int mdio_pins_disable;

void mdio_begin(void)
{
	if (readl(SB_PINCTRL) & BIT(4)) {
		setbitsl(SB_PINCTRL, 0, BIT(4));
		mdio_pins_disable = 1;
	}
}

void mdio_end(void)
{
	if (mdio_pins_disable) {
		setbitsl(SB_PINCTRL, BIT(4), BIT(4));
		mdio_pins_disable = 0;
	}
}

void mdio_write(int dev, int reg, u16 val)
{
	mdio_wait();
	writel(SMI_DEV(dev) | SMI_REG(reg) | val, SMI);
	mdio_wait();
}

int mdio_read(int dev, int reg)
{
	u32 val;

	mdio_wait();
	writel(SMI_DEV(dev) | SMI_REG(reg) | SMI_READ, SMI);

	mdio_wait();
	val = readl(SMI);

	if (val & SMI_READ_VALID)
		return val & 0xffff;
	else
		return -1;
}

DECL_DEBUG_CMD(cmd_mii)
{
	u32 addr, reg;

	if (argc < 4 || number(argv[2], &addr) || number(argv[3], &reg) ||
	    addr > 0x1f || reg > 0x1f)
		goto usage;

	if (argv[1][0] == 'r') {
		int res;

		mdio_begin();
		res = mdio_read(addr, reg);
		mdio_end();

		if (res < 0)
			printf("Cannot read at address %#04x!\n", addr);
		else
			printf("%#06x\n", res);
	} else if (argv[1][0] == 'w') {
		u32 val;

		if (argc < 5 || number(argv[4], &val) || val > 0xffff)
			goto usage;

		mdio_begin();
		mdio_write(addr, reg, val);
		mdio_end();
	} else {
		goto usage;
	}

	return;
usage:
	printf("usage: mii read <addr> <reg>\n"
	       "       mii write <addr> <reg> <data>\n");
}

DEBUG_CMD("mii", "raw MII access", cmd_mii);

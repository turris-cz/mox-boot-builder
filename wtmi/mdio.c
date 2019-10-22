#include "types.h"
#include "io.h"
#include "clock.h"
#include "mdio.h"

#define SMI		0xc0032004
#define SMI_BUSY	BIT(28)
#define SMI_READ_VALID	BIT(27)
#define SMI_READ	BIT(26)
#define SMI_DEV(x)	(((x) & 0x1f) << 16)
#define SMI_REG(x)	(((x) & 0x1f) << 21)

static inline void mdio_wait(void)
{
	while (readl(SMI) & SMI_BUSY)
		wait_ns(10);
}

void mdio_write(int dev, int reg, u16 val)
{
	mdio_wait();
	writel(SMI_DEV(dev) | SMI_REG(reg) | val, SMI);
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

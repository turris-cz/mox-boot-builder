#ifndef _GIC_H_
#define _GIC_H_

#include "io.h"

#if !defined(GICD_BASE) || !defined(GICR_BASE)
#error do not include gic.h directly
#endif

#define GIC_CTLR		0x0
#define GIC_IGROUPR		0x80
#define GIC_ISENABLER		0x100
#define GIC_ICENABLER		0x180
#define GIC_ISPENDR		0x200
#define GIC_ICPENDR		0x280
#define GIC_ISACTIVER		0x300
#define GIC_ICACTIVER		0x380
#define GIC_IPRIORITYR		0x400
#define GIC_ICFGR		0xc00

#define GICD_CTLR		GIC_CTLR
#define GICD_IGROUPR		GIC_IGROUPR
#define GICD_IPRIORITYR		GIC_IPRIORITYR
#define GICD_ICFGR		GIC_ICFGR

#define CTLR_ENABLE_G0_BIT	BIT(0)
#define CTLR_ENABLE_G1NS_BIT	BIT(1)
#define CTLR_ENABLE_G1S_BIT	BIT(2)
#define CTLR_ARE_S_BIT		BIT(4)
#define CTLR_ARE_NS_BIT		BIT(5)
#define GICD_CTLR_RWP_BIT	BIT(31)
#define GICR_CTLR_RWP_BIT	BIT(3)
#define WAKER_PS_BIT		BIT(1)
#define WAKER_CA_BIT		BIT(2)

#define GICR_CTLR		0x0
#define GICR_WAKER		0x14
#define GICR_SGI_BASE		0x10000
#define GICR_IGROUPR		(GICR_SGI_BASE + GIC_IGROUPR)
#define GICR_ISENABLER0		(GICR_SGI_BASE + GIC_ISENABLER)
#define GICR_ICENABLER0		(GICR_SGI_BASE + GIC_ICENABLER)
#define GICR_ISPENDR0		(GICR_SGI_BASE + GIC_ISPENDR)
#define GICR_ICPENDR0		(GICR_SGI_BASE + GIC_ICPENDR)
#define GICR_IPRIORITYR		(GICR_SGI_BASE + GIC_IPRIORITYR)
#define GICR_ICFGR		(GICR_SGI_BASE + GIC_ICFGR)

#define GIC_ERROR(...)

static inline u32 gicd_read(u32 reg)
{
	return readl(GICD_BASE + reg);
}

static inline void gicd_write(u32 reg, u32 value)
{
	writel(value, GICD_BASE + reg);
}

static inline void gicd_ctlr_clear_bits(u32 bits)
{
	u32 val;

	val = gicd_read(GICD_CTLR);
	if (val & bits) {
		gicd_write(GICD_CTLR, val & ~bits);
		udelay(1000);

		if (gicd_read(GICD_CTLR) & GICD_CTLR_RWP_BIT)
			GIC_ERROR("could not clear bits 0x%x in GIC distributor control\n",
				  bits);
	}
}

static inline void gic_dist_disable_irqs(void)
{
	int i;

	for (i = 32; i < 224; i += 32)
		gicd_write(GIC_ICENABLER + (i >> 3), 0xffffffff);
}

static inline u32 gic_rdist_base(unsigned int proc)
{
	return GICR_BASE + 0x20000 * proc;
}

static inline u32 gicr_read(unsigned int proc, u32 reg)
{
	return readl(gic_rdist_base(proc) + reg);
}

static inline void gicr_write(unsigned int proc, u32 reg, u32 value)
{
	writel(value, gic_rdist_base(proc) + reg);
}

static inline void gic_redist_disable_irqs(unsigned int proc)
{
	gicr_write(proc, GICR_ICENABLER0, 0xffffffff);
	udelay(1000);

	if (gicr_read(proc, GICR_CTLR) & GICR_CTLR_RWP_BIT)
		GIC_ERROR("could not disable core %u PPIs & SGIs\n", proc);
}

static inline void gic_redist_mark_asleep(unsigned int proc)
{
	gicr_write(proc, GICR_WAKER,
		   gicr_read(proc, GICR_WAKER) | WAKER_PS_BIT);
	udelay(1000);

	if (!(gicr_read(proc, GICR_WAKER) & WAKER_CA_BIT))
		GIC_ERROR("could not mark core %u redistributor asleep\n",
			  proc);
}

#endif /* _GIC_H_ */

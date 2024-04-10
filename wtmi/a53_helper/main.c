#include "io.h"

static inline int cpuid(void)
{
	return read_mpidr_el1() & 0xff;
}

static void udelay(u32 usec)
{
	u32 start, delta, total_delta;

	start = (u32)~read_cntpct_el0();
	total_delta = 25 * usec / 2 + 1U;
	do {
		delta = start - (u32)~read_cntpct_el0();
	} while (delta < total_delta);
}

#define RWTM_CMD_PARAM(i)	(size_t)(0xd00b0000 + (i) * 4)
#define RWTM_CMD		0xd00b0040
#define RWTM_HOST_INT_RESET	0xd00b00c8
#define RWTM_HOST_INT_MASK	0xd00b00cc

#define A53_HELPER_DONE		0x10010000
#define A53_HELPER_ARGS		0x10010004

static inline u32 arg(u32 i)
{
	return readl(A53_HELPER_ARGS + 4*i);
}

static void do_reset(void);

const u64 stack_core0 = 0x0f000000;
const u64 stack_core1 = 0x0e000000;

static void core1_reset(int reset)
{
	u32 reg = readl(0xd000d00c);
	if (reset)
		reg &= ~BIT(31);
	else
		reg |= BIT(31);
	writel(reg, 0xd000d00c);
}

void run_on_core1_helper_asm(void);

static void core1_boot(void)
{
	u32 reg = (u32)(u64)run_on_core1_helper_asm;

	dsb();
	writel(reg >> 2, 0xd0014044);
	core1_reset(1);
	core1_reset(0);
}

static void setup_regs(void)
{
	write_cntfrq_el0(25000000);
	write_sysreg_s(BIT(0) | BIT(3), SYS_ICC_SRE_EL3);
	write_sysreg_s(BIT(0), SYS_ICC_SRE_EL1);
}

void run_on_core1_helper(void)
{
	setup_regs();
	while (1)
		nop();
}

static void core0_main(void)
{
	do_reset();

	writel(1, A53_HELPER_DONE);
	while (1)
		nop();
}

void main(void)
{
	setup_regs();
	core1_boot();
	core0_main();
}

struct reg_val {
	u32 reg;
	u32 val;
};

static void _write_reg_vals(const struct reg_val *p, u32 n)
{
	while (n) {
		writel(p->val, p->reg);
		++p, --n;
	}
}

#define write_reg_vals(x) _write_reg_vals((x), sizeof((x))/sizeof((x)[0]))

static const struct reg_val reset_dvfs_regs[] = {
	/* Disable DVFS */
	{ 0xd0014024, 0 },

	/* Load Levels to default values */
	{ 0xd0014018, 0x48004800 },
	{ 0xd001401c, 0x48004800 },

	/* Disable AVS */
	{ 0xd0011500, 0x18e3ffff },

	/* Disable low voltage mode */
	{ 0xd0011508, 0x00008000 },

	/* Reset AVS VSET 1, 2 and 3 */
	{ 0xd001151c, 0x0526ffff },
	{ 0xd0011520, 0x0526ffff },
	{ 0xd0011524, 0x0526ffff },

	/* Enable AVS */
	{ 0xd0011500, 0x58e3ffff },
};

static void dvfs_clear_detect(void)
{
	u32 reg;

	reg = readl(0xd0014000);
	if (reg & (BIT(9) | BIT(13) | BIT(14) | BIT(15)))
		writel(reg & ~BIT(15), 0xd0014000);
}

static void reset_dvfs(void)
{
	u32 reg;

	dvfs_clear_detect();

	if (!(readl(0xd0014024) & BIT(31)))
		return;

	/* Switch to Load Level L0 */
	reg = readl(0xd0014030) & 3;
	if (reg >= 2) {
		/* If on L2 or L3, first switch to L1 and wait 20ms */
		writel(1, 0xd0014030);
		udelay(20000);
	}
	writel(0, 0xd0014030);
	udelay(100);

	dvfs_clear_detect();

	write_reg_vals(reset_dvfs_regs);
	udelay(10000);
}

static void soc_ack_irqs(void)
{
	u32 reg;

#define ACK(addr)			\
	reg = readl((addr));		\
	if (reg)			\
		writel(reg, (addr));
	ACK(0xd0008a00);
	ACK(0xd0008a10);
	ACK(0xd0008a20);
	ACK(0xd0008a30);
	ACK(0xd0008a40);
	ACK(0xd0008a50);
	ACK(0xd0013c10);
	ACK(0xd0013c14);
	ACK(0xd0018c10);
#undef ACK
}

#define GICD_BASE	0xd1d00000
#define GICR_BASE	0xd1d40000

#include "gic.h"

static void reset_soc_irqs(void)
{
	gic_redist_disable_irqs(0);
	gic_redist_disable_irqs(1);

	gic_redist_mark_asleep(0);
	gic_redist_mark_asleep(1);

	soc_ack_irqs();

	gic_dist_disable_irqs();

	gicd_ctlr_clear_bits(CTLR_ENABLE_G0_BIT | CTLR_ENABLE_G1NS_BIT |
			     CTLR_ENABLE_G1S_BIT);

	/* Clearing ARE_S and ARE_NS bits is undefined in the specification, but
	 * works if the previous operations are successful. We need to do it in
	 * order to put GIC into the same state it was in just after reset.
	 */

	gicd_ctlr_clear_bits(CTLR_ARE_S_BIT);
	gicd_ctlr_clear_bits(CTLR_ARE_NS_BIT);
}

static const struct reg_val reset_wdt_and_counters_regs[] = {
	{ 0xd000d064, 0 },
	{ 0xd0008300, 0x200 },
	{ 0xd0008310, 0x200 },
	{ 0xd0008320, 0x1904 },
	{ 0xd0008320, 0x1905 },
	{ 0xd0008330, 0x204 },
	{ 0xd0008330, 0x205 },
};

static void do_reset(void)
{
	if (gicd_read(GICD_CTLR))
		reset_soc_irqs();
	else
		soc_ack_irqs();

	reset_dvfs();
	write_reg_vals(reset_wdt_and_counters_regs);
}

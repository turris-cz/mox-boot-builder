#include "io.h"

static inline int cpuid(void)
{
	return read_mpidr_el1() & 0xff;
}

static void udelay(u32 usec)
{
	u32 start, delta, total_delta;

	start = (u32)~read_cntpct_el0();
	total_delta = 25*usec/2 + 1U;
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

static void run_el1(void (*func)(void));
static void do_reset(void);

extern const u32 vbar_start[];
const u64 stack_core0 = 0x10000000;
const u64 stack_core1 = 0x0f000000;
const u64 stack_core0_el1 = 0x0e000000;
const u64 stack_core1_el1 = 0x0d000000;

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

	asm("dsb sy\n");
	writel(reg >> 2, 0xd0014044);
	core1_reset(1);
	core1_reset(0);
}

static volatile void (*run_on_core1_cb)(void);

static void run_on_core1_helper_el1(void)
{
	while (1) {
		while (!run_on_core1_cb) {
			udelay(1000);
			asm("dsb sy\n");
		}

		run_on_core1_cb();

		run_on_core1_cb = NULL;
	}
}

static void setup_regs(void)
{
	write_cntfrq_el0(25000000);
	write_sysreg_s(BIT(0) | BIT(3), SYS_ICC_SRE_EL3);
	write_sysreg_s(BIT(0), SYS_ICC_SRE_EL1);
	write_vbar_el1((u64) &vbar_start);
	write_vbar_el3((u64) &vbar_start);
}

void run_on_core1_helper(void)
{
	setup_regs();
	run_el1(run_on_core1_helper_el1);
}

static void run_on_core1(void (*cb)(void))
{
	while (run_on_core1_cb) {
		udelay(1000);
		asm("dsb sy\n");
	}

	run_on_core1_cb = cb;
}

static void run_on_core1_wait(void (*cb)(void))
{
	run_on_core1(cb);

	while (run_on_core1_cb) {
		udelay(1000);
		asm("dsb sy\n");
	}
}

static void main_el1(void)
{
	do_reset();

	writel(1, A53_HELPER_DONE);
	while (1)
		;
}

void main(void)
{
	setup_regs();
	core1_boot();

	run_el1(main_el1);
}

__attribute__((noreturn)) void vbar(int ex)
{
	asm("eret\n");
}

#define DECL_EXCEPTION(n,a)				\
__attribute__((noreturn, section(".vbar" # n)))	\
void vbar##n(void)				\
{						\
	vbar(a);				\
}
DECL_EXCEPTION(0, 0)
DECL_EXCEPTION(1, 1)
DECL_EXCEPTION(2, 2)
DECL_EXCEPTION(3, 3)
DECL_EXCEPTION(4, 4)
DECL_EXCEPTION(5, 5)
DECL_EXCEPTION(6, 6)
DECL_EXCEPTION(7, 7)
DECL_EXCEPTION(8, 8)
DECL_EXCEPTION(9, 9)
DECL_EXCEPTION(A, 0xa)
DECL_EXCEPTION(B, 0xb)
DECL_EXCEPTION(C, 0xc)
DECL_EXCEPTION(D, 0xd)
DECL_EXCEPTION(E, 0xe)
DECL_EXCEPTION(F, 0xf)
#undef DECL_EXCEPTION

static void run_el1(void (*func)(void))
{
	write_sctlr_el1(BIT(29) | BIT(28) | BIT(23) | BIT(22) | BIT(20) | BIT(11));
	write_scr_el3((read_scr_el3() | BIT(10) | BIT(0)) & ~(BIT(8) | BIT(0) | BIT(1) | BIT(2)));

	/* disable el2 */
	write_hcr_el2(BIT(31));
	write_cptr_el2(0x33ff);
	write_cnthctl_el2(0x3);
	write_cntvoff_el2(0);
	write_vpidr_el2(read_midr_el1());
	write_vmpidr_el2(read_mpidr_el1());
	write_vttbr_el2(0);
	write_mdcr_el2(0);
	write_hstr_el2(0);
	write_cnthp_ctl_el2(0);

	write_spsr_el3(BIT(8) | BIT(7) | BIT(6) | 0x5);
	write_elr_el3((u64) func);
	write_sp_el1(cpuid() ? stack_core1_el1 : stack_core0_el1);
	asm("eret\n");
}

static void write_range(size_t start, size_t end, u32 value)
{
	size_t reg;

	for (reg = start; reg < end; reg += 4)
		writel(value, reg);
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

static void reset_dvfs(void)
{
	u32 reg;

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

static inline u32 gic_rdist_addr(void)
{
	return 0xd1d40000 + 0x20000*cpuid();
}

static void gicr_waker_up(void)
{
	u32 reg = gic_rdist_addr() + 0x14;

	if (!(readl(reg) & BIT(1)))
		return;

	writel(readl(reg) & ~BIT(1), reg);
	while (readl(reg) & BIT(2))
		udelay(1000);
}

static void gicr_waker_down(void)
{
	u32 reg = gic_rdist_addr() + 0x14;

	if (readl(reg) & BIT(1))
		return;

	writel(readl(reg) | BIT(1), reg);
	while (!(readl(reg) & BIT(2)))
		udelay(1000);
}

static void local_irq_enable(void)
{
	asm("msr daifclr, #2\n");
	write_sysreg_s(0xe0, SYS_ICC_PMR_EL1);
}

static void reset_soc_irqs_core(void)
{
	u32 rdist;

	write_sysreg_s(read_sysreg_s(SYS_ICC_IGRPEN0_EL1) & ~BIT(0),
		       SYS_ICC_IGRPEN0_EL1);

	asm("isb\n");

	gicr_waker_down();
	rdist = gic_rdist_addr();

	writel(0, rdist + 0x10080);
	writel(0, rdist + 0x10c00);
	writel(0, rdist + 0x10c04);
	writel(0, rdist + 0x10d00);
	write_range(rdist + 0x10400, rdist + 0x1041c, 0);
	udelay(1000);
	writel(0xffffffff, rdist + 0x10180);
	udelay(1000);

	soc_ack_irqs();

	udelay(1000);
}

static void reset_soc_irqs(void)
{
	local_irq_enable();
	run_on_core1_wait(local_irq_enable);
	gicr_waker_up();
	run_on_core1_wait(gicr_waker_up);
	run_on_core1_wait(reset_soc_irqs_core);
	reset_soc_irqs_core();

	writel(0x0, 0xd1d00080);
	write_range(0xd1d00084, 0xd1d000fc, 0);
	writel(0xffff0000, 0xd1d00180);
	write_range(0xd1d00184, 0xd1d001fc, 0xffffffff);
	write_range(0xd1d00284, 0xd1d001fc, 0xffffffff);
	write_range(0xd1d00384, 0xd1d001fc, 0xffffffff);
	udelay(1000);
	writel(0x10, 0xd1d00000);
	udelay(1000);
	writel(0x0, 0xd1d00000);
	udelay(1000);
	writel(0x0, 0xd1d00000);
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
	reset_soc_irqs();
	reset_dvfs();
	write_reg_vals(reset_wdt_and_counters_regs);
}

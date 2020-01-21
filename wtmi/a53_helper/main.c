#include "io.h"

#define RWTM_CMD_PARAM(i)	(size_t)(0xd00b0000 + (i) * 4)
#define RWTM_CMD		0xd00b0040
#define RWTM_HOST_INT_RESET	0xd00b00c8
#define RWTM_HOST_INT_MASK	0xd00b00cc

static void udelay(u32 usec)
{
	u32 start, delta, total_delta;

	start = (u32)~read_cntpct_el0();
	total_delta = 25*usec/2 + 1U;
	do {
		delta = start - (u32)~read_cntpct_el0();
	} while (delta < total_delta);
}

const u64 stack_core0 = 0x10000000;
const u64 stack_core1 = 0x0f000000;

void main(void)
{
#if 0
	volatile u64 *regs = (volatile u64 *)0x11000000;
	int i = 0;

	setbitsl(RWTM_HOST_INT_MASK, 0, 1);
#define X(n)	regs[i++] = read_ ## n();
	CALL_FOR_EACH_REG(X)
#undef X
	write_sysreg_s(BIT(0), SYS_ICC_SRE_EL3);
	regs[i++] = read_sysreg_s(SYS_ICC_IAR1_EL1);
	regs[i++] = read_sysreg_s(SYS_ICC_CTLR_EL1);
	regs[i++] = read_sysreg_s(SYS_ICC_IGRPEN1_EL1);
	regs[i++] = read_sysreg_s(SYS_ICC_SRE_EL1);
	regs[i++] = read_sysreg_s(SYS_ICC_BPR1_EL1);
	regs[i++] = read_sysreg_s(SYS_ICC_PMR_EL1);
	regs[i++] = read_sysreg_s(SYS_ICC_RPR_EL1);
	u32 current_el;
	__asm__ __volatile__("mrs %0, CurrentEL\n\t" : "=r" (current_el) :  : "memory");
	regs[i++] = current_el;
	write_sysreg_s(0, SYS_ICC_SRE_EL3);

	writel(i, RWTM_CMD_PARAM(0));
	writel(0xf, RWTM_CMD);

	writel(0x200004, 0xd0018800);
	writel(0x200004, 0xd0018818);
#else
#define DO(n,v) write_ ## n(v);

	DO(scr_el3, 0x000000000000000eULL);
	DO(hcr_el2, 0x0000000000000002ULL);
	DO(vbar_el1, 0x0000000000000000ULL);
	DO(vbar_el2, 0x898cda2e0ca11400ULL);
	DO(vbar_el3, 0x00000000ffff0800ULL);
	DO(sctlr_el1, 0x0000000000c52838ULL);
	DO(sctlr_el2, 0x0000000030c50838ULL);
	DO(sctlr_el3, 0x0000000000c52838ULL);
	DO(actlr_el1, 0x0000000000000000ULL);
	DO(actlr_el2, 0x0000000000000000ULL);
	DO(actlr_el3, 0x0000000000000000ULL);
	DO(esr_el1, 0x0000000070234c12ULL);
	DO(esr_el2, 0x00000000ce86184fULL);
	DO(esr_el3, 0x000000005ba25004ULL);
	DO(afsr0_el1, 0x0000000000000000ULL);
	DO(afsr0_el2, 0x0000000000000000ULL);
	DO(afsr0_el3, 0x0000000000000000ULL);
	DO(afsr1_el1, 0x0000000000000000ULL);
	DO(afsr1_el2, 0x0000000000000000ULL);
	DO(afsr1_el3, 0x0000000000000000ULL);
	DO(far_el1, 0x06588fa2ac050151ULL);
	DO(far_el2, 0x142bd029540658f2ULL);
	DO(far_el3, 0x2227e255ecac63b9ULL);
	DO(mair_el1, 0x44e048e000098aa4ULL);
	DO(mair_el2, 0x3dd59dd2ed9bfcb1ULL);
	DO(mair_el3, 0x44e048e000098aa4ULL);
	DO(amair_el1, 0x0000000000000000ULL);
	DO(amair_el2, 0x0000000000000000ULL);
	DO(amair_el3, 0x0000000000000000ULL);
	DO(rmr_el3, 0x0000000000000001ULL);
	DO(tcr_el1, 0x0000000000000000ULL);
	DO(tcr_el2, 0x0000000080800000ULL);
	DO(tcr_el3, 0x0000000000000000ULL);
	DO(ttbr0_el1, 0xdf46019567f57649ULL);
	DO(ttbr0_el2, 0x000098c874811850ULL);
	DO(ttbr0_el3, 0x00020801768c645bULL);
	DO(ttbr1_el1, 0xff2082cc66851791ULL);
	DO(vttbr_el2, 0x000052b66fbf34f0ULL);
	DO(cptr_el2, 0x00000000000033ffULL);
	DO(cptr_el3, 0x0000000000000000ULL);
	DO(cpacr_el1, 0x0000000000000000ULL);
	DO(cntfrq_el0, 0x0000000000000000ULL);
	DO(cnthp_ctl_el2, 0x0000000000000002ULL);
	DO(cnthp_tval_el2, 0x000000001f1d5445ULL);
	DO(cnthp_cval_el2, 0x225fa7852285c290ULL);
	DO(cntps_ctl_el1, 0x0000000000000002ULL);
	DO(cntps_tval_el1, 0x00000000375c9665ULL);
	DO(cntps_cval_el1, 0x2464a1ce3ac504b2ULL);
	DO(cntp_ctl_el0, 0x0000000000000000ULL);
	DO(cntp_tval_el0, 0x00000000473a97d3ULL);
	DO(cntp_cval_el0, 0x360754834aa30620ULL);
	DO(cnthctl_el2, 0x0000000000000003ULL);
	DO(tpidr_el3, 0x4a2cdca2c3211a35ULL);
	DO(cntvoff_el2, 0x83a692fc408cb41bULL);
	DO(vpidr_el2, 0x00000000410fd034ULL);
	DO(vmpidr_el2, 0x0000000080000000ULL);
	DO(mdcr_el2, 0x0000000000000006ULL);
	DO(mdcr_el3, 0x0000000000000000ULL);
	DO(hstr_el2, 0x0000000000000000ULL);
	DO(pmcr_el0, 0x0000000041033000ULL);

	writel(0x200004, 0xd0018800);
	writel(0x200004, 0xd0018818);
#endif
	while (1);
}

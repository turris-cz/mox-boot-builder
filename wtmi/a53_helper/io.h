#ifndef A53_HELPER_IO_H
#define A53_HELPER_IO_H

#include <stdarg.h>
#include "types.h"
#include "sysreg.h"

static inline void isb(void)
{
	asm volatile("isb\n");
}

static inline void dsb(void)
{
	asm volatile("dsb sy\n");
}

static inline void nop(void)
{
	asm volatile("nop\n");
}

#define _DEFINE_SYSREG_READ_FUNC(_name, _reg_name)		\
static inline u_register_t read_ ## _name(void)			\
{								\
	u_register_t v;						\
	__asm__ volatile ("mrs %0, " #_reg_name : "=r" (v));	\
	return v;						\
}

#define _DEFINE_SYSREG_WRITE_FUNC(_name, _reg_name)			\
static inline void write_ ## _name(u_register_t v)			\
{									\
	__asm__ volatile ("msr " #_reg_name ", %0" : : "r" (v));	\
}

/* Define read & write function for system register */
#define DEFINE_SYSREG_RW_FUNCS(_name)			\
	_DEFINE_SYSREG_READ_FUNC(_name, _name)		\
	_DEFINE_SYSREG_WRITE_FUNC(_name, _name)

#define CALL_FOR_EACH_REG(M)	\
	M(scr_el3)		\
	M(hcr_el2)		\
	M(vbar_el1)		\
	M(vbar_el2)		\
	M(vbar_el3)		\
	M(sctlr_el1)		\
	M(sctlr_el2)		\
	M(sctlr_el3)		\
	M(actlr_el1)		\
	M(actlr_el2)		\
	M(actlr_el3)		\
	M(esr_el1)		\
	M(esr_el2)		\
	M(esr_el3)		\
	M(afsr0_el1)		\
	M(afsr0_el2)		\
	M(afsr0_el3)		\
	M(afsr1_el1)		\
	M(afsr1_el2)		\
	M(afsr1_el3)		\
	M(far_el1)		\
	M(far_el2)		\
	M(far_el3)		\
	M(mair_el1)		\
	M(mair_el2)		\
	M(mair_el3)		\
	M(midr_el1)		\
	M(amair_el1)		\
	M(amair_el2)		\
	M(amair_el3)		\
	M(rmr_el3)		\
	M(tcr_el1)		\
	M(tcr_el2)		\
	M(tcr_el3)		\
	M(ttbr0_el1)		\
	M(ttbr0_el2)		\
	M(ttbr0_el3)		\
	M(ttbr1_el1)		\
	M(vttbr_el2)		\
	M(cptr_el2)		\
	M(cptr_el3)		\
	M(cpacr_el1)		\
	M(cntfrq_el0)		\
	M(cnthp_ctl_el2)	\
	M(cnthp_tval_el2)	\
	M(cnthp_cval_el2)	\
	M(cntps_ctl_el1)	\
	M(cntps_tval_el1)	\
	M(cntps_cval_el1)	\
	M(cntp_ctl_el0)		\
	M(cntp_tval_el0)	\
	M(cntp_cval_el0)	\
	M(cntkctl_el1)		\
	M(cnthctl_el2)		\
	M(tpidr_el3)		\
	M(cntvoff_el2)		\
	M(vpidr_el2)		\
	M(vmpidr_el2)		\
	M(mdcr_el2)		\
	M(mdcr_el3)		\
	M(hstr_el2)		\
	M(pmcr_el0)		\
	M(spsr_el1)		\
	M(spsr_el3)		\
	M(elr_el1)		\
	M(elr_el3)		\
	M(sp_el1)		\
	M(sp_el2)		\
	M(daif)			\
	M(isr_el1)		\
	M(rvbar_el3)		\
	M(mpidr_el1)		\
	M(ccsidr_el1)		\
	M(clidr_el1)

CALL_FOR_EACH_REG(DEFINE_SYSREG_RW_FUNCS)

_DEFINE_SYSREG_READ_FUNC(cntpct_el0, cntpct_el0)

static inline u32 read_current_el(void)
{
	u32 current_el;
	asm("mrs %0, CurrentEL\n\t" : "=r" (current_el) :  : "memory");
	return current_el >> 2;
}

static inline u32 readl(size_t addr)
{
	return *(volatile u32 *)addr;
}

static inline void writel(u32 val, size_t addr)
{
	*(volatile u32 *)addr = val;
}

static inline void setbitsl(size_t addr, u32 val, u32 mask)
{
	writel((readl(addr) & ~mask) | (val & mask), addr);
}

static inline u16 readw(size_t addr)
{
	return *(volatile u16 *)addr;
}

static inline void writew(u16 val, size_t addr)
{
	*(volatile u16 *)addr = val;
}

static inline u8 readb(size_t addr)
{
	return *(volatile u8 *)addr;
}

static inline void writeb(u8 val, size_t addr)
{
	*(volatile u8 *)addr = val;
}

#endif /* !A53_HELPER_IO_H */

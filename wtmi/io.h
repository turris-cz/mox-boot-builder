#ifndef _IO_H_
#define _IO_H_

#define BIT(n)		(1UL << (n))

static inline u16 readw(u32 addr)
{
	u16 val;
	asm volatile("ldrh %0, %1"
		     : "=r" (val)
		     : "Q" (*(volatile u16 *)addr));
	return val;
}

static inline u32 readl(u32 addr)
{
	u32 val;
	asm volatile("ldr %0, %1"
		     : "=r" (val)
		     : "Qo" (*(volatile u32 *)addr));
	return val;
}

static inline void writew(u16 val, u32 addr)
{
	asm volatile("strh %1, %0"
		     : : "Q" (*(volatile u16 *)addr), "r" (val));
}

static inline void writel(u32 val, u32 addr)
{
	asm volatile("str %1, %0"
		     : : "Qo" (*(volatile u32 *)addr), "r" (val));
}

static inline void setbitsl(u32 addr, u32 val, u32 mask)
{
	writel((readl(addr) & ~mask) | (val & mask), addr);
}

#endif /* _IO_H_ */

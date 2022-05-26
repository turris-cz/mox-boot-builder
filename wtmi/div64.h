#ifndef _DIV64_H
#define _DIV64_H
/*
 * Copyright (C) 2003 Bernardo Innocenti <bernie@develer.com>
 * Based on former asm-ppc/div64.h and asm-m68knommu/div64.h
 *
 * Optimization for constant divisors on 32-bit machines:
 * Copyright (C) 2006-2015 Nicolas Pitre
 *
 * The semantics of do_div() are:
 *
 * u32 do_div(u64 *n, u32 base)
 * {
 *	u32 remainder = *n % base;
 *	*n = *n / base;
 *	return remainder;
 * }
 *
 * NOTE: macro parameter n is evaluated multiple times,
 *       beware of side effects!
 */

/**
 * is_power_of_2() - check if a value is a power of two
 * @n: the value to check
 *
 * Determine whether some value is a power of two, where zero is
 * *not* considered a power of two.
 * Return: true if @n is a power of 2, otherwise false.
 */
static inline __attribute__((const))
_Bool is_power_of_2(unsigned long n)
{
	return (n != 0 && ((n & (n - 1)) == 0));
}

/**
 * ilog2 - log base 2 of 32-bit or a 64-bit unsigned value
 * @n: parameter
 *
 * constant-capable log of base 2 calculation
 * - this can be used to initialise global variables from constant data, hence
 * the massive ternary operator construction
 *
 * selects the appropriately-sized optimised version depending on sizeof(n)
 */
#define ilog2(n)				\
(						\
	__builtin_constant_p(n) ? (		\
		(n) < 2 ? 0 :			\
		(n) & (1ULL << 63) ? 63 :	\
		(n) & (1ULL << 62) ? 62 :	\
		(n) & (1ULL << 61) ? 61 :	\
		(n) & (1ULL << 60) ? 60 :	\
		(n) & (1ULL << 59) ? 59 :	\
		(n) & (1ULL << 58) ? 58 :	\
		(n) & (1ULL << 57) ? 57 :	\
		(n) & (1ULL << 56) ? 56 :	\
		(n) & (1ULL << 55) ? 55 :	\
		(n) & (1ULL << 54) ? 54 :	\
		(n) & (1ULL << 53) ? 53 :	\
		(n) & (1ULL << 52) ? 52 :	\
		(n) & (1ULL << 51) ? 51 :	\
		(n) & (1ULL << 50) ? 50 :	\
		(n) & (1ULL << 49) ? 49 :	\
		(n) & (1ULL << 48) ? 48 :	\
		(n) & (1ULL << 47) ? 47 :	\
		(n) & (1ULL << 46) ? 46 :	\
		(n) & (1ULL << 45) ? 45 :	\
		(n) & (1ULL << 44) ? 44 :	\
		(n) & (1ULL << 43) ? 43 :	\
		(n) & (1ULL << 42) ? 42 :	\
		(n) & (1ULL << 41) ? 41 :	\
		(n) & (1ULL << 40) ? 40 :	\
		(n) & (1ULL << 39) ? 39 :	\
		(n) & (1ULL << 38) ? 38 :	\
		(n) & (1ULL << 37) ? 37 :	\
		(n) & (1ULL << 36) ? 36 :	\
		(n) & (1ULL << 35) ? 35 :	\
		(n) & (1ULL << 34) ? 34 :	\
		(n) & (1ULL << 33) ? 33 :	\
		(n) & (1ULL << 32) ? 32 :	\
		(n) & (1ULL << 31) ? 31 :	\
		(n) & (1ULL << 30) ? 30 :	\
		(n) & (1ULL << 29) ? 29 :	\
		(n) & (1ULL << 28) ? 28 :	\
		(n) & (1ULL << 27) ? 27 :	\
		(n) & (1ULL << 26) ? 26 :	\
		(n) & (1ULL << 25) ? 25 :	\
		(n) & (1ULL << 24) ? 24 :	\
		(n) & (1ULL << 23) ? 23 :	\
		(n) & (1ULL << 22) ? 22 :	\
		(n) & (1ULL << 21) ? 21 :	\
		(n) & (1ULL << 20) ? 20 :	\
		(n) & (1ULL << 19) ? 19 :	\
		(n) & (1ULL << 18) ? 18 :	\
		(n) & (1ULL << 17) ? 17 :	\
		(n) & (1ULL << 16) ? 16 :	\
		(n) & (1ULL << 15) ? 15 :	\
		(n) & (1ULL << 14) ? 14 :	\
		(n) & (1ULL << 13) ? 13 :	\
		(n) & (1ULL << 12) ? 12 :	\
		(n) & (1ULL << 11) ? 11 :	\
		(n) & (1ULL << 10) ? 10 :	\
		(n) & (1ULL <<  9) ?  9 :	\
		(n) & (1ULL <<  8) ?  8 :	\
		(n) & (1ULL <<  7) ?  7 :	\
		(n) & (1ULL <<  6) ?  6 :	\
		(n) & (1ULL <<  5) ?  5 :	\
		(n) & (1ULL <<  4) ?  4 :	\
		(n) & (1ULL <<  3) ?  3 :	\
		(n) & (1ULL <<  2) ?  2 :	\
		1) :				\
	(sizeof(n) <= 4) ?			\
	((sizeof(n)*8 - __builtin_clz(n))-1) :	\
	((sizeof(n)*8 - __builtin_clzll(n))-1)	\
 )

/*
 * If the divisor happens to be constant, we determine the appropriate
 * inverse at compile time to turn the division into a few inline
 * multiplications which ought to be much faster. And yet only if compiling
 * with a sufficiently recent gcc version to perform proper 64-bit constant
 * propagation.
 *
 * (It is unfortunate that gcc doesn't perform all this internally.)
 */

#define __div64_const32(n, ___b)					\
({									\
	/*								\
	 * Multiplication by reciprocal of b: n / b = n * (p / b) / p	\
	 *								\
	 * We rely on the fact that most of this code gets optimized	\
	 * away at compile time due to constant propagation and only	\
	 * a few multiplication instructions should remain.		\
	 * Hence this monstrous macro (static inline doesn't always	\
	 * do the trick here).						\
	 */								\
	u64 ___res, ___x, ___t, ___m, ___n = (n);			\
	u32 ___p, ___bias;						\
									\
	/* determine MSB of b */					\
	___p = 1 << ilog2(___b);					\
									\
	/* compute m = ((p << 64) + b - 1) / b */			\
	___m = (~0ULL / ___b) * ___p;					\
	___m += (((~0ULL % ___b + 1) * ___p) + ___b - 1) / ___b;	\
									\
	/* one less than the dividend with highest result */		\
	___x = ~0ULL / ___b * ___b - 1;					\
									\
	/* test our ___m with res = m * x / (p << 64) */		\
	___res = ((___m & 0xffffffff) * (___x & 0xffffffff)) >> 32;	\
	___t = ___res += (___m & 0xffffffff) * (___x >> 32);		\
	___res += (___x & 0xffffffff) * (___m >> 32);			\
	___t = (___res < ___t) ? (1ULL << 32) : 0;			\
	___res = (___res >> 32) + ___t;					\
	___res += (___m >> 32) * (___x >> 32);				\
	___res /= ___p;							\
									\
	/* Now sanitize and optimize what we've got. */			\
	if (~0ULL % (___b / (___b & -___b)) == 0) {			\
		/* special case, can be simplified to ... */		\
		___n /= (___b & -___b);					\
		___m = ~0ULL / (___b / (___b & -___b));			\
		___p = 1;						\
		___bias = 1;						\
	} else if (___res != ___x / ___b) {				\
		/*							\
		 * We can't get away without a bias to compensate	\
		 * for bit truncation errors.  To avoid it we'd need an	\
		 * additional bit to represent m which would overflow	\
		 * a 64-bit variable.					\
		 *							\
		 * Instead we do m = p / b and n / b = (n * m + m) / p.	\
		 */							\
		___bias = 1;						\
		/* Compute m = (p << 64) / b */				\
		___m = (~0ULL / ___b) * ___p;				\
		___m += ((~0ULL % ___b + 1) * ___p) / ___b;		\
	} else {							\
		/*							\
		 * Reduce m / p, and try to clear bit 31 of m when	\
		 * possible, otherwise that'll need extra overflow	\
		 * handling later.					\
		 */							\
		u32 ___bits = -(___m & -___m);				\
		___bits |= ___m >> 32;					\
		___bits = (~___bits) << 1;				\
		/*							\
		 * If ___bits == 0 then setting bit 31 is  unavoidable.	\
		 * Simply apply the maximum possible reduction in that	\
		 * case. Otherwise the MSB of ___bits indicates the	\
		 * best reduction we should apply.			\
		 */							\
		if (!___bits) {						\
			___p /= (___m & -___m);				\
			___m /= (___m & -___m);				\
		} else {						\
			___p >>= ilog2(___bits);			\
			___m >>= ilog2(___bits);			\
		}							\
		/* No bias needed. */					\
		___bias = 0;						\
	}								\
									\
	/*								\
	 * Now we have a combination of 2 conditions:			\
	 *								\
	 * 1) whether or not we need to apply a bias, and		\
	 *								\
	 * 2) whether or not there might be an overflow in the cross	\
	 *    product determined by (___m & ((1 << 63) | (1 << 31))).	\
	 *								\
	 * Select the best way to do (m_bias + m * n) / (1 << 64).	\
	 * From now on there will be actual runtime code generated.	\
	 */								\
	___res = __arch_xprod_64(___m, ___n, ___bias);			\
									\
	___res /= ___p;							\
})

/*
 * Default C implementation for __arch_xprod_64()
 *
 * Prototype: u64 __arch_xprod_64(const u64 m, u64 n, bool bias)
 * Semantic:  retval = ((bias ? m : 0) + m * n) >> 64
 *
 * The product is a 128-bit value, scaled down to 64 bits.
 * Assuming constant propagation to optimize away unused conditional code.
 * Architectures may provide their own optimized assembly implementation.
 */
static inline u64 __arch_xprod_64(const u64 m, u64 n, _Bool bias)
{
	u32 m_lo = m;
	u32 m_hi = m >> 32;
	u32 n_lo = n;
	u32 n_hi = n >> 32;
	u64 res, tmp;

	if (!bias) {
		res = ((u64)m_lo * n_lo) >> 32;
	} else if (!(m & ((1ULL << 63) | (1ULL << 31)))) {
		/* there can't be any overflow here */
		res = (m + (u64)m_lo * n_lo) >> 32;
	} else {
		res = m + (u64)m_lo * n_lo;
		tmp = (res < m) ? (1ULL << 32) : 0;
		res = (res >> 32) + tmp;
	}

	if (!(m & ((1ULL << 63) | (1ULL << 31)))) {
		/* there can't be any overflow here */
		res += (u64)m_lo * n_hi;
		res += (u64)m_hi * n_lo;
		res >>= 32;
	} else {
		tmp = res += (u64)m_lo * n_hi;
		res += (u64)m_hi * n_lo;
		tmp = (res < tmp) ? (1ULL << 32) : 0;
		res = (res >> 32) + tmp;
	}

	res += (u64)m_hi * n_hi;

	return res;
}

extern u32 __div64_32(u64 *dividend, u32 divisor)
__attribute__((error("do_div() with non-constant base is not implemented")));

/* The unnecessary pointer compare is there
 * to check for type safety (n must be 64bit)
 */
# define do_div(n,base) ({				\
	u32 __base = (base);			\
	u32 __rem;					\
	(void)(((typeof((n)) *)0) == ((u64 *)0));	\
	if (__builtin_constant_p(__base) &&		\
	    is_power_of_2(__base)) {			\
		__rem = (n) & (__base - 1);		\
		(n) >>= ilog2(__base);			\
	} else if (__builtin_constant_p(__base) &&	\
		   __base != 0) {			\
		u32 __res_lo, __n_lo = (n);	\
		(n) = __div64_const32(n, __base);	\
		/* the remainder can be computed with 32-bit regs */ \
		__res_lo = (n);				\
		__rem = __n_lo - __res_lo * __base;	\
	} else						\
		__rem = __div64_32(&(n), __base);	\
	__rem;						\
 })

/* Wrapper for do_div(). Doesn't modify dividend and returns
 * the result, not remainder.
 */
static inline u64 lldiv(u64 dividend, u32 divisor)
{
	u64 __res = dividend;
	do_div(__res, divisor);
	return(__res);
}

#endif

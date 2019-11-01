#include "errno.h"
#include "types.h"
#include "ebg.h"
#include "io.h"
#include "clock.h"
#include "efuse.h"
#include "crypto.h"
#include "debug.h"

#define AIB_CTRL	0x40000c00
#define SP_CTRL		0x40001c00
#define SP_STATUS	0x40001c04

#define ZMODP_CONF		0x40002000
#define ZMODP_CONF_OP_MONT	0x0
#define ZMODP_CONF_OP_MUL	0x1
#define ZMODP_CONF_OP_EXP	0x2
#define ZMODP_CONF_OP_INV	0x3
#define ZMODP_CONF_OP_PRE	0x4
#define ZMODP_CONF_OP_MASK	0x7
#define ZMODP_CONF_SZ(b) ({			\
	int __t = 25 - __builtin_clz((b) - 1);	\
	if (__t < 0)				\
		__t = 0;			\
	__t <<= 3;				\
	__t; })
#define ZMODP_CONF_SZ_128	(0x0 << 3)
#define ZMODP_CONF_SZ_256	(0x1 << 3)
#define ZMODP_CONF_SZ_512	(0x2 << 3)
#define ZMODP_CONF_SZ_1024	(0x3 << 3)
#define ZMODP_CONF_SZ_2048	(0x4 << 3)
#define ZMODP_CONF_SZ_MASK	(0x7 << 3)
#define ZMODP_CONF_SZ_SHIFT	3
#define ZMODP_CONF_COEFS(x)	(((x) & 0x7f) << 6)
#define ZMODP_CONF_COEFS_MASK	(0x7f << 6)
#define ZMODP_CONF_SECURE	BIT(13)
#define ZMODP_CONF_X_FROM_OTP	BIT(15)
#define ZMODP_CTRL		0x40002004
#define ZMODP_CMD		0x40002008
#define ZMODP_CMD_ZEROIZE	BIT(0)
#define ZMODP_CMD_START		BIT(1)
#define ZMODP_STATUS		0x4000200c
#define ZMODP_INT		0x40002010
#define ZMODP_INTMASK		0x40002014
#define ZMODP_X(i)		(0x40002018 + 4 * (i))
#define ZMODP_X1(i)		(0x40002118 + 4 * (i))
#define ZMODP_EXP(i)		(0x40002218 + 4 * (i))
#define ZMODP_MODULI		0x40002318
#define ZMODP_Y			0x4000231c
#define ZMODP_Z			0x40002320

#define ECP_CONF		0x40001400
#define ECP_CONF_FLD_224	0x0
#define ECP_CONF_FLD_256	0x1
#define ECP_CONF_FLD_384	0x2
#define ECP_CONF_FLD_521	0x3
#define ECP_CONF_FLD_MASK	0x3
#define ECP_CONF_OP_MUL		(0x0 << 2)
#define ECP_CONF_OP_ADD		(0x1 << 2)
#define ECP_CONF_OP_SUB		(0x2 << 2)
#define ECP_CONF_OP_DBL		(0x3 << 2)
#define ECP_CONF_OP_INV		(0x4 << 2)
#define ECP_CONF_OP_ZERO	(0x5 << 2)
#define ECP_CONF_OP_MASK	(0x7 << 2)
#define ECP_CONF_KEY_FROM_OTP	BIT(5)
#define ECP_CTRL		0x40001404
#define ECP_CMD			0x40001408
#define ECP_INT			0x4000140c
#define ECP_INT_DONE		BIT(0)
#define ECP_INT_INVALID		BIT(1)
#define ECP_INT_ZERO_KEY	BIT(2)
#define ECP_INT_ZERO_OUTPUT	BIT(3)
#define ECP_INT_CAL_ZERO_INV	BIT(4)
#define ECP_STATUS		0x40001410
#define ECP_INTMASK		0x40001414
#define ECP_PARAM_A		((u32 *) 0x40001418)
#define ECP_PARAM_B		((u32 *) 0x4000145c)
#define ECP_OP1_X		((u32 *) 0x400014a0)
#define ECP_OP1_Y		((u32 *) 0x400014e4)
#define ECP_OP2_X		((u32 *) 0x40001528)
#define ECP_OP2_Y		((u32 *) 0x4000156c)
#define ECP_RES_X		((u32 *) 0x400015b0)
#define ECP_RES_Y		((u32 *) 0x400015f4)

static const ec_info_t secp521r1 = {
	.bits = 521,
	.prime = {
		.p = {
			0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			0x000001ff
		},
		.r = { 0, 0x4000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
	},
	.curve = {
		.a = {
			0xfffffffc, 0xffffffff, 0xffffffff, 0xffffffff,
			0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			0x000001ff
		},
		.b = {
			0x6b503f00, 0xef451fd4, 0x3d2c34f1, 0x3573df88,
			0x3bb1bf07, 0x1652c0bd, 0xec7e937b, 0x56193951,
			0x8ef109e1, 0xb8b48991, 0x99b315f3, 0xa2da725b,
			0xb68540ee, 0x929a21a0, 0x8e1c9a1f, 0x953eb961,
			0x00000051,
		}
	},
	.base = {
		.x = {
			0xc2e5bd66, 0xf97e7e31, 0x856a429b, 0x3348b3c1,
			0xa2ffa8de, 0xfe1dc127, 0xefe75928, 0xa14b5e77,
			0x6b4d3dba, 0xf828af60, 0x053fb521, 0x9c648139,
			0x2395b442, 0x9e3ecb66, 0x0404e9cd, 0x858e06b7,
			0x000000c6
		},
		.y = {
			0x9fd16650, 0x88be9476, 0xa272c240, 0x353c7086,
			0x3fad0761, 0xc550b901, 0x5ef42640, 0x97ee7299,
			0x273e662c, 0x17afbd17, 0x579b4468, 0x98f54449,
			0x2c7d1bd9, 0x5c8a5fb4, 0x9a3bc004, 0x39296a78,
			0x00000118
		}
	},
	.order = {
		.p = {
			0x91386409, 0xbb6fb71e, 0x899c47ae, 0x3bb5c9b8,
			0xf709a5d0, 0x7fcc0148, 0xbf2f966b, 0x51868783,
			0xfffffffa, 0xffffffff, 0xffffffff, 0xffffffff,
			0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			0x000001ff,
		},
		.r = {
			0x61c64ca7, 0x1163115a, 0x4374a642, 0x18354a56,
			0x0791d9dc, 0x5d4dd6d3, 0xd3402705, 0x4fb35b72,
			0xb7756e3a, 0xcff3d142, 0xa8e567bc, 0x5bcc6d61,
			0x492d0d45, 0x2d8e03d1, 0x8c44383d, 0x5b5a3afe,
			0x0000019a
		},
	}
};

static inline void bn_copy(u32 *dst, const u32 *src)
{
	int i;

	for (i = 0; i < 17; ++i)
		dst[i] = src[i];
}

static inline void bn_from_u32(u32 *dst, u32 src)
{
	int i;

	dst[0] = src;
	for (i = 1; i < 17; ++i)
		dst[i] = 0;
}

static inline void bn_print(const u32 *x)
{
	int i;

	for (i = 0; i < 17; ++i)
		printf("%08x%c", x[16 - i],
		       (i == 16 || (i % 6) == 5) ? '\n' : ' ');
}

static int bn_cmp(const u32 *a, const u32 *b)
{
	int i;

	for (i = 16; i >= 0; --i) {
		if (a[i] < b[i])
			return -1;
		else if (a[i] > b[i])
			return 1;
	}

	return 0;
}

static int bn_is_zero(const u32 *x)
{
	int i;

	for (i = 0; i < 17; ++i)
		if (x[i] != 0)
			return 0;

	return 1;
}

static void bn_random(u32 *dst, const u32 *max, int bits)
{
	int longs;

	longs = (bits + 31) / 32;

	do {
		ebg_rand_sync(dst, 4 * longs);
		dst[longs - 1] &= (1UL << (bits % 32)) - 1UL;
	} while (bn_cmp(dst, max) >= 0);
}

static void bn_modulo(u32 *dst, const u32 *mod)
{
	while (bn_cmp(dst, mod) >= 0) {
		int i, c1, c2, carry;

		carry = 0;
		for (i = 0; i < 17; ++i) {
			c1 = __builtin_usub_overflow(dst[i], mod[i], &dst[i]);
			c2 = __builtin_usub_overflow(dst[i], carry, &dst[i]);
			carry = c1 || c2;
		}
	}
}

int bn_add(u32 *dst, const u32 *src, int len)
{
	int i, c1, c2, carry;

	carry = 0;
	for (i = 0; i < len; ++i) {
		c1 = __builtin_uadd_overflow(dst[i], carry, &dst[i]);
		c2 = __builtin_uadd_overflow(dst[i], src[i], &dst[i]);
		carry = c1 || c2;
	}

	return carry;
}

static void bn_addmod(u32 *dst, const u32 *src, const u32 *mod)
{
	int i, c1, c2, carry;

	carry = 0;
	for (i = 0; i < 17; ++i) {
		c1 = __builtin_uadd_overflow(dst[i], carry, &dst[i]);
		c2 = __builtin_uadd_overflow(dst[i], src[i], &dst[i]);
		carry = c1 || c2;
	}

	bn_modulo(dst, mod);
}

static inline u32 ecp_wait(void)
{
	u32 val;

	while (!(val = readl(ECP_INT)))
		wait_ns(100);
	writel(val, ECP_INT);

//	printf("ECP_INT = %x\n", val);

	return val;
}

static inline void ecp_init(void)
{
	writel(0x250, AIB_CTRL);
	setbitsl(SP_CTRL, 0x10, 0x30);
	writel(ECP_CONF_FLD_521 | ECP_CONF_OP_ZERO, ECP_CONF);
	writel(0x1, ECP_CMD);
	ecp_wait();
}

static void ecp_secp521r1_init(void)
{
	ecp_init();
	bn_copy(ECP_PARAM_A, secp521r1.curve.a);
	bn_copy(ECP_PARAM_B, secp521r1.curve.b);
}

static void ecp_clear_operands(void)
{
	bn_from_u32(ECP_OP1_X, 0);
	bn_from_u32(ECP_OP1_Y, 0);
	bn_from_u32(ECP_OP2_X, 0);
	bn_from_u32(ECP_OP2_Y, 0);
}

static int ecp_secp521r1_add(ec_point_t *r, const ec_point_t *a,
			     const ec_point_t *b)
{
	ecp_secp521r1_init();
	bn_copy(ECP_OP1_X, a->x);
	bn_copy(ECP_OP1_Y, a->y);
	bn_copy(ECP_OP2_X, b->x);
	bn_copy(ECP_OP2_Y, b->y);
	writel(ECP_CONF_FLD_521 | ECP_CONF_OP_ADD, ECP_CONF);
	writel(0x1, ECP_CMD);

	if (ecp_wait() & (ECP_INT_ZERO_OUTPUT | ECP_INT_CAL_ZERO_INV))
		return -EDOM;

	if (r) {
		bn_copy(r->x, ECP_RES_X);
		bn_copy(r->y, ECP_RES_Y);
	}

	return 0;
}

static int ecp_secp521r1_point_mul(ec_point_t *r, const ec_point_t *a,
				   const u32 *scalar)
{
	ecp_secp521r1_init();
	bn_copy(ECP_OP1_X, a->x);
	bn_copy(ECP_OP1_Y, a->y);
	bn_copy(ECP_OP2_X, scalar);
	bn_copy(ECP_OP2_Y, scalar);
	writel(ECP_CONF_FLD_521 | ECP_CONF_OP_MUL, ECP_CONF);
	writel(0x1, ECP_CMD);

	if (ecp_wait() & (ECP_INT_ZERO_OUTPUT | ECP_INT_CAL_ZERO_INV))
		return -EDOM;

	if (r) {
		bn_copy(r->x, ECP_RES_X);
		bn_copy(r->y, ECP_RES_Y);
	}

	return 0;
}

static int ecp_secp521r1_point_mul_by_otp(ec_point_t *r, const ec_point_t *a)
{
	int res;

	res = efuse_read_secure_buffer();
	if (res < 0)
		return res;

	ecp_secp521r1_init();
	bn_copy(ECP_OP1_X, a->x);
	bn_copy(ECP_OP1_Y, a->y);
	bn_from_u32(ECP_OP2_X, 1);
	bn_from_u32(ECP_OP2_Y, 1);
	writel(ECP_CONF_FLD_521 | ECP_CONF_OP_MUL | ECP_CONF_KEY_FROM_OTP,
	       ECP_CONF);
	writel(0x1, ECP_CMD);

	if (ecp_wait() & (ECP_INT_ZERO_OUTPUT | ECP_INT_CAL_ZERO_INV))
		return -EDOM;

	if (r) {
		bn_copy(r->x, ECP_RES_X);
		bn_copy(r->y, ECP_RES_Y);
	}

	return 0;
}

static inline u32 zmodp_wait(void)
{
	u32 val;

	while (!(val = readl(ZMODP_INT)))
		wait_ns(100);
	writel(val, ZMODP_INT);

//	printf("ZMODP_INT = %x\n", val);

	return val;
}

void zmodp_zeroize(void)
{
	writel(ZMODP_CMD_ZEROIZE, ZMODP_CMD);
	zmodp_wait();
}

static int zmodp_longs, zmodp_pad;

static int zmodp_set_size(int bits)
{
	if (bits > 2048)
		return -EINVAL;

	zmodp_longs = (bits + 31) / 32;

	if (bits < 128)
		bits = 128;

	zmodp_pad = 1 << (27 - __builtin_clz(bits - 1));

	setbitsl(ZMODP_CONF,
		 ZMODP_CONF_SZ(bits) | ZMODP_CONF_COEFS(zmodp_longs),
		 ZMODP_CONF_SZ_MASK | ZMODP_CONF_COEFS_MASK);

	return 0;
}

static const prime_t *zmodp_prime;

static void zmodp_set_prime(const prime_t *prime)
{
	zmodp_prime = prime;
}

enum zmodp_flags {
	X_FROM_OTP = BIT(0),
	CLEAR_X = BIT(1)
};

static inline int zmodp_op(u32 op, u32 *res, const u32 *x, const u32 *y,
			   enum zmodp_flags flags)
{
	int i;
	u32 reg;

	reg = ZMODP_CONF_SECURE | (op & ZMODP_CONF_OP_MASK);
	if (flags & X_FROM_OTP) {
		int res;

		if (op != ZMODP_CONF_OP_MUL && op != ZMODP_CONF_OP_MONT)
			return -EINVAL;

		res = efuse_read_secure_buffer();
		if (res < 0)
			return res;

		reg |= ZMODP_CONF_X_FROM_OTP;
	}

	writel(0x260, AIB_CTRL);
	setbitsl(SP_CTRL, 0x10, 0x30);
	setbitsl(ZMODP_CONF, reg,
		 ZMODP_CONF_SECURE | ZMODP_CONF_OP_MASK |
		 ZMODP_CONF_X_FROM_OTP);

#define copy_words(dst, src)				\
	do {						\
		for (i = 0; i < zmodp_longs; ++i)	\
			writel((src)[i], (dst));	\
		for (; i < zmodp_pad; ++i)		\
			writel(0, (dst));		\
	} while (0)

	copy_words(ZMODP_MODULI, zmodp_prime->p);

	if (op != ZMODP_CONF_OP_PRE) {
		if (!(flags & X_FROM_OTP))
			copy_words(ZMODP_X(i), x);

		if (op == ZMODP_CONF_OP_EXP || op == ZMODP_CONF_OP_INV) {
			copy_words(ZMODP_Y, zmodp_prime->r);
		} else {
			copy_words(ZMODP_X1(i), zmodp_prime->r);
			copy_words(ZMODP_Y, y);
		}

		if (op == ZMODP_CONF_OP_EXP)
			copy_words(ZMODP_EXP(i), y);
	}
#undef copy_words

	writel(ZMODP_CMD_START, ZMODP_CMD);
	zmodp_wait();

	if (flags & CLEAR_X) {
		for (i = 0; i < zmodp_longs; ++i)
			writel(0, ZMODP_X(i));
	}

	for (i = 0; i < zmodp_longs; ++i)
		res[i] = readl(ZMODP_Z);
	for (; i < zmodp_pad; ++i)
		readl(ZMODP_Z);

	return 0;
}

static int ecp_secp521r1_point_valid(const ec_point_t *p)
{
	int res;
	u32 r[17], t[17];

	zmodp_set_size(521);
	zmodp_set_prime(&secp521r1.prime);

	bn_from_u32(t, 3);
	res = zmodp_op(ZMODP_CONF_OP_EXP, r, p->x, t, 0);
	if (res < 0)
		return res;

	res = zmodp_op(ZMODP_CONF_OP_MUL, t, p->x, secp521r1.curve.a, 0);
	if (res < 0)
		return res;

	bn_addmod(r, t, secp521r1.prime.p);
	bn_addmod(r, secp521r1.curve.b, secp521r1.prime.p);

	res = zmodp_op(ZMODP_CONF_OP_MUL, t, p->y, p->y, 0);
	if (res < 0)
		return res;

	return !bn_cmp(r, t);
}

int ecdsa_generate_efuse_private_key(void)
{
	int res;
	u32 priv[17];

	do
		bn_random(priv, secp521r1.order.p, 521);
	while (bn_is_zero(priv));

	res = efuse_write_secure_buffer(priv);

	bn_from_u32(priv, 0);

	return res;
}

int ecdsa_sign(ec_sig_t *sig, const u32 *z)
{
	int res;
	ec_point_t p;
	u32 k[17];

	/* is message too long */
	if (z[16] > 0x1ff)
		return -EINVAL;

	zmodp_set_size(secp521r1.bits);
	zmodp_set_prime(&secp521r1.order);

	do {
		do {
			do
				bn_random(k, secp521r1.order.p, 521);
			while (bn_is_zero(k));

			ecp_secp521r1_point_mul(&p, &secp521r1.base, k);
			ecp_clear_operands();

			bn_copy(sig->r, p.x);
			bn_modulo(sig->r, secp521r1.order.p);
		} while (bn_is_zero(sig->r));

		res = zmodp_op(ZMODP_CONF_OP_MUL, sig->s, NULL, sig->r,
			       X_FROM_OTP);
		if (res < 0)
			return res;

		res = zmodp_op(ZMODP_CONF_OP_INV, k, k, NULL, CLEAR_X);
		if (res < 0)
			return res;

		bn_addmod(sig->s, z, secp521r1.order.p);

		res = zmodp_op(ZMODP_CONF_OP_MUL, sig->s, k, sig->s, CLEAR_X);
		if (res < 0)
			return res;
	} while (bn_is_zero(sig->s));

	bn_from_u32(k, 0);

	return 0;
}

static inline int ecdsa_valid_scalar(const u32 *x)
{
	return !bn_is_zero(x) && bn_cmp(x, secp521r1.order.p) < 0;
}

int ecdsa_verify(const ec_point_t *pub, const ec_sig_t *sig, const u32 *z)
{
	int res;
	u32 w[17], u[17];
	ec_point_t a, b;

	/* is message too long */
	if (z[16] > 0x1ff)
		return 0;

	/* does the point lie on the curve? */
	if (!ecp_secp521r1_point_valid(pub))
		return 0;

	if (!ecp_secp521r1_point_mul(NULL, pub, secp521r1.order.p))
		return 0;

	if (!ecdsa_valid_scalar(sig->r) || !ecdsa_valid_scalar(sig->s))
		return 0;

	zmodp_set_size(secp521r1.bits);
	zmodp_set_prime(&secp521r1.order);

	res = zmodp_op(ZMODP_CONF_OP_INV, w, sig->s, NULL, 0);
	if (res < 0)
		return 0;

	res = zmodp_op(ZMODP_CONF_OP_MUL, u, z, w, 0);
	if (res < 0)
		return 0;

	if (ecp_secp521r1_point_mul(&a, &secp521r1.base, u) < 0)
		return 0;

	res = zmodp_op(ZMODP_CONF_OP_MUL, u, sig->r, w, 0);
	if (res < 0)
		return 0;

	if (ecp_secp521r1_point_mul(&b, pub, u) < 0)
		return 0;

	if (ecp_secp521r1_add(&a, &a, &b) < 0)
		return 0;

	return bn_cmp(a.x, sig->r) == 0;
}

int ecdsa_get_efuse_public_key(u32 *compressed_pub)
{
	int res, i;
	ec_point_t pub;

	res = ecp_secp521r1_point_mul_by_otp(&pub, &secp521r1.base);
	if (res < 0)
		return res;

	if (bn_is_zero(pub.x))
		return -ENODATA;

	for (i = 0; i < 17; ++i)
		compressed_pub[i] = pub.x[16 - i];

	compressed_pub[0] |= (pub.y[0] & 1) ? 0x30000 : 0x20000;

	return 0;
}

#if 1
DECL_DEBUG_CMD(cmd_ecdsa)
{
	ec_point_t pub;
	ec_sig_t sig;
	u32 z[17];

	bn_random(z, secp521r1.order.p, 521);

	printf("Message:\n");
	bn_print(z);
	printf("\n");

	ecdsa_sign(&sig, z);
	printf("Signature:\n");
	bn_print(sig.r);
	bn_print(sig.s);
	printf("\n");

	ecp_secp521r1_point_mul_by_otp(&pub, &secp521r1.base);
	printf("Public key:\n");
	bn_print(pub.x);
	bn_print(pub.y);
	printf("\n");

	printf("Verification status: %d\n", ecdsa_verify(&pub, &sig, z));
}

DEBUG_CMD("ecdsa", "Test ECDSA cryptographic engine", cmd_ecdsa);
#endif

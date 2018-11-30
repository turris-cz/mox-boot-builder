#ifndef _CRYPTO_H_
#define _CRYPTO_H_

#include "types.h"

typedef struct {
	u32 p[17];
	u32 r[17];
} prime_t;

typedef struct {
	u32 a[17];
	u32 b[17];
} ec_curve_t;

typedef struct {
	u32 x[17];
	u32 y[17];
} ec_point_t;

typedef struct {
	u32 r[17];
	u32 s[17];
} ec_sig_t;

typedef struct {
	int bits;
	prime_t prime;
	ec_curve_t curve;
	ec_point_t base;
	prime_t order;
} ec_info_t;

extern int bn_add(u32 *dst, const u32 *src, int len);
extern int ecdsa_sign(ec_sig_t *sig, const u32 *z);
extern int ecdsa_verify(const ec_point_t *pub, const ec_sig_t *sig,
			const u32 *z);
extern int ecdsa_generate_efuse_private_key(void);
extern int ecdsa_get_efuse_public_key(u32 *compressed_pub);
extern void test_ecp(void);

#endif /* _CRYPTO_H_ */

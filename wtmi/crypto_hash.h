#ifndef _CRYPTO_HASH_H_
#define _CRYPTO_HASH_H_

#include "types.h"

typedef void (*hw_hash_fnc_t)(const void *, u32, u32 *);

extern void hw_md5(const void *msg, u32 size, u32 *digest);
extern void hw_sha1(const void *msg, u32 size, u32 *digest);
extern void hw_sha224(const void *msg, u32 size, u32 *digest);
extern void hw_sha256(const void *msg, u32 size, u32 *digest);
extern void hw_sha384(const void *msg, u32 size, u32 *digest);
extern void hw_sha512(const void *msg, u32 size, u32 *digest);
extern void test_hash(void);

#endif /* _CRYPTO_HASH_H_ */

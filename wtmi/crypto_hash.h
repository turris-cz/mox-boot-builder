#ifndef _CRYPTO_HASH_H_
#define _CRYPTO_HASH_H_

#include "types.h"
#include "string.h"

typedef void (*hw_hash_fnc_t)(const void *, u32, u32 *);

extern void hw_md5(const void *msg, u32 size, u32 *digest);
extern void hw_sha1(const void *msg, u32 size, u32 *digest);
extern void hw_sha224(const void *msg, u32 size, u32 *digest);
extern void hw_sha256(const void *msg, u32 size, u32 *digest);
extern void hw_sha384(const void *msg, u32 size, u32 *digest);
extern void hw_sha512(const void *msg, u32 size, u32 *digest);

enum {
	HASH_NA = 0,
	HASH_MD5,
	HASH_SHA1,
	HASH_SHA224,
	HASH_SHA256,
	HASH_SHA384,
	HASH_SHA512,
};

static inline int hash_id(const char *name)
{
	if (!strcmp(name, "md5"))
		return HASH_MD5;
	else if (!strcmp(name, "sha1"))
		return HASH_SHA1;
	else if (!strcmp(name, "sha224"))
		return HASH_SHA224;
	else if (!strcmp(name, "sha256"))
		return HASH_SHA256;
	else if (!strcmp(name, "sha384"))
		return HASH_SHA384;
	else if (!strcmp(name, "sha512"))
		return HASH_SHA512;
	else
		return HASH_NA;
}

static inline int hw_hash(int h, const void *msg, u32 size, u32 *digest)
{
	switch (h) {
	case HASH_MD5:
		hw_md5(msg, size, digest);
		return 4;
	case HASH_SHA1:
		hw_sha1(msg, size, digest);
		return 5;
	case HASH_SHA224:
		hw_sha224(msg, size, digest);
		return 7;
	case HASH_SHA256:
		hw_sha256(msg, size, digest);
		return 8;
	case HASH_SHA384:
		hw_sha384(msg, size, digest);
		return 12;
	case HASH_SHA512:
		hw_sha512(msg, size, digest);
		return 16;
	default:
		return -1;
	}
}

#endif /* _CRYPTO_HASH_H_ */

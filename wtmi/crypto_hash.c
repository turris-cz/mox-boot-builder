#include "errno.h"
#include "types.h"
#include "printf.h"
#include "io.h"
#include "clock.h"
#include "crypto_hash.h"
#include "crypto_dma.h"
#include "debug.h"

#define TEST_HASH	0

#define AIB_CTRL	0x40000c00
#define SP_CTRL		0x40001c00
#define SP_STATUS	0x40001c04

#define HASH_CONF		0x40001800
#define HASH_CONF_AES_COMBINE	BIT(4)
#define HASH_CONF_MODE_HMAC	BIT(3)
#define HASH_CONF_ALG_SHA1	0x0
#define HASH_CONF_ALG_SHA256	0x1
#define HASH_CONF_ALG_SHA224	0x2
#define HASH_CONF_ALG_MD5	0x3
#define HASH_CONF_ALG_SHA512	0x4
#define HASH_CONF_ALG_SHA384	0x5
#define HASH_CONF_ALG_MASK	0x7
#define HASH_CTRL		0x40001804
#define HASH_CTRL_RESET		BIT(3)
#define HASH_CTRL_PADDING	BIT(2)
#define HASH_CTRL_OP_INIT	0x1
#define HASH_CTRL_OP_UPDATE	0x2
#define HASH_CTRL_OP_FINAL	0x3
#define HASH_CTRL_OP_MASK	0x3
#define HASH_CMD		0x40001808
#define HASH_CMD_START		BIT(0)
#define HASH_STATUS		0x4000180c
#define HASH_STATUS_DONE	BIT(0)
#define HASH_STATUS_BUSY	BIT(1)
#define HASH_STATUS_XFER_BUSY	BIT(2)
#define HASH_STATUS_HMAC_BUSY	BIT(3)
#define HASH_MSG_SEG_SZ		0x40001810
#define HASH_MSG_SZ_L		0x40001818
#define HASH_MSG_SZ_H		0x4000181c
#define HASH_DIGEST(i)		(0x40001820 + 4 * (i))
#define HASH_DIGEST_H(i)	(0x40001840 + 4 * (i))
#define HASH_HMAC_CTX(i)	(0x40001860 + 4 * (i))
#define HASH_HMAC_CTX_H(i)	(0x40001880 + 4 * (i))
#define HASH_HMAC_KEY_LEN	0x400018a0
#define HASH_HMAC_KEY(i)	(0x400018a4 + 4 * (i))

static inline void hash_wait(void)
{
	while (readl(HASH_STATUS) != HASH_STATUS_DONE)
		wait_ns(100);
}

static void hash_init(u32 alg, u32 size)
{
	writel(0x205, AIB_CTRL);
	setbitsl(SP_CTRL, 0x00, 0x30);
	writel(HASH_CTRL_RESET, HASH_CTRL);
	writel(HASH_CTRL_PADDING, HASH_CTRL);
	writel(alg & HASH_CONF_ALG_MASK, HASH_CONF);
	writel(size, HASH_MSG_SZ_L);
	writel(0, HASH_MSG_SZ_H);

	setbitsl(HASH_CTRL, HASH_CTRL_OP_INIT, HASH_CTRL_OP_MASK);
	writel(HASH_CMD_START, HASH_CMD);
	hash_wait();
}

static void hash_update(const void *data, u32 size, int final)
{
	dma_input_enable(data, size);
	writel(size, HASH_MSG_SEG_SZ);
	if (final) {
		setbitsl(HASH_CTRL, HASH_CTRL_PADDING, HASH_CTRL_PADDING);
		setbitsl(HASH_CTRL, HASH_CTRL_OP_FINAL, HASH_CTRL_OP_MASK);
	} else {
		setbitsl(HASH_CTRL, 0, HASH_CTRL_PADDING);
		setbitsl(HASH_CTRL, HASH_CTRL_OP_UPDATE, HASH_CTRL_OP_MASK);
	}
	writel(HASH_CMD_START, HASH_CMD);
	hash_wait();
	dma_input_disable();
}

static void hash_final(u32 *digest, int d)
{
	int i;

	if (d > 8) {
		int j;

		for (i = j = 0; i < d; i += 2, ++j) {
			digest[i] = readl(HASH_DIGEST_H(j));
			digest[i + 1] = readl(HASH_DIGEST(j));
		}
	} else {
		for (i = 0; i < d; ++i)
			digest[i] = readl(HASH_DIGEST(i));
	}
}

void hw_md5(const void *msg, u32 size, u32 *digest)
{
	hash_init(HASH_CONF_ALG_MD5, size);
	hash_update(msg, size, 1);
	hash_final(digest, 4);
}

void hw_sha1(const void *msg, u32 size, u32 *digest)
{
	hash_init(HASH_CONF_ALG_SHA1, size);
	hash_update(msg, size, 1);
	hash_final(digest, 5);
}

void hw_sha224(const void *msg, u32 size, u32 *digest)
{
	hash_init(HASH_CONF_ALG_SHA224, size);
	hash_update(msg, size, 1);
	hash_final(digest, 7);
}

void hw_sha256(const void *msg, u32 size, u32 *digest)
{
	hash_init(HASH_CONF_ALG_SHA256, size);
	hash_update(msg, size, 1);
	hash_final(digest, 8);
}

void hw_sha384(const void *msg, u32 size, u32 *digest)
{
	hash_init(HASH_CONF_ALG_SHA384, size);
	hash_update(msg, size, 1);
	hash_final(digest, 12);
}

void hw_sha512(const void *msg, u32 size, u32 *digest)
{
	hash_init(HASH_CONF_ALG_SHA512, size);
	hash_update(msg, size, 1);
	hash_final(digest, 16);
}

DECL_DEBUG_CMD(cmd_hash)
{
	u32 digest[16];
	u32 addr, len;
	int i, id, dlen;

	if (argc != 3)
		goto usage;

	if (number(argv[1], &addr))
		return;

	if (number(argv[2], &len))
		return;

	id = hash_id(argv[0]);
	if (id == HASH_NA)
		goto usage;

	dlen = hw_hash(id, (void *)addr, len, digest);

	for (i = 0; i < dlen; ++i)
		printf("%08x", __builtin_bswap32(digest[i]));
	puts("\n\n");

	return;
usage:
	printf("usage: %s addr len\n", argv[0]);
}

DEBUG_CMD("md5", "MD5 hash", cmd_hash);
DEBUG_CMD("sha1", "SHA1 hash", cmd_hash);
DEBUG_CMD("sha224", "SHA224 hash", cmd_hash);
DEBUG_CMD("sha256", "SHA256 hash", cmd_hash);
DEBUG_CMD("sha384", "SHA384 hash", cmd_hash);
DEBUG_CMD("sha512", "SHA512 hash", cmd_hash);

#if TEST_HASH
static int digestcmp(const u32 *x, const u32 *y, int l)
{
	while (l--)
		if (*x++ != *y++)
			return 1;

	return 0;
}

void test_hash(void)
{
	static const u32 test_md5[4] = {
		0x7ebe238b, 0x33767b42, 0xf3fcf55f, 0x23f4d754
	};
	static const u32 test_sha1[5] = {
		0x9dfc2a45, 0x0770e6c2, 0x4d7d5eac, 0xdc4d9fa6, 0x53db010a
	};
	static const u32 test_sha224[7] = {
		0xc7de9620, 0x494b5048, 0x187e24d3, 0xb708d1a4, 0x0cd9f150,
		0xcda73571, 0xe19b12c1
	};
	static const u32 test_sha256[8] = {
		0xf0936472, 0xbeecb613, 0x963d950d, 0xa60459cd, 0x4a794c46,
		0xabcb512f, 0xcdf76ca7, 0xe8c70da9
	};
	static const u32 test_sha384[12] = {
		0x4becc8bf, 0x787868be, 0x0f0fe979, 0x2c78b9cf, 0xe0dafdb2,
		0xde2f28cb, 0xbb2ee17b, 0x74c96049, 0xb5ee6a9a, 0x2d977726,
		0x2ae23861, 0x5991de77
	};
	static const u32 test_sha512[16] = {
		0x9d4685d9, 0x11ce57d1, 0x03642942, 0xad03df0e, 0x3b72faf8,
		0x3653cdac, 0x281ede44, 0x6132ba76, 0x59b2b67a, 0x0f226d7c,
		0x29d32e7e, 0xf989ca74, 0xb3c09291, 0x70294d68, 0xc12fce64,
		0x93554c88
	};
	static u32 msg[1024] __attribute__((aligned(16)));
	u32 dig[16];
	int i;

	for(i = 0; i < sizeof(msg) / sizeof(*msg); ++i)
		msg[i] = i + 0x1234beef;

	printf("Testing hardware hashing:\n");

#define TEST(n,l)					\
	do {						\
		hw_##n(msg, sizeof(msg) - 1, dig);	\
		if (digestcmp(dig, test_##n, (l)))	\
			printf("%s failed\n", #n);	\
		else					\
			printf("%s success\n", #n);	\
	} while (0)

	TEST(md5, 4);
	TEST(sha1, 5);
	TEST(sha224, 7);
	TEST(sha256, 8);
	TEST(sha384, 12);
	TEST(sha512, 16);
}
#endif /* TEST_HASH */

#include "crypto.h"
#include "crypto_hash.h"
#include "string.h"
#include "spi.h"
#include "tim.h"
#include "efuse.h"
#include "irq.h"
#include "stdio.h"

#define TIMH_ID		0x54494d48
#define TIMN_ID		0x54494d4e

#define RES_ID		0x4f505448
#define IMAP_ID		0x494d4150
#define CSKT_ID		0x43534b54

#define BOOTFS_SPINOR	0x5350490a

#define DSALG_ECDSA_521	6

#define HASHALG_SHA256	32
#define HASHALG_SHA512	64

#define SIG_SCHEME_ECDSA_P521_SHA256	0x0000b311

#define die(...) do { wait_for_irq(); } while (1)

typedef struct {
	u32 version;
	u32 identifier;
	u32 trusted;
	u32 issuedate;
	u32 oemuid;
	u32 reserved[5];
	u32 bootflashsign;
	u32 numimages;
	u32 numkeys;
	u32 sizeofreserved;
} timhdr_t;

typedef struct {
	u32 id;
	u32 nextid;
	u32 flashentryaddr;
	u32 loadaddr;
	u32 size;
	u32 sizetohash;
	u32 hashalg;
	u32 hash[16];
	u32 partitionnumber;
	u32 encalg;
	u32 encryptoffset;
	u32 encryptsize;
} imginfo_t;

typedef struct {
	u32 id;
	u32 hashalg;
	u32 size;
	u32 publickeysize;
	u32 encryptalg;
	union {
		u32 data[128];
		struct {
			u32 RSAexp[64];
			u32 RSAmod[64];
		};
		ec_point_t ECDSA;
	};
	u32 hash[16];
} keyinfo_t;

typedef struct {
	u32 id;
	u32 pkgs;
} reshdr_t;

typedef struct {
	u32 id;
	u32 size;
	union {
		u32 data[0];
		struct {
			u32 nmaps;
			struct {
				u32 id;
				u32 type;
				u32 flashentryaddr[2];
				u32 partitionnumber;
			} maps[0];
		} imap;
	};
} respkg_t;

typedef struct {
	u32 dsalg;
	u32 hashalg;
	u32 keysize;
	u32 hash[8];
	union {
		u32 data[192];
		struct {
			struct {
				u32 exp[64];
				u32 mod[64];
			} pub;
			u32 sig[64];
		} RSA;
		struct {
			ec_point_t pub;
			ec_sig_t sig;
		} ECDSA;
	};
} platds_t;

static imginfo_t *tim_find_image(timhdr_t *hdr, u32 id)
{
	imginfo_t *img = (void *)(hdr + 1);
	u32 i;

	for (i = 0; i < hdr->numimages; ++i, ++img)
		if (img->id == id)
			return img;

	return NULL;
}

static u32 tim_size(timhdr_t *hdr)
{
	u32 res;

	res = sizeof(timhdr_t);
	res += hdr->numimages * sizeof(imginfo_t);
	res += hdr->numkeys * sizeof(keyinfo_t);
	res += hdr->sizeofreserved;
	if (hdr->trusted)
		res += sizeof(platds_t);

	return res;
}

static void key_hash(const ec_point_t *pubkey, u32 *digest, int pad)
{
	u32 buf[129];

	memset(buf, 0, sizeof(buf));
	buf[0] = SIG_SCHEME_ECDSA_P521_SHA256;
	memcpy(&buf[1], &pubkey->x, 68);
	memcpy(&buf[18], &pubkey->y, 68);

	hw_sha256(buf, pad ? sizeof(buf) : 140, digest);
}

static int ecc_3bit_majority(u8 x)
{
	x &= 7;
	return (x < 3 || x == 4) ? 0 : 1;
}

#if 1
static int is_board_trusted(void)
{
	static u8 cache;
	u64 val64;
	u16 val;
	u8 boot_state;

	if (cache) {
		boot_state = cache - 1;
		goto cached;
	}

	if (efuse_read_row(1, &val64, NULL))
		die();

	val = (u16)val64;

	boot_state = ecc_3bit_majority(val);
	boot_state |= ecc_3bit_majority(val >> 4) << 1;
	cache = boot_state + 1;

cached:
	if (boot_state == 3)
		die();
	else if (boot_state == 2)
		return 1;
	else
		return 0;
}

static const u32 *board_read_otp_hash(void)
{
	static u32 hash[8];
	static int cached;
	u64 val;
	int i;

	if (cached)
		return hash;

	for (i = 0; i < 8; i += 2) {
		if (efuse_read_row(8 + i / 2, &val, NULL))
			die();

		hash[i + 1] = val >> 32;
		hash[i] = (u32)val;
	}

	cached = 1;

	return hash;
}
#else
static int is_board_trusted(void)
{
	return 1;
}

static const u32 *board_read_otp_hash(void)
{
	static u32 hash[8] = {
		0x85b46362, 0x094f0a17, 0x325b2bd4, 0xa59be253,
		0x5b394878, 0x292ea1d0, 0x117dd601, 0x256145f8
	};

	return hash;
}
#endif

static void bswap_hash(u32 *hash, u32 len)
{
	u32 i, tmp;

	for (i = 0; i < len / 2; ++i) {
		tmp = hash[i];
		hash[i] = __builtin_bswap32(hash[len - 1 - i]);
		hash[len - 1 - i] = __builtin_bswap32(tmp);
	}
}

static respkg_t *find_respkg(timhdr_t *hdr, u32 id)
{
	reshdr_t *reshdr;
	respkg_t *pkg;
	void *next;
	u32 i;

	if (hdr->sizeofreserved < sizeof(reshdr_t))
		return NULL;

	reshdr = (void *)(hdr + 1) + hdr->numimages * sizeof(imginfo_t) +
		 hdr->numkeys * sizeof(keyinfo_t);

	if (reshdr->id != RES_ID)
		return NULL;

	pkg = (void *)(reshdr + 1);
	for (i = 0; i < reshdr->pkgs; ++i) {
		if (pkg->size % 4 || pkg->size < 8)
			die();

		next = (void *)pkg + pkg->size;
		if (next - (void *)reshdr > hdr->sizeofreserved)
			return NULL;

		if (pkg->id == id)
			return pkg;

		pkg = next;
	}

	return NULL;
}

static u32 find_timn_entry(timhdr_t *hdr)
{
	respkg_t *imap;

	imap = find_respkg(hdr, IMAP_ID);
	if (imap->size < 12)
		return 0;

	/* force 1 IMAP */
	if (imap->imap.nmaps != 1)
		return 0;

	if (imap->size != 12 + imap->imap.nmaps * sizeof(imap->imap.maps[0]))
		return 0;

	if (imap->imap.maps[0].id == CSKT_ID &&
	    imap->imap.maps[0].type == 0 &&
	    imap->imap.maps[0].flashentryaddr[1] == 0 &&
	    imap->imap.maps[0].partitionnumber == 0)
		return imap->imap.maps[0].flashentryaddr[0];

	return 0;
}

static int check_tim(timhdr_t *hdr, u32 id)
{
	imginfo_t *self;
	u32 size;

	size = tim_size(hdr);
	if (size >= 0x2000)
		return -1;

	if (hdr->sizeofreserved % 4)
		die();

	if (hdr->identifier != id)
		return -1;

	if (hdr->bootflashsign != BOOTFS_SPINOR)
		return -1;

	self = tim_find_image(hdr, hdr->identifier);
	if (!self)
		return -1;

	if (self->size != size)
		return -1;

	if (is_board_trusted() != !!hdr->trusted)
		return -1;

	if (hdr->trusted) {
		const u32 *otp_hash;
		platds_t *platds;
		u32 hash[17];

		platds = (void *)hdr + tim_size(hdr) - sizeof(platds_t);
		if (platds->dsalg != DSALG_ECDSA_521 ||
		    platds->hashalg != HASHALG_SHA256 ||
		    platds->keysize != 521)
			return -1;

		key_hash(&platds->ECDSA.pub, &hash[0], 1);
		otp_hash = board_read_otp_hash();
		if (memcmp(&hash[0], otp_hash, 8 * sizeof(u32)))
			return -1;

		memset(hash, 0, sizeof(hash));
		hw_sha256(hdr, (u8 *)&platds->ECDSA.sig - (u8 *)hdr, hash);
		bswap_hash(hash, 8);

		if (ecdsa_verify(&platds->ECDSA.pub, &platds->ECDSA.sig, hash) != 1)
			return -1;
	}

	return 0;
}

static void boot_device_read(void *buf, u32 offset, u32 size)
{
	spi_nor_read(&nordev, buf, offset, size);
}

extern u8 next_timh_image[], next_timn_image[];
static timhdr_t *timhdr;

static int load_tim(void)
{
	timhdr_t *timh_hdr, *timn_hdr;

	boot_device_read(&next_timh_image, 0, 0x2000);
	timhdr = timh_hdr = (void *)&next_timh_image;

	if (check_tim(timhdr, TIMH_ID))
		return -1;

	if (timh_hdr->trusted) {
		u32 timn_entry = find_timn_entry(timh_hdr);

		if (!timn_entry || timn_entry >= 0x20000)
			return -1;

		boot_device_read(&next_timn_image, timn_entry, 0x2000);
		timhdr = timn_hdr = (void *)&next_timn_image;
		if (check_tim(timhdr, TIMN_ID))
			return -1;
	}

	return 0;
}

static int check_image_hash(imginfo_t *img, void *buf)
{
	u32 digest[16];
	int hashid;

	if (img->hashalg == HASHALG_SHA256)
		hashid = HASH_SHA256;
	else if (img->hashalg == HASHALG_SHA512)
		hashid = HASH_SHA512;
	else
		return -1;

	if (img->encalg)
		return -1;

	memset(digest, 0, sizeof(digest));
	if (memcmp(img->hash, digest, sizeof(digest))) {
		/* hash is not zero, we have to check it */
		if (hw_hash(hashid, buf, img->size, digest) < 0)
			return -1;

		if (memcmp(img->hash, digest, sizeof(digest)))
			return -1;
	}

	return 0;
}

int load_image(u32 id, void *dest, u32 *plen)
{
	imginfo_t *img;

	spi_init(&nordev);

	if (load_tim())
		return -1;

	img = tim_find_image(timhdr, id);
	if (!img)
		return -1;

	if (img->hashalg != HASHALG_SHA256 && img->hashalg != HASHALG_SHA512)
		return -1;

	boot_device_read(dest, img->flashentryaddr, img->size);

	if (check_image_hash(img, dest))
		return -1;

	if (plen)
		*plen = img->size;

	return 0;
}

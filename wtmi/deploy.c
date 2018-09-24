#include "io.h"
#include "ebg.h"
#include "efuse.h"
#include "clock.h"
#include "crypto.h"
#include "efuse.h"
#include "printf.h"

#define __from_mox_builder __attribute__((section(".from_mox_builder")))

struct mox_builder_data {
	u32 op;
	u32 serial_number;
	u32 manufacturing_time;
	u32 mac_addr_low;
	u32 mac_addr_high;
	u32 board_version;
	u32 otp_hash[8];
};

struct mox_builder_data __from_mox_builder mbd = {
	0x05050505, 0xdeaddead, 0,
	0xdeadbeef, 0xbeefdead, 0xb7b7b7b7,
	{ 0, 0, 0, 0, 0, 0, 0, 0 }
};

#define AP_RAM		0x60000000
#define PATTERN_1	0x0acc55aa
#define PATTERN_2	0xdeadbeef

static void write_test_data(int mb, u32 pattern)
{
	u32 dat;
	int i;

	dat = pattern;
	for (i = 0; i < 32; ++i) {
		writel(dat, (AP_RAM + (mb << 20)) + 4 * i);
		dat = (dat << 1) | (dat >> 31);
	}
}

static int check_test_data(int mb, u32 pattern)
{
	u32 dat;
	int i;

	dat = pattern;
	for (i = 0; i < 32; ++i) {
		if (readl((AP_RAM + (mb << 20)) + 4 * i) != dat)
			return 0;
		dat = (dat << 1) | (dat >> 31);
	}

	return 1;
}

static int get_ram_size(void)
{
	write_test_data(3, PATTERN_1);
	write_test_data(515, PATTERN_2);

	if (check_test_data(3, PATTERN_2))
		return 512;

	if (check_test_data(3, PATTERN_1) &&
	    check_test_data(515, PATTERN_2))
		return 1024;

	return -1;
}

static void do_deploy(void)
{
	int ram_size, i, res;
	u32 pubkey[17];
	u64 val;

	ram_size = get_ram_size();

	if (ram_size != 1024 && ram_size != 512)
		goto fail;

	printf("RAM%c", ram_size == 1024 ? '1' : '0');

#if 0
	val = mbd.manufacturing_time;
	val <<= 32;
	val |= mbd.serial_number;
	res = efuse_write_row_with_ecc_lock(43, val);
	if (res < 0)
		goto fail;

	val = (ram_size == 1024 ? 1 : 0);
	val <<= 8;
	val |= mbd.board_version & 0xff;
	val <<= 16;
	val |= mbd.mac_addr_high & 0xffff;
	val <<= 32;
	val |= mbd.mac_addr_low;
	res = efuse_write_row_with_ecc_lock(42, val);
	if (res < 0)
		goto fail;

	res = ecdsa_generate_efuse_private_key();
	if (res < 0)
		goto fail;
#endif

	res = ecdsa_get_efuse_public_key(pubkey);
	if (res < 0)
		goto fail;

	printf("PUBK%06x", pubkey[0]);
	for (i = 1; i < 17; ++i)
		printf("%08x", pubkey[i]);

	printf("\nSN: %08x\n", mbd.serial_number);
	printf("time: %08x\n", mbd.manufacturing_time);
	printf("bv: %d\n", mbd.board_version);
	printf("mac addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
	       (mbd.mac_addr_high >> 8) & 0xff,
	       (mbd.mac_addr_high >> 0) & 0xff,
	       (mbd.mac_addr_low >> 24) & 0xff,
	       (mbd.mac_addr_low >> 16) & 0xff,
	       (mbd.mac_addr_low >>  8) & 0xff,
	       (mbd.mac_addr_low >>  0) & 0xff);

	return;
fail:
	printf("FAIL");
}

void print_row_bits(u64 val, int lock)
{
	int i;

	printf("%c ", lock ? 'U' : '?');
	for (i = 63; i >= 0; --i) {
		printf("%c", ((val >> i) & 1) ? 'U' : '?');
		if (i == 32)
			printf(" ");
	}
	printf("\n");
}

void deploy(void)
{
	if (mbd.op == 0) {
		/* read OTP */
		int i, res, lock;
		u64 val;

		printf("OTP\n");
		for (i = 0; i < 44; ++i) {
			res = efuse_read_row_no_ecc(i, &val, &lock);
			if (res < 0)
				goto fail;
			print_row_bits(val, lock);
		}
	} else if (mbd.op == 1) {
		/* deploy */
		do_deploy();
	} else {
		goto fail;
	}

	return;
fail:
	printf("FAIL");
}
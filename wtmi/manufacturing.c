#include "io.h"
#include "ebg.h"
#include "efuse.h"
#include "clock.h"
#include "crypto.h"
#include "efuse.h"
#include "printf.h"

#define __from_mox_builder __attribute__((section(".from_mox_builder")))

struct mox_builder_data {
	u32 serial_number;
	u32 manufacturing_time;
	u32 mac_addr_low;
	u32 mac_addr_high;
	u32 board_version
};

struct mox_builder_data __from_mox_builder mbd;

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

void manufacturing(void)
{
	int ram_size, i, res;
	u32 pubkey[17];
	u64 val;

	wait_ns(100000000);

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
#endif

	res = ecdsa_generate_efuse_private_key();
	if (res < 0)
		goto fail;
	
	res = ecdsa_get_efuse_public_key(pubkey);
	if (res < 0)
		goto fail;

	printf("PUBK%06x", pubkey[0]);
	for (i = 1; i < 17; ++i)
		printf("%08x", pubkey[i]);

	return;
fail:
	printf("FAIL");
}

#include "io.h"
#include "ebg.h"
#include "efuse.h"
#include "clock.h"
#include "crypto.h"
#include "efuse.h"
#include "uart.h"
#include "printf.h"

#define __from_mox_builder __attribute__((section(".from_mox_builder")))

struct mox_builder_data {
	u32 op;
	u32 serial_number_low;
	u32 serial_number_high;
	u32 mac_addr_low;
	u32 mac_addr_high;
	u32 board_version;
	u32 otp_hash[8];
};

volatile struct mox_builder_data __from_mox_builder mbd = {
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
	/*
	 * Sometimes AP RAM behaves weirdly on first read.
	 * This is probably something to do with how the AP RAM is
	 * connected to secure processor.
	 * We do one read on AP RAM to overcome this.
	 */
	readl(AP_RAM);

	write_test_data(3, PATTERN_1);
	write_test_data(515, PATTERN_2);

	if (check_test_data(3, PATTERN_2))
		return 512;

	if (check_test_data(3, PATTERN_1) &&
	    check_test_data(515, PATTERN_2))
		return 1024;

	return -1;
}

static int row_write_if_not_locked(int row, u64 val)
{
	int res, lock;

	res = efuse_read_row_no_ecc(row, NULL, &lock);
	if (res < 0)
		return res;

	if (lock)
		return 0;

	return efuse_write_row_with_ecc_lock(row, val);
}

static int write_sn(void)
{
	u64 val;

	val = mbd.serial_number_high;
	val <<= 32;
	val |= mbd.serial_number_low;

	return row_write_if_not_locked(43, val);
}

static int write_ram_bver_mac(int ram_size)
{
	u64 val;

	val = (ram_size == 1024 ? 1 : 0);
	val <<= 8;
	val |= mbd.board_version & 0xff;
	val <<= 16;
	val |= mbd.mac_addr_high & 0xffff;
	val <<= 32;
	val |= mbd.mac_addr_low;
	return row_write_if_not_locked(42, val);
}

static int generate_and_write_ecdsa_key(void)
{
	int res, lock;

	res = efuse_read_row_no_ecc(30, NULL, &lock);
	if (res < 0)
		return res;

	if (lock)
		return 0;

	return ecdsa_generate_efuse_private_key();
}

static int write_security_info(void)
{
	int i, res, lock;
	u64 val;

	for (i = 0; i < 8; ++i) {
		if (mbd.otp_hash[i])
			break;
	}

	/* if otp_hash is all zeros, do not secure the board */
	if (i == 8)
		return 0;

	res = efuse_read_row_no_ecc(0, NULL, &lock);
	if (res < 0)
		return res;

	if (lock)
		return 0;

	/* if these fail, we are screwed anyway */

	for (i = 0; i < 8; i += 2) {
		val = mbd.otp_hash[i + 1];
		val <<= 32;
		val |= mbd.otp_hash[i];

		efuse_write_row_with_ecc_lock(8 + i / 2, val);
	}

	efuse_write_row_with_ecc_lock(0, 0x70000070000700ULL);
	efuse_write_row_with_ecc_lock(1, 0x70ULL);
	efuse_write_row_with_ecc_lock(2, 0x70770000ULL);
	efuse_write_row_with_ecc_lock(3, 0x7ULL);

	return 0;
}

static void do_deploy(void)
{
	int ram_size, i, res;
	u32 pubkey[17];
	u64 val;

	ram_size = get_ram_size();

#define die(...) do { printf(__VA_ARGS__); return; } while (0)

	if (ram_size != 1024 && ram_size != 512)
		die("FAIL_DETERMIN_RAM");

	/* write SN and time */
	if (write_sn() < 0)
		die("FAIL_WR_SERIAL_NR");

	/* write RAM size, board version, MAC address */
	if (write_ram_bver_mac(ram_size) < 0)
		die("FAIL_WR_RAM_V_MAC");

	/* generate ECDSA key if not yet generated */
	if (generate_and_write_ecdsa_key() < 0)
		die("FAIL_WR_ECDSA_KEY");

#if 0
	if (write_security_info() < 0)
		die("FAIL_WR_SECURITY ");
#endif

	/* send RAM info */
	printf("RAM%c", ram_size == 1024 ? '1' : '0');

	/* read SN and time and send back */
	if (efuse_read_row(43, &val, NULL) < 0)
		die("FAIL_RD_SERIAL_NR");

	printf("SERN%08x%08x", (u32) (val >> 32), (u32) val);

	/* read board version, MAC address and send back */
	if (efuse_read_row(42, &val, NULL) < 0)
		die("FAIL_RD_RAM_V_MAC");

	if ((((val >> 56) & 1) + 1) * 512 != ram_size)
		die("FAIL_BAD_RAM_WRTN");

	printf("BVER%02XMACA%04x%08x", (u32) ((val >> 48) & 0xff),
	       (u32) ((val >> 32) & 0xffff), (u32) val);

	res = ecdsa_get_efuse_public_key(pubkey);
	if (res < 0)
		die("FAIL_RD_ECDSA_KEY ");

	printf("PUBK%06x", pubkey[0]);
	for (i = 1; i < 17; ++i)
		printf("%08x", pubkey[i]);

	return;
}

static void deploy_putc(void *p, char c)
{
	int i;
	u8 t = (u8) c;

	for (i = 0; i < 8; ++i) {
		uart_putc(p, (char) ((t & 1) ? 0xff : 0x00));
		t >>= 1;
	}
}

void deploy(void)
{
	init_printf(NULL, deploy_putc);

	if (mbd.op == 0) {
		/* read OTP */
		int i, res, lock;
		u64 val;

		printf("OTP\n");
		for (i = 0; i < 44; ++i) {
			res = efuse_read_row_no_ecc(i, &val, &lock);
			if (res < 0)
				goto fail;

			printf("%d %08x%08x\n", lock ? 1 : 0,
			       (u32) (val >> 32), (u32) val);
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
#include "errno.h"
#include "types.h"
#include "printf.h"
#include "uart.h"
#include "mbox.h"
#include "efuse.h"
#include "ebg.h"
#include "clock.h"
#include "irq.h"
#include "crypto.h"
#include "ddr.h"
#include "deploy.h"

#define AP_RAM		0x60000000

volatile int jiffies;

void __irq systick_handler(void)
{
	++jiffies;
	ebg_systick();
}

#ifndef DEPLOY
static u32 cmd_get_random(u32 *args, u32 *out_args)
{
	int res;

	if (args[0] == 1) {
		if (args[1] & 3 || !args[2])
			return MBOX_STS(0, EINVAL, FAIL);

		ebg_rand_sync((void *) (AP_RAM + args[1]), args[2]);
		res = 0xfffff;
	} else {
		res = ebg_rand(out_args, MBOX_MAX_ARGS * sizeof(u32));
	}

	return MBOX_STS(0, res, SUCCESS);
}

static u32 cmd_board_info(u32 *args, u32 *out_args)
{
	int res, lock;
	u64 row42, row43;

	res = efuse_read_row(42, &row42, &lock);
	if (res < 0)
		return MBOX_STS(0, -res, FAIL);
	else if (!lock)
		return MBOX_STS(0, ENODATA, FAIL);

	res = efuse_read_row(43, &row43, &lock);
	if (res < 0)
		return MBOX_STS(0, -res, FAIL);
	else if (!lock)
		return MBOX_STS(0, ENODATA, FAIL);

	/*
	 * 0-1 = serial number
	 * 2 = board version
	 * 3 = RAM in MiB
	 * 4-5 = MAC address of eth0
	 * 6-7 = MAC address of eth1
	 */
	out_args[0] = row43 & 0xffffffff;
	out_args[1] = row43 >> 32;
	out_args[2] = (row42 >> 48) & 0xff;
	out_args[3] = ((row42 >> 56) & 1) ? 1024 : 512;

	out_args[4] = (row42 >> 32) & 0xffff;
	out_args[5] = row42 & 0xffffffff;
	++row42;
	out_args[6] = (row42 >> 32) & 0xffff;
	out_args[7] = row42 & 0xffffffff;

	return MBOX_STS(0, 0, SUCCESS);
}

static u32 cmd_ecdsa_pub_key(u32 *args, u32 *out_args)
{
	static int has_pub;
	static u32 pub[17];
	int res, i;

	if (!has_pub) {
		res = ecdsa_get_efuse_public_key(pub);

		if (res < 0)
			return MBOX_STS(0, -res, FAIL);

		has_pub = 1;
	}

	for (i = 0; i < 16; ++i)
		out_args[i] = pub[i + 1];

	return MBOX_STS(0, pub[0], SUCCESS);
}

static u32 cmd_ecdsa_sign(u32 *args, u32 *out_args)
{
	ec_sig_t sig;
	u32 msg[17];
	int res, i;

	/* check if src and dst addresses are correctly aligned */
	if (args[0] & 3 || args[1] & 3 || args[2] & 3)
		return MBOX_STS(0, 22, FAIL);

	/* read src message from AP RAM */
	for (i = 0; i < 17; ++i)
		msg[16 - i] = readl(AP_RAM + args[0] + 4 * i);

	res = ecdsa_sign(&sig, msg);
	if (res < 0)
		return MBOX_STS(0, -res, FAIL);

	/* write signature to AP RAM */
	for (i = 0; i < 17; ++i) {
		writel(sig.r[16 - i], AP_RAM + args[1] + 4 * i);
		writel(sig.s[16 - i], AP_RAM + args[2] + 4 * i);
	}

	return MBOX_STS(0, 0, SUCCESS);
}

static u32 cmd_otp_read(u32 *args, u32 *out_args)
{
	int lock, res;
	u64 val;

	if (args[0] > 43)
		return MBOX_STS(0, EACCES, FAIL);

	res = efuse_read_row_no_ecc(args[0], &val, &lock);
	if (res < 0)
		return MBOX_STS(0, -res, FAIL);

	out_args[0] = val;
	out_args[1] = val >> 32;
	out_args[2] = lock;

	return MBOX_STS(0, 0, SUCCESS);
}

static u32 cmd_otp_write(u32 *args, u32 *out_args)
{
	int res;
	u64 val;

	if (args[0] > 43)
		return MBOX_STS(0, EACCES, FAIL);

	val = args[2];
	val <<= 32;
	val |= args[1];

	res = efuse_write_row_no_ecc(args[0], val, args[3]);
	if (res < 0)
		return MBOX_STS(0, -res, FAIL);

	return MBOX_STS(0, 0, SUCCESS);
}

static void init_ddr(void)
{
	int res, size;
	u64 val;

	res = efuse_read_row(42, &val, NULL);
	if (res < 0) {
		size = 512;
	} else if ((val >> 56) & 1) {
		size = 1024;
	} else {
		size = 512;
	}

	ddr_main(CLK_PRESET_CPU1000_DDR800, 16, 12, 1, size);

	wait_ns(1000000);
}
#endif /* !DEPLOY */

void main(void)
{
	int res;

	res = clock_init();
	if (res < 0)
		return;

	res = uart_init(115200);
	if (res < 0)
		return;

	init_printf(NULL, uart_putc);

#ifdef DEPLOY
	ebg_init();
	deploy();

	/* warm reset */
	wait_ns(10000000);
	writel(0x1d1e, 0xc0013840);
#else /* !DEPLOY */
	init_ddr();
	ebg_init();
	enable_systick();

	mbox_init();
	mbox_register_cmd(MBOX_CMD_GET_RANDOM, cmd_get_random);
	mbox_register_cmd(MBOX_CMD_BOARD_INFO, cmd_board_info);
	mbox_register_cmd(MBOX_CMD_ECDSA_PUB_KEY, cmd_ecdsa_pub_key);
	mbox_register_cmd(MBOX_CMD_ECDSA_SIGN, cmd_ecdsa_sign);
	mbox_register_cmd(MBOX_CMD_OTP_READ, cmd_otp_read);
	mbox_register_cmd(MBOX_CMD_OTP_WRITE, cmd_otp_write);
	enable_irq();

	start_ap();

	while (1) {
		wait_for_irq();
		mbox_process_commands();
	}
#endif /* !DEPLOY */
}

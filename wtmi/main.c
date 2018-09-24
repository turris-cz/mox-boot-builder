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
#include "manufacturing.h"

volatile int jiffies;

void __irq systick_handler(void)
{
	++jiffies;
	ebg_systick();
}

static u32 cmd_get_random(u32 *args, u32 *out_args)
{
	int res;

	res = ebg_rand(out_args, MBOX_MAX_ARGS * sizeof(u32));

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
	 * 0 = serial number
	 * 1 = manufacturing timestamp
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

	ddr_main(CLK_PRESET_CPU1000_DDR800, 0, 16, 12, 1, size);

	wait_ns(1000000);
}

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

#if 0
	ebg_init();
	manufacturing();
	while (1)
		wait_for_irq();
#else

	init_ddr();
	ebg_init();
	enable_systick();

	mbox_init();
	mbox_register_cmd(MBOX_CMD_GET_RANDOM, cmd_get_random);
	mbox_register_cmd(MBOX_CMD_BOARD_INFO, cmd_board_info);
	mbox_register_cmd(MBOX_CMD_ECDSA_PUB_KEY, cmd_ecdsa_pub_key);
	enable_irq();

	start_ap();

	while (1) {
		wait_for_irq();
		mbox_process_commands();
	}
#endif
}

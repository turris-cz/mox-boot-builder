#include "errno.h"
#include "types.h"
#include "uart.h"
#include "mbox.h"
#include "efuse.h"
#include "ebg.h"
#include "clock.h"
#include "irq.h"
#include "crypto.h"
#include "crypto_hash.h"
#include "ddr.h"
#include "deploy.h"
#include "soc.h"
#include "board.h"
#include "debug.h"

#ifndef DEPLOY
static int process_ap_mem(void *param, u32 addr, u32 len,
			  void (*cb)(void **, void *, u32))
{
	u32 cur_base, new_base, cur_len;

	if ((addr + len) < addr)
		return -EIO;

	cur_base = 0;
	new_base = addr & 0xc0000000;

	if (cur_base != new_base) {
		rwtm_win_remap(0, new_base);
		cur_base = new_base;
	}

	while (1) {
		new_base = (addr + MIN(len, 0x40000000) - 1) & 0xc0000000;
		if (cur_base == new_base)
			cur_len = len;
		else
			cur_len = new_base - addr;

		cb(&param, (void *)AP_RAM(addr - cur_base), cur_len);
		len -= cur_len;
		addr = new_base;
		if (cur_base == new_base)
			break;

		rwtm_win_remap(0, new_base);
		cur_base = new_base;
	}

	if (cur_base != 0)
		rwtm_win_remap(0, 0);

	return 0;
}

static void paranoid_rand_ap_cb(void **param, void *addr, u32 len)
{
	paranoid_rand(addr, len);
}

static void copy_from_ap_cb(void **dst_p, void *addr, u32 len)
{
	memcpy(*dst_p, addr, len);
	*dst_p += len;
}

static int copy_from_ap(void *dst, u32 src, u32 len)
{
	return process_ap_mem(dst, src, len, copy_from_ap_cb);
}

static void copy_to_ap_cb(void **src_p, void *addr, u32 len)
{
	memcpy(addr, *src_p, len);
	*src_p += len;
}

static int copy_to_ap(u32 dst, void *src, u32 len)
{
	return process_ap_mem(src, dst, len, copy_to_ap_cb);
}

static int check_ap_addr(u32 addr, u32 len, u32 align)
{
	int ram_size = get_ram_size();

	if (addr % align)
		return 0;
	else if (ram_size == 4096)
		return 1;
	else if (addr > (ram_size << 20) ||
		 (addr + len - 1) > (ram_size << 20))
		return 0;
	else
		return 1;
}

maybe_unused static u32 cmd_get_random(u32 *args, u32 *out_args)
{
	int res;

	if (args[0] == 1) {
		if (!check_ap_addr(args[1], args[2], 4))
			return MBOX_STS(0, EINVAL, FAIL);

		res = process_ap_mem(NULL, args[1], args[2], paranoid_rand_ap_cb);
		if (res < 0)
			return MBOX_STS(0, -res, FAIL);

		res = 0xfffff;
	} else {
		res = ebg_rand(out_args, MBOX_MAX_ARGS * sizeof(u32));
	}

	return MBOX_STS(0, res, SUCCESS);
}

maybe_unused static u32 cmd_board_info(u32 *args, u32 *out_args)
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
	 * 8 = device (0 = Turris Mox, 2 = RIPE Atlas, other = reserved)
	 */
	out_args[0] = row43 & 0xffffffff;
	out_args[1] = row43 >> 32;
	out_args[2] = (row42 >> 48) & 0x3f;
	out_args[3] = 512 << ((row42 >> 56) & 3);

	out_args[4] = (row42 >> 32) & 0xffff;
	out_args[5] = row42 & 0xffffffff;
	++row42;
	out_args[6] = (row42 >> 32) & 0xffff;
	out_args[7] = row42 & 0xffffffff;
	out_args[8] = (row42 >> 54) & 3;

	return MBOX_STS(0, 0, SUCCESS);
}

maybe_unused static u32 cmd_ecdsa_pub_key(u32 *args, u32 *out_args)
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

/*
 * args[0] = 1, 2, 3, 4, 5, 6 for MD5, SHA1, SHA224, SHA256, SHA384 and SHA512
 * args[1] = address of input, aligned to 4 bytes
 * args[2] = input length
 */
maybe_unused static u32 cmd_hash(u32 *args, u32 *out_args)
{
	hw_hash_fnc_t fn;

	switch (args[0]) {
	case 0x1:
		fn = hw_md5;
		break;
	case 0x2:
		fn = hw_sha1;
		break;
	case 0x3:
		fn = hw_sha224;
		break;
	case 0x4:
		fn = hw_sha256;
		break;
	case 0x5:
		fn = hw_sha384;
		break;
	case 0x6:
		fn = hw_sha512;
		break;
	default:
		return MBOX_STS(0, EOPNOTSUPP, FAIL);
	}

	if (!check_ap_addr(args[1], args[2], 4))
		return MBOX_STS(0, EINVAL, FAIL);

	fn((void *)AP_RAM(args[1]), args[2], out_args);

	return MBOX_STS(0, 0, SUCCESS);
}

static void array_reverse_u32(u32 *x, int len)
{
	u32 tmp;
	int i, j;

	for (i = 0, j = len - 1; i < len / 2; ++i, --j) {
		tmp = x[i];
		x[i] = x[j];
		x[j] = tmp;
	}
}

/*
 * For ECDSA521
 *   args[0] = 0x1
 *   args[1] = address of input, 521 bits (17 words, little endian, msb first)
 *   args[2] = address of output signature R
 *   args[3] = address of output signature S
 *
 *   addresses must be aligned to 4 bytes
 */
maybe_unused static u32 cmd_sign(u32 *args, u32 *out_args)
{
	ec_sig_t sig;
	u32 msg[17];
	int res;

	if (args[0] != 0x1)
		return MBOX_STS(0, EOPNOTSUPP, FAIL);

	/* check if src and dst addresses are correctly aligned */
	if (!check_ap_addr(args[1], 68, 4) ||
	    !check_ap_addr(args[2], 68, 4) ||
	    !check_ap_addr(args[3], 68, 4))
		return MBOX_STS(0, EINVAL, FAIL);

	/* read src message from AP RAM */
	res = copy_from_ap(msg, args[1], 68);
	if (res < 0)
		return MBOX_STS(0, -res, FAIL);
	array_reverse_u32(msg, 17);

	res = ecdsa_sign(&sig, msg);
	if (res < 0)
		return MBOX_STS(0, -res, FAIL);

	array_reverse_u32(sig.r, 17);
	res = copy_to_ap(args[2], sig.r, 68);
	if (res < 0)
		return MBOX_STS(0, -res, FAIL);

	array_reverse_u32(sig.s, 17);
	res = copy_to_ap(args[3], sig.s, 68);
	if (res < 0)
		return MBOX_STS(0, -res, FAIL);

	return MBOX_STS(0, 0, SUCCESS);
}

maybe_unused static u32 cmd_verify(u32 *args, u32 *out_args)
{
	return MBOX_STS(0, EOPNOTSUPP, SUCCESS);
}

maybe_unused static u32 cmd_otp_read(u32 *args, u32 *out_args)
{
	int lock, res;
	u64 val;

	res = efuse_read_row_no_ecc(args[0], &val, &lock);
	if (res < 0)
		return MBOX_STS(0, -res, FAIL);

	out_args[0] = val;
	out_args[1] = val >> 32;
	out_args[2] = lock;

	return MBOX_STS(0, 0, SUCCESS);
}

maybe_unused static u32 cmd_otp_read_1b(u32 *args, u32 *out_args)
{
	int res;
	u64 val;

	/*
	 * check row and offset validity: 3-bit majority encoding is only used
	 * for rows 0-3, and offsets are always multiplies of 4
	 */
	if (args[0] > 3 || args[1] >= 64 || args[1] & 3)
		return MBOX_STS_MARVELL(EINVAL);

	res = efuse_read_row_no_ecc(args[0], &val, NULL);
	if (res < 0)
		return MBOX_STS_MARVELL(-res);

	/* read 3 bits from specified offset and return majority vote */
	val >>= args[1];
	val &= 0x7;
	val = (val == 3 || val > 4) ? 1 : 0;

	out_args[0] = val;

	return MBOX_STS_MARVELL(0);
}

maybe_unused static u32 cmd_otp_read_Nb(u32 *args, u32 *out_args, u64 mask,
					int dword)
{
	int res;
	u64 val;

	/* check offset validity (if not reading whole 64 bits) */
	if (!dword && args[1] >= 64)
		return MBOX_STS_MARVELL(EINVAL);

	res = efuse_read_row_no_ecc(args[0], &val, NULL);
	if (res < 0)
		return MBOX_STS_MARVELL(-res);

	/* read bits from specified offset (if not reading all 64 bits) */
	if (!dword)
		val >>= args[1];
	val &= mask;

	out_args[0] = val;
	if (dword)
		out_args[1] = val >> 32;

	return MBOX_STS_MARVELL(0);
}

maybe_unused static u32 cmd_otp_read_8b(u32 *args, u32 *out_args)
{
	return cmd_otp_read_Nb(args, out_args, 0xff, 0);
}

maybe_unused static u32 cmd_otp_read_32b(u32 *args, u32 *out_args)
{
	return cmd_otp_read_Nb(args, out_args, 0xffffffff, 0);
}

maybe_unused static u32 cmd_otp_read_64b(u32 *args, u32 *out_args)
{
	return cmd_otp_read_Nb(args, out_args, 0xffffffffffffffffULL, 1);
}

maybe_unused static u32 cmd_otp_read_256b(u32 *args, u32 *out_args)
{
	int res, i;
	u64 val;

	for (i = 0; i < 4; ++i) {
		res = efuse_read_row_no_ecc(args[0] + i, &val, NULL);
		if (res < 0)
			return MBOX_STS_MARVELL(-res);

		out_args[2 * i] = val;
		out_args[2 * i + 1] = val >> 32;
	}

	return MBOX_STS_MARVELL(0);
}

maybe_unused static u32 cmd_otp_write(u32 *args, u32 *out_args)
{
	int res;
	u64 val;

	val = args[2];
	val <<= 32;
	val |= args[1];

	res = efuse_write_row_no_ecc(args[0], val, args[3]);
	if (res < 0)
		return MBOX_STS(0, -res, FAIL);

	return MBOX_STS(0, 0, SUCCESS);
}

maybe_unused static u32 cmd_otp_write_1b(u32 *args, u32 *out_args)
{
	int res;
	u64 val;

	/*
	 * check row and offset validity: 3-bit majority encoding is only used
	 * for rows 0-3, and offsets are always multiplies of 4
	 */
	if (args[0] > 3 || args[1] >= 64 || args[1] & 3)
		return MBOX_STS_MARVELL(EINVAL);

	/* expand single bit to 3 bits for majority vote */
	val = (args[2] & 1) ? 7 : 0;
	val <<= args[1];

	res = efuse_write_row_no_ecc(args[0], val, 0);
	if (res < 0)
		return MBOX_STS_MARVELL(-res);

	return MBOX_STS_MARVELL(0);
}

maybe_unused static u32 cmd_otp_write_Nb(u32 *args, u32 mask)
{
	int res;
	u64 val;

	if (args[1] >= 64)
		return MBOX_STS_MARVELL(EINVAL);

	val = args[2] & mask;
	val <<= args[1];

	res = efuse_write_row_no_ecc(args[0], val, 0);
	if (res < 0)
		return MBOX_STS_MARVELL(-res);

	return MBOX_STS_MARVELL(0);
}

maybe_unused static u32 cmd_otp_write_8b(u32 *args, u32 *out_args)
{
	return cmd_otp_write_Nb(args, 0xff);
}

maybe_unused static u32 cmd_otp_write_32b(u32 *args, u32 *out_args)
{
	return cmd_otp_write_Nb(args, 0xffffffff);
}

maybe_unused static u32 cmd_otp_write_64b(u32 *args, u32 *out_args)
{
	int res;
	u64 val;

	val = args[2];
	val <<= 32;
	val |= args[1];

	res = efuse_write_row_no_ecc(args[0], val, 0);
	if (res < 0)
		return MBOX_STS_MARVELL(-res);

	return MBOX_STS_MARVELL(0);
}

maybe_unused static u32 cmd_otp_write_256b(u32 *args, u32 *out_args)
{
	int res, i;
	u64 val;

	for (i = 0; i < 4; ++i) {
		val = args[2 * i + 2];
		val <<= 32;
		val |= args[2 * i + 1];
		res = efuse_write_row_no_ecc(args[0] + i, val, 0);
		if (res < 0)
			return MBOX_STS_MARVELL(-res);
	}

	return MBOX_STS_MARVELL(0);
}

maybe_unused static u32 cmd_reboot(u32 *args, u32 *out_args)
{
	if (args[0] == SOC_MBOX_RESET_CMD_MAGIC)
		reset_soc();

	return MBOX_STS(0, EINVAL, FAIL);
}

static void init_ddr(void)
{
	int type, ram_size;
	u32 reg;

	reg = (readl(0xc00002c4) >> 4) & 0xf;
	if (reg == 3)
		type = DDR4;
	else
		type = DDR3;

	ram_size = get_ram_size();
	if (!ram_size) {
		printf("No DDR chip configured!\n");
		while (1)
			wait_for_irq();
	}

	printf("Initializing DDR%c (%i MiB)... ", type == DDR4 ? '4' : '3',
	       ram_size);
	if (ddr_main(CLK_PRESET_CPU1000_DDR800, type, 16,
		     type == DDR4 ? 11 : 12, 1, MIN(ram_size, 1024)))
		printf("failed\n");
	else
		printf("done\n");

	udelay(1000);
}
#endif /* !DEPLOY */

static void uart_init(const struct uart_info *uart, int reset)
{
	if (reset)
		uart_reset(uart, 115200);

	uart_set_stdio(uart);
}

#ifndef WTMI_APP
# define WTMI_APP 0
#endif

#ifndef WITHOUT_OTP_READ
# define WITHOUT_OTP_READ 0
#endif

#ifndef WITHOUT_OTP_WRITE
# define WITHOUT_OTP_WRITE 0
#endif

void __attribute__((noreturn)) main(void)
{
	enum board board;

	if (WTMI_APP)
		uart_init(&uart1_info, 0);
	else
		uart_init(get_debug_uart(), 1);

#ifdef DEPLOY
	ebg_init();
	deploy();

	/* warm reset */
	udelay(10000);
	writel(0x1d1e, 0xc0013840);
#else /* !DEPLOY */

	puts("CZ.NIC's Armada 3720 Secure Firmware " WTMI_VERSION
	     " (" __DATE__ " " __TIME__ ")");
	fputs("Running on ", stdout);
	puts(get_board_name());

	if (!WTMI_APP)
		init_ddr();

	ebg_init();
	enable_systick();

	board = get_board();
	if (board == Turris_MOX)
		soc_init();

	/* TODO: what do we want to do with the disabled commands */
	mbox_init();
	mbox_register_cmd(MBOX_CMD_GET_RANDOM, cmd_get_random);

	if (board == Turris_MOX || board == RIPE_Atlas) {
		mbox_register_cmd(MBOX_CMD_BOARD_INFO, cmd_board_info);
		mbox_register_cmd(MBOX_CMD_ECDSA_PUB_KEY, cmd_ecdsa_pub_key);
		/*mbox_register_cmd(MBOX_CMD_HASH, cmd_hash);*/
		mbox_register_cmd(MBOX_CMD_SIGN, cmd_sign);
		/*mbox_register_cmd(MBOX_CMD_VERIFY, cmd_verify);*/
	}

	mbox_register_cmd(MBOX_CMD_REBOOT, cmd_reboot);

	if (!WITHOUT_OTP_READ) {
		mbox_register_cmd(MBOX_CMD_OTP_READ, cmd_otp_read);
		mbox_register_cmd(MBOX_CMD_OTP_READ_1B, cmd_otp_read_1b);
		mbox_register_cmd(MBOX_CMD_OTP_READ_8B, cmd_otp_read_8b);
		mbox_register_cmd(MBOX_CMD_OTP_READ_32B, cmd_otp_read_32b);
		mbox_register_cmd(MBOX_CMD_OTP_READ_64B, cmd_otp_read_64b);
		mbox_register_cmd(MBOX_CMD_OTP_READ_256B, cmd_otp_read_256b);
	}
	if (!WITHOUT_OTP_WRITE && !is_secure_boot()) {
		mbox_register_cmd(MBOX_CMD_OTP_WRITE, cmd_otp_write);
		mbox_register_cmd(MBOX_CMD_OTP_WRITE_1B, cmd_otp_write_1b);
		mbox_register_cmd(MBOX_CMD_OTP_WRITE_8B, cmd_otp_write_8b);
		mbox_register_cmd(MBOX_CMD_OTP_WRITE_32B, cmd_otp_write_32b);
		mbox_register_cmd(MBOX_CMD_OTP_WRITE_64B, cmd_otp_write_64b);
		mbox_register_cmd(MBOX_CMD_OTP_WRITE_256B, cmd_otp_write_256b);
	}

	enable_irq();

	/*
	 * Start AP immediately only if debugging is disabled.
	 * If running as application, AP is already running.
	 */
	if (!debug_init() && !WTMI_APP)
		start_ap_workaround();

	while (1) {
		disable_irq();
		if (!mbox_has_cmd())
			wait_for_irq();
		enable_irq();
		if (board == Turris_MOX)
			mox_wdt_workaround();
		mbox_process_commands();
		debug_process();
		ebg_process();
	}
#endif /* !DEPLOY */
}

#include "types.h"
#include "io.h"
#include "clock.h"
#include "mbox.h"
#include "errno.h"
#include "string.h"
#include "debug.h"

#define EFUSE_CTRL		0x40003430
#define EFUSE_RW		0x40003434
#define EFUSE_D0		0x40003438
#define EFUSE_D1		0x4000343c
#define EFUSE_AUX		0x40003440
#define EFUSE_ROW_MASK0		0x40003450
#define EFUSE_ROW_MASK1		0x40003454
#define EFUSE_MASTER_CTRL	0x400037f4
#define EFUSE_RC(r,c)		((((r) & 0x3f) << 7) | ((c) & 0x7f))

#define SEC_STATUS		0x40000104

static const struct {
	s8 row;
	u8 pos;
} __attribute__((packed)) ecc[44] = {
	[ 0] = {  7, 1 },
	[ 1] = {  7, 2 },
	[ 2] = {  7, 3 },
	[ 3] = {  7, 4 },
	[ 4] = {  7, 5 },
	[ 5] = {  7, 6 },
	[ 6] = {  7, 7 },
	[ 7] = { -1, 0 },
	[ 8] = { 24, 1 },
	[ 9] = { 24, 2 },
	[10] = { 24, 3 },
	[11] = { 24, 4 },
	[12] = { 24, 5 },
	[13] = { 24, 6 },
	[14] = { 24, 7 },
	[15] = { 24, 8 },
	[16] = { 25, 1 },
	[17] = { 25, 2 },
	[18] = { 25, 3 },
	[19] = { 25, 4 },
	[20] = { 25, 5 },
	[21] = { 25, 6 },
	[22] = { 25, 7 },
	[23] = { 25, 8 },
	[24] = { -1, 0 },
	[25] = { -1, 0 },
	[26] = { 38, 1 },
	[27] = { 38, 2 },
	[28] = { 38, 3 },
	[29] = { 38, 4 },
	[30] = { 38, 5 },
	[31] = { 38, 6 },
	[32] = { 38, 7 },
	[33] = { 38, 8 },
	[34] = { 39, 1 },
	[35] = { 39, 2 },
	[36] = { 39, 3 },
	[37] = { 39, 4 },
	[38] = { -1, 0 },
	[39] = { -1, 0 },
	[40] = { 39, 5 },
	[41] = { 39, 6 },
	[42] = { 39, 7 },
	[43] = { 39, 8 },
};

static u64 secded_ecc(u64 data)
{
	static const u64 pos[8] = {
		0x145011110ff014ffULL, 0x24ff2222f000249fULL,
		0x4c9f444400ff44d0ULL, 0x84d08888ff0f8c50ULL,
		0x0931f0ff11110b21ULL, 0x0b22ff002222fa32ULL,
		0xfa24000f4444ff24ULL, 0xff280ff088880928ULL
	};
	u64 ecc, tmp;
	int i, j, t;

	ecc = 0;
	for (i = 0; i < 8; ++i) {
		tmp = pos[i];
		t = 0;
		for (j = 0; j < 64; ++j) {
			if (tmp & 1)
				t ^= (data >> j) & 1;
			tmp >>= 1;
		}
		ecc |= t << i;
	}

	return ecc;
}

static int _efuse_raw_read(u32 rwreg, u32 ecc_pos, u64 *val, int *lock)
{
	int i, res;

	writel(0x4, EFUSE_CTRL);
	setbitsl(EFUSE_CTRL, 0x8, 0x8);
	setbitsl(EFUSE_CTRL, 0x3, 0x7);
	ndelay(300);

	writel(rwreg, EFUSE_RW);
	if (ecc_pos)
		setbitsl(EFUSE_CTRL, ecc_pos << 24, 0xff000000);

	ndelay(300);
	setbitsl(EFUSE_CTRL, 0x100, 0x100);
	ndelay(200);
	setbitsl(EFUSE_CTRL, 0, 0x100);

	res = -ETIMEDOUT;

	for (i = 0; i < 100000; ++i) {
		u32 reg;

		reg = readl(EFUSE_AUX);
		if (reg & BIT(31)) {
			if (val) {
				*val = readl(EFUSE_D1);
				*val <<= 32;
				*val |= readl(EFUSE_D0);
			}
			if (lock)
				*lock = !!(reg & 0x10);
			res = 0;
			break;
		}
		ndelay(100);
	}

	setbitsl(EFUSE_CTRL, 0x4, 0x6);

	return res;
}

static inline int is_row_masked(int row)
{
	if (!(readl(SEC_STATUS) & BIT(1)))
		return 0;

	if (row < 32) {
		if ((readl(EFUSE_ROW_MASK0) >> row) & 1)
			return 1;
	} else {
		if ((readl(EFUSE_ROW_MASK1) >> (row - 32)) & 1)
			return 1;
	}

	return 0;
}

static int _efuse_read_row(int row, u64 *val, int *lock, int check_ecc, int sb)
{
	int res;
	u32 rwreg;

	if (row < 0 || row > 43)
		return -EINVAL;

	if (ecc[row].row == -1)
		check_ecc = 0;

	rwreg = EFUSE_RC(row, 0);
	if (sb)
		rwreg |= BIT(28);

	if (!check_ecc)
		return _efuse_raw_read(rwreg, 0, val, lock);

	res = _efuse_raw_read(BIT(31) | EFUSE_RC(ecc[row].row, 0), 0, val,
			      lock);
	if (res < 0)
		return res;

	res = _efuse_raw_read(rwreg, ecc[row].pos, val, lock);
	if (res < 0)
		return res;

	/* on ECC uncorrectable error return -EIO */
	if (((readl(EFUSE_AUX) >> 16) & 3) >= 2)
		return -EIO;

	return 0;
}

int efuse_read_row(int row, u64 *val, int *lock)
{
	int res;

	res = _efuse_read_row(row, val, lock, 1, 0);
	if (is_row_masked(row))
		return -EACCES;

	return res;
}

int efuse_read_row_no_ecc(int row, u64 *val, int *lock)
{
	int res;

	res = _efuse_read_row(row, val, lock, 0, 0);
	if (is_row_masked(row))
		return -EACCES;

	return res;
}

int efuse_read_secure_buffer(void)
{
	static int sbfill;
	int i, res;

	if (sbfill)
		return 0;

	res = _efuse_read_row(40, NULL, NULL, 1, 1);
	if (res < 0)
		return res;

	for (i = 37; i >= 30; --i) {
		res = _efuse_read_row(i, NULL, NULL, 1, 1);
		if (res < 0)
			return res;
	}

	sbfill = 1;

	return 0;
}

static int efuse_write_enable(void)
{
	int i, j, seq_val;

	writel(0x0, EFUSE_CTRL);
	ndelay(300);
	writel(0x5a, EFUSE_MASTER_CTRL);
	writel(0x100, EFUSE_CTRL);

	for (i = 0; i < 6; ++i) {
		seq_val = 0x18d;
		for (j = 0; j < 10; ++j) {
			writel(seq_val & 1 ? 0x300 : 0x100, EFUSE_CTRL);
			writel(seq_val & 1 ? 0x700 : 0x500, EFUSE_CTRL);
			seq_val >>= 1;
		}
	}

	writel(0x0, EFUSE_CTRL);

	for (i = 0; i < 100000; ++i) {
		if (readl(EFUSE_AUX) & BIT(29))
			return 0;

		ndelay(100);
	}

	return -ETIMEDOUT;
}

static void efuse_write_disable(void)
{
	writel(0x0005, EFUSE_CTRL);
	writel(0x0405, EFUSE_CTRL);
	writel(0x0005, EFUSE_CTRL);
	writel(0, EFUSE_MASTER_CTRL);
}

static int efuse_raw_write(int row, u64 val)
{
	int res, i;
	u64 _val;

	res = efuse_write_enable();
	if (res < 0)
		return res;

	/* row must be read before programming, otherwise writing won't work */
	res = _efuse_read_row(row, &_val, NULL, 0, 0);
	if (res < 0)
		return res;

	val |= _val;

	setbitsl(EFUSE_CTRL, 0x8, 0x8);
	setbitsl(EFUSE_CTRL, 0, 0x7);
	ndelay(500);

	for (i = 0; i < 64; ++i) {
		if (!((val >> i) & 1))
			continue;

		writel(EFUSE_RC(row, i), EFUSE_RW);
		setbitsl(EFUSE_CTRL, 0x100, 0x100);
		udelay(13);
		setbitsl(EFUSE_CTRL, 0, 0x100);
	}

	setbitsl(EFUSE_CTRL, 0x4, 0x4);
	setbitsl(EFUSE_CTRL, 0x1, 0x3);

	udelay(1000);
	efuse_write_disable();
	udelay(1000);

	return 0;
}

static int efuse_raw_lock(int row)
{
	int res;

	res = efuse_write_enable();
	if (res < 0)
		return res;

	/* row must be read before programming, otherwise locking won't work */
	res = _efuse_read_row(row, NULL, NULL, 0, 0);
	if (res < 0)
		return res;

	setbitsl(EFUSE_CTRL, 0x8, 0x8);
	setbitsl(EFUSE_CTRL, 0, 0x5);
	setbitsl(EFUSE_CTRL, 0x2, 0x2);
	ndelay(500);

	writel(EFUSE_RC(row, 0), EFUSE_RW);
	setbitsl(EFUSE_CTRL, 0x100, 0x100);
	udelay(13);
	setbitsl(EFUSE_CTRL, 0, 0x100);

	setbitsl(EFUSE_CTRL, 0x4, 0x4);
	setbitsl(EFUSE_CTRL, 0x1, 0x3);

	udelay(1000);
	efuse_write_disable();
	udelay(1000);

	return 0;
}

int efuse_write_row_no_ecc(int row, u64 val, int lock)
{
	int res, try, _lock, masked;
	u64 _val;

	if (row < 0 || row > 43)
		return -EINVAL;

	masked = is_row_masked(row);
	res = _efuse_read_row(row, &_val, &_lock, 0, 0);
	if (res < 0)
		return res;

	if (_lock)
		return val ? -EACCES : 0;

	if (val) {
		if (!masked)
			val |= _val;

		for (try = 0; try < 5; ++try) {
			res = efuse_raw_write(row, val);
			if (res < 0)
				return res;

			if (masked)
				break;

			res = _efuse_read_row(row, &_val, NULL, 0, 0);
			if (res < 0)
				return res;

			if (val == _val)
				break;
		}

		if (!masked && val != _val)
			return -EIO;
	}

	if (lock) {
		for (try = 0; try < 5; ++try) {
			res = efuse_raw_lock(row);
			if (res < 0)
				return res;

			res = _efuse_read_row(row, NULL, &_lock, 0, 0);
			if (res < 0)
				return res;

			if (_lock)
				break;
		}

		if (!_lock)
			return -EIO;
	}

	return 0;
}

int efuse_write_row_with_ecc_lock(int row, u64 val)
{
	u64 _val, eccval;
	int res, lock_ecc, _lock, i;

	if (row < 0 || row > 43)
		return -EINVAL;

	if (ecc[row].row == -1)
		return -EINVAL;

	res = _efuse_read_row(row, &_val, &_lock, 0, 0);
	if (res < 0)
		return res;

	if (_lock)
		return -EACCES;

	res = _efuse_read_row(ecc[row].row, &eccval, &_lock, 0, 0);
	if (res < 0)
		return res;

	if (_lock)
		return -EACCES;

	if ((eccval >> ((ecc[row].pos - 1) * 8)) & 0xff)
		return -EACCES;

	val |= _val;
	eccval |= secded_ecc(val) << ((ecc[row].pos - 1) * 8);

	for (i = 0; i < 44; ++i) {
		if (i != row && ecc[i].row == ecc[row].row) {
			res = _efuse_read_row(ecc[i].row, NULL, &_lock, 0, 0);
			if (res < 0)
				return res;

			if (!_lock)
				break;
		}
	}

	lock_ecc = (i == 45);

	res = efuse_write_row_no_ecc(row, val, 1);
	if (res < 0)
		return res;

	res = efuse_write_row_no_ecc(ecc[row].row, eccval, lock_ecc);
	if (res < 0)
		return res;

	res = _efuse_read_row(row, &_val, NULL, 1, 0);
	if (res < 0)
		return res;

	if (_val != val)
		return -EIO;

	return 0;
}

int efuse_write_secure_buffer(u32 *priv)
{
	int i, res;
	u64 val;

	for (i = 0; i < 8; ++i) {
		val = priv[i * 2 + 1];
		val <<= 32;
		val |= priv[i * 2];
		res = efuse_write_row_with_ecc_lock(30 + i, val);
		val = 0;
		if (res < 0)
			return res;
	}

	val = priv[16];
	res = efuse_write_row_with_ecc_lock(40, val);
	val = 0;

	return res;
}

DECL_DEBUG_CMD(cmd_efuse)
{
	u32 row = 0;

	if (argc < 2)
		goto usage;

	if (argc > 2 && decnumber(argv[2], &row))
		return;

	if (row >= 44) {
		printf("Invalid row number\n");
		return;
	}

	if (argv[1][0] == 'r') {
		u64 val;
		int lock, res, i;

		for (i = (argc == 3 ? row : 0);
		     i < (argc == 3 ? row + 1 : 44);
		     ++i) {
			res = efuse_read_row(i, &val, &lock);
			printf("Row %d: %016llx%s%s\n", i, val,
			       lock ? ", locked" : "",
			       res == -EIO ? ", ECC error" :
			       res == -EACCES ? ", masked" : "");
		}
	} else if (!strcmp(argv[1], "write")) {
		u64 val, oldval;
		u32 high, low;
		int lock, res;

		if (argc < 5)
			goto usage;

		if (number(argv[3], &high))
			goto usage;

		if (number(argv[4], &low))
			goto usage;

		val = high;
		val <<= 32;
		val |= low;

		res = efuse_read_row(row, &oldval, &lock);
		if (res < 0 && res != -EIO) {
			printf("Could not read old value!\n");
		} else if (lock) {
			printf("Row is already locked!\n");
		} else if ((val & oldval) != oldval) {
			printf("New value has to be a superset of old value!\n");
		} else {
			res = efuse_write_row_no_ecc(row, val, 0);
			if (res < 0)
				printf("Could not write row %i with value %016llx\n", row, val);
			else
				printf("Written row %i with value %016llx\n", row, val);
		}
	} else if (!strcmp(argv[1], "lock")) {
		int res;

		if (argc < 3)
			goto usage;

		res = efuse_write_row_with_ecc_lock(row, 0);
		if (res < 0)
			printf("Failed locking row %i\n", row);
		else
			printf("Row %i locked\n", row);
	} else {
		goto usage;
	}

	return;
usage:
	printf("usage: efuse read [row]\n"
	       "       efuse write <row> <high_value> <low_value>\n"
	       "       efuse lock <row>\n");
}

DEBUG_CMD("efuse", "eFuse access", cmd_efuse);

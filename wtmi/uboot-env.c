#include "types.h"
#include "string.h"
#include "crc32.h"
#include "spi.h"
#include "debug.h"

#define UBOOT_ENV_NOR_OFFSET	0x180000
#define UBOOT_ENV_NOR_SIZE	0x10000

extern char uboot_env_buffer[0x2000];

static int uboot_env_check_crc(void)
{
	char *buf = uboot_env_buffer;
	u32 crc, stored_crc;
	size_t pos;

	spi_nor_read(&nordev, &stored_crc, UBOOT_ENV_NOR_OFFSET, 4);

	crc = 0xffffffff;

	pos = sizeof(u32);
	while (pos < UBOOT_ENV_NOR_SIZE) {
		size_t len = MIN(sizeof(uboot_env_buffer),
				 UBOOT_ENV_NOR_SIZE - pos);

		spi_nor_read(&nordev, buf, UBOOT_ENV_NOR_OFFSET + pos, len);

		crc = crc32(crc, buf, len);
		pos += len;
	}

	return ~crc == stored_crc;
}

int uboot_env_for_each(int (*cb)(const char *var, const char *val, void *data),
		       void *data)
{
	char *buf = uboot_env_buffer;
	int ignore_first, ignored;
	size_t pos;

	spi_init(&nordev);

	if (!uboot_env_check_crc())
		return -1;

	/* skip crc */
	pos = sizeof(u32);

	ignore_first = 0;
	ignored = 0;

	while (pos < UBOOT_ENV_NOR_SIZE) {
		size_t len = MIN(sizeof(uboot_env_buffer),
				 UBOOT_ENV_NOR_SIZE - pos);
		char *p, *z, *e;

		spi_nor_read(&nordev, buf, UBOOT_ENV_NOR_OFFSET + pos, len);

		p = buf;
		e = buf + len;
		z = memchr(p, '\0', e - p);

		if (!ignore_first && p == z)
			break;

		while (z) {
			if (ignore_first) {
				ignore_first = 0;
			} else {
				char *q = memchr(p, '=', z - p);

				if (q) {
					*q = '\0';
					if (!cb(p, q + 1, data))
						return ignored;
				} else {
					/* variable ignored due to not
					 * containing '=' */
					ignored++;
				}
			}

			p = z + 1;
			if (p == e || *p == '\0')
				break;

			z = memchr(p, '\0', e - p);
		}

		if (p < e && *p == '\0')
			break;

		if (p == buf) {
			/* variable ignored due to not having enough space in
			 * our work buffer */
			ignore_first = 1;
			ignored++;
			pos += len;
		} else {
			pos += p - buf;
		}
	}

	return ignored;
}

struct find_env {
	const char *var;
	const char *val;
};

static int find_one_env(const char *var, const char *val, void *ptr)
{
	struct find_env *find = ptr;

	if (!strcmp(var, find->var)) {
		find->val = val;
		return 0;
	}

	return 1;
}

const char *uboot_env_get(const char *var)
{
	struct find_env find = {
		.var = var,
		.val = NULL,
	};

	if (uboot_env_for_each(find_one_env, &find) < 0)
		return NULL;

	return find.val;
}

static int print_one_env(const char *var, const char *val, void *cmp)
{
	if (!cmp || !strcmp(cmp, var)) {
		printf("%s=%s\n", var, val);
		if (cmp)
			return 0;
	}

	return 1;
}

DECL_DEBUG_CMD(cmd_print)
{
	int res;

	res = uboot_env_for_each(print_one_env, argc > 1 ? argv[1] : NULL);
	if (res < 0)
		printf("u-boot env has bad CRC\n");
	else if (res > 0)
		printf("\n%d variables ignored (were invalid or did not have enough space in work buffer)\n",
		       res);
}
DEBUG_CMD("print", "Print U-Boot environment variable", cmd_print);

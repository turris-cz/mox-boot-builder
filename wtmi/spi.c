#include "types.h"
#include "io.h"
#include "clock.h"
#include "spi.h"
#include "string.h"
#include "debug.h"

#define NB_TBG_SEL		0xc0013000
#define NB_DIV_SEL0		0xc0013004
#define NB_DIV_SEL1		0xc0013008
#define NB_DIV_SEL2		0xc001300c
#define NB_CLK_SEL		0xc0013010
#define NB_CLK_EN		0xc0013014

#define SPI_CTRL		0xc0010600
#define SPI_CTRL_RDY		BIT(1)
#define SPI_CFG			0xc0010604
#define SPI_DOUT		0xc0010608
#define SPI_DIN			0xc001060c

#define SPINOR_OP_RDID		0x9f
#define SPINOR_OP_READ		0x03
#define SPINOR_OP_READ_FAST	0x0b

const struct spi nordev = {
	.cs = 0,
	.cpol = 0,
	.cpha = 0,
};

static inline void spi_ctrl_wait_bit(u32 x)
{
	while (!(readl(SPI_CTRL) & x))
		ndelay(100);
}

void spi_init(const struct spi *spi)
{
	u32 cfg;

	/* set CS0 to SPI mode */
	setbitsl(NB_PINCTRL, 0, BIT(11));

	/* configure SQF clock */
	setbitsl(NB_CLK_EN, BIT(12), BIT(12));
	setbitsl(NB_CLK_SEL, BIT(7), BIT(7));
	setbitsl(NB_DIV_SEL1, (5 << 27) | (5 << 24), (7 << 27) | (7 << 24));
	setbitsl(NB_TBG_SEL, 1, 3 << 12);
	setbitsl(NB_CLK_EN, 0, BIT(12));

	setbitsl(SPI_CTRL, 0x0, 0xf0000);
	cfg = BIT(9);
	if (spi->cpol)
		cfg |= BIT(7);
	if (spi->cpha)
		cfg |= BIT(6);

	/* prescaler 20 */
	cfg |= 1;//0x1a;
	writel(cfg, SPI_CFG);
}

static inline void spi_cs_activate(const struct spi *spi)
{
	setbitsl(SPI_CTRL, BIT(16 + spi->cs), BIT(16 + spi->cs));
}

static inline void spi_cs_deactivate(const struct spi *spi)
{
	setbitsl(SPI_CTRL, 0, BIT(16 + spi->cs));
}

static void spi_xfer(void *din, const void *dout, u32 len)
{
	const u8 *pout = dout;
	u8 *pin = din;
	u32 c;

	while (len) {
		spi_ctrl_wait_bit(SPI_CTRL_RDY);
		c = dout ? *pout++ : 0;

		writel(c, SPI_DOUT);

		spi_ctrl_wait_bit(SPI_CTRL_RDY);
		c = readl(SPI_DIN);
		if (din)
			*pin++ = c;

		--len;
	}
}

static void spi_flash_cmd_rw(const struct spi *spi, const void *cmd, u32 cmdlen,
			     void *din, const void *dout, u32 datalen)
{
	spi_cs_activate(spi);

	spi_xfer(NULL, cmd, cmdlen);
	if (datalen)
		spi_xfer(din, dout, datalen);

	spi_cs_deactivate(spi);
}

static void spi_flash_cmd(const struct spi *spi, u8 cmd, void *din, u32 datalen)
{
	return spi_flash_cmd_rw(spi, &cmd, 1, din, NULL, datalen);
}

void spi_nor_read_id(const struct spi *spi, u8 *id)
{
	spi_flash_cmd(spi, SPINOR_OP_RDID, id, 6);
}

void spi_nor_read(const struct spi *spi, void *dst, u32 pos, u32 len)
{
	u8 op[5];

	op[0] = SPINOR_OP_READ_FAST;
	op[1] = pos >> 16;
	op[2] = pos >> 8;
	op[3] = pos;
	op[4] = 0xff;

	spi_flash_cmd_rw(spi, op, sizeof(op), dst, NULL, len);
}

DECL_DEBUG_CMD(cmd_spi)
{
	struct spi spidev;
	u32 rlen, wlen;
	const char *wptr_hex;

	spidev.cpol = spidev.cpha = 0;

	if (argc < 3)
		goto usage;

	if (argv[1][0] < '0' || argv[1][0] > '3') {
		printf("Only values 0-3 supported for chip-select!\n");
		return;
	}

	spidev.cs = argv[1][0] - '0';

	if (argv[1][1] != '\0') {
		if (argv[1][1] != '.')
			goto usage;

		if (argv[1][2] < '0' || argv[1][2] > '3') {
			printf("Only values 0-3 supported for mode!\n");
			return;
		}

		spidev.cpha = !!((argv[1][2] - '0') & BIT(0));
		spidev.cpol = !!((argv[1][2] - '0') & BIT(1));
	}

	if (number(argv[2], &rlen))
		return;

	if (!rlen)
		return;

	wlen = argc > 3 ? strlen(argv[3]) : 0;
	wptr_hex = argc > 3 ? argv[3] : NULL;

	if (wlen && wlen != rlen * 2) {
		printf("Data argument has wrong length\n");
		return;
	}

	spi_cs_activate(&spidev);

	while (rlen) {
		u8 r, w;

		if (wlen) {
			u32 lw;
			char tmp[3];

			tmp[0] = *wptr_hex++;
			tmp[1] = *wptr_hex++;
			tmp[2] = '\0';

			if (number(tmp, &lw))
				w = 0;
			else
				w = lw;
		} else {
			w = 0;
		}

		spi_xfer(&r, &w, 1);

		printf("%02x", r);
		--rlen;
	}

	spi_cs_deactivate(&spidev);
	printf("\n");

	return;
usage:
	printf("usage: spi <cs>[.<mode>] <hex_len> [hex_data_out]\n");
}

DEBUG_CMD("spi", "SPI utility", cmd_spi);

DECL_DEBUG_CMD(cmd_sf)
{
	spi_init(&nordev);

	if (argc < 2)
		goto usage;

	if (!strcmp(argv[1], "id")) {
		u8 id[6];
		spi_nor_read_id(&nordev, id);
		printf("ID: %02x %02x %02x %02x %02x %02x\n", id[0], id[1],
		        id[2], id[3], id[4], id[5]);
	} else if (!strcmp(argv[1], "read")) {
		u32 addr, pos, len, total, block = 256;

		if (argc < 5)
			goto usage;
		if (number(argv[2], &addr))
			goto usage;
		if (number(argv[3], &pos))
			goto usage;
		if (number(argv[4], &len))
			goto usage;
		if (argc > 5 && number(argv[5], &block))
			goto usage;

		printf("Reading 0x%x bytes from SPI position 0x%x to address 0x%08x, block size 0x%x\n",
		        len, pos, addr, block);

		total = 0;
		while (len) {
			void *buf = (void *)addr;
			u32 rd;

			rd = len > block ? block : len;
			spi_nor_read(&nordev, buf, pos, rd);
			total += rd;
			if (total % 0x10000 == 0)
				printf("\r%x read", total);

			len -= rd;
			pos += rd;
			addr += rd;
		}
		printf("\r%x read\n", total);
	} else {
		goto usage;
	}

	return;
usage:
	printf("usage: sf id\n");
	printf("       sf read addr position length [block_size]\n");
}

DEBUG_CMD("sf", "SPI flash", cmd_sf);

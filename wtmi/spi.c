#include "types.h"
#include "io.h"
#include "clock.h"
#include "spi.h"

#define NB_PINCTRL		0xc0013830
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
		wait_ns(10);
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
	spi_flash_cmd(spi, SPINOR_OP_RDID, id, sizeof(id));
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

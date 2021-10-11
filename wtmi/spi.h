#ifndef _SPI_H_
#define _SPI_H_

#include "types.h"

struct spi {
	int cs;
	int cpol;
	int cpha;
};

extern const struct spi nordev;
void spi_init(const struct spi *spi);
void spi_nor_read_id(const struct spi *spi, u8 *id);
void spi_nor_read(const struct spi *spi, void *dst, u32 pos, u32 len);
void spi_write(const struct spi *spi, const void *buf, u32 len);

#endif /* _SPI_H_ */

#ifndef _MDIO_H_
#define _MDIO_H_

#include "types.h"

void mdio_write(int dev, int reg, u16 val);
int mdio_read(int dev, int reg);

#endif /* _MDIO_H_ */

#ifndef _MDIO_H_
#define _MDIO_H_

#include "types.h"

extern void mdio_begin(void);
extern void mdio_end(void);
extern void mdio_write(int dev, int reg, u16 val);
extern int mdio_read(int dev, int reg);

#endif /* _MDIO_H_ */

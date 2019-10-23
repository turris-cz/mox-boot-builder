#ifndef _EBG_H_
#define _EBG_H_

#include "types.h"

extern void ebg_init(void);
extern void ebg_process(void);
extern u32 ebg_rand(void *buffer, u32 size);
extern void ebg_rand_sync(void *buffer, u32 size);
extern void paranoid_rand(void *buffer, u32 size);

#endif /* _EBG_H_ */

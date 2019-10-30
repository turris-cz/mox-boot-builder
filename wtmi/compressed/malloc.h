#ifndef _MALLOC_H_
#define _MALLOC_H_

#include "../types.h"

extern void malloc_init(void);
extern void *malloc(u32 size);
extern void *calloc(u32 items, u32 size);
extern void free(void *ptr);

#endif /* !_MALLOC_H_ */
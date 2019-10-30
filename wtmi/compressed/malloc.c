#include "malloc.h"

extern u8 malloc_start, malloc_end;

void malloc_init(void)
{

}

void *malloc(u32 size)
{
	return &malloc_start;
}

void *calloc(u32 items, u32 size)
{
	return &malloc_start;
}

void free(void *ptr)
{

}

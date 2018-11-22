#ifndef _EBG_H_
#define _EBG_H_

extern void ebg_init(void);
extern void ebg_systick(void);
extern int ebg_rand(void *buffer, int size);
extern void ebg_rand_sync(void *buffer, int size);
extern void paranoid_rand(void *buffer, int size);

#endif /* _EBG_H_ */

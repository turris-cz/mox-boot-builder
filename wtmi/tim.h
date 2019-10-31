#ifndef _TIM_H_
#define _TIM_H_

#include "types.h"

#define WTMI_ID		0x57544d49
#define OBMI_ID		0x4f424d49

extern int load_image(u32 id, void *dest, u32 *len);

#endif /* !_TIM_H_ */

#ifndef _GPIO_H_
#define _GPIO_H_

#define NGPIOS			66

#include "types.h"
#include "io.h"

#define GPIO_NB_BASE		0xc0013800
#define GPIO_SB_BASE		0xc0018800

#define GPIO_OUTPUT_ENABLE	0x0
#define GPIO_PIN_INPUT		0x10
#define GPIO_PIN_OUTPUT		0x18

static inline int gpio_is_valid(int gpio)
{
	return (gpio >= 0 && gpio < NGPIOS);
}

static inline int gpio_regs(int gpio, u32 *base, u32 *pos, u32 *bit)
{
	switch (gpio) {
	case 0 ... 35:
		if (base)
			*base = GPIO_NB_BASE;
		if (pos)
			*pos = (gpio >> 5) << 2;
		if (bit)
			*bit = gpio % 32;
		return 0;
	case 36 ... 65:
		if (base)
			*base = GPIO_SB_BASE;
		if (pos)
			*pos = 0;
		if (bit)
			*bit = gpio - 36;
		return 0;
	default:
		return -1;
	}
}

typedef enum {
	GPIO_IN = 0,
	GPIO_OUT = 1
} gpio_dir_t;

static inline gpio_dir_t gpio_get_dir(int gpio)
{
	u32 base, pos, bit;

	if (gpio_regs(gpio, &base, &pos, &bit))
		return -1;

	if (readl(base + GPIO_OUTPUT_ENABLE + pos) & BIT(bit))
		return GPIO_OUT;
	else
		return GPIO_IN;
}

static inline void gpio_set_dir(int gpio, gpio_dir_t dir)
{
	u32 base, pos, bit;

	if (gpio_regs(gpio, &base, &pos, &bit))
		return;

	setbitsl(base + GPIO_OUTPUT_ENABLE + pos, dir ? BIT(bit) : 0, BIT(bit));
}

static inline int gpio_get_val(int gpio)
{
	u32 base, pos, bit;

	if (gpio_regs(gpio, &base, &pos, &bit))
		return -1;

	if (readl(base + GPIO_OUTPUT_ENABLE + pos) & BIT(bit))
		base += GPIO_PIN_OUTPUT;
	else
		base += GPIO_PIN_INPUT;

	return !!(readl(base + pos) & BIT(bit));
}

static inline void gpio_set_val(int gpio, int val)
{
	u32 base, pos, bit;

	if (gpio_regs(gpio, &base, &pos, &bit))
		return;

	setbitsl(base + GPIO_PIN_OUTPUT + pos, val ? BIT(bit) : 0, BIT(bit));
	setbitsl(base + GPIO_OUTPUT_ENABLE + pos, BIT(bit), BIT(bit));
}

#endif /* _GPIO_H_ */

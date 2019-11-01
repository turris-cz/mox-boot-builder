#include "types.h"
#include "string.h"
#include "gpio.h"
#include "debug.h"

DECL_DEBUG_CMD(gpio)
{
	u32 i;

	if (argc < 2)
		goto usage;

	if (!strncmp(argv[1], "stat", 4)) {
		for (i = 0; i < NGPIOS; ++i)
			printf("GPIO %u: %s: %d\n", i,
			       gpio_get_dir(i) ? "output" : "input",
			       gpio_get_val(i));
	} else {
		if (argc < 3)
			goto usage;

		if (decnumber(argv[2], &i))
			return;

		if (i >= NGPIOS) {
			printf("GPIO %u unavailable\n", i);
			return;
		}

		if (argv[1][0] == 'i') {
			gpio_set_dir(i, GPIO_IN);
			printf("GPIO %u: input: %d\n", i, gpio_get_val(i));
		} else if (argv[1][0] == 'c') {
			gpio_set_val(i, 0);
			printf("GPIO %u: output: 0\n", i);
		} else if (!strcmp(argv[1], "set")) {
			gpio_set_val(i, 1);
			printf("GPIO %u: output: 1\n", i);
		} else if (argv[1][0] == 't') {
			int val = gpio_get_val(i);

			gpio_set_val(i, !val);
			printf("GPIO %u: output: %d\n", i, !val);
		} else {
			goto usage;
		}
	}

	return;
usage:
	puts("usage: gpio <input|set|clear|toggle> <pin>\n");
	puts("       gpio status\n");
}

DEBUG_CMD("gpio", "query and control gpio pins", gpio);

DECL_DEBUG_CMD(led)
{
	if (argc != 2)
		goto usage;

	if (!strcmp(argv[1], "on") || !strcmp(argv[1], "1")) {
		gpio_set_val(LED_GPIO, 0);
	} else if (!strcmp(argv[1], "off") || !strcmp(argv[1], "0")) {
		gpio_set_val(LED_GPIO, 1);
	} else {
		goto usage;
	}

	return;
usage:
	puts("usage: led <on|off>\n");
}

DEBUG_CMD("led", "led control", led);

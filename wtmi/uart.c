/*
* ***************************************************************************
* Copyright (C) 2017 Marvell International Ltd.
* ***************************************************************************
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* Neither the name of Marvell nor the names of its contributors may be used
* to endorse or promote products derived from this software without specific
* prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
* OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
***************************************************************************
*/

#include "errno.h"
#include "types.h"
#include "io.h"
#include "uart.h"
#include "clock.h"
#include "stdio.h"
#include "debug.h"

const struct uart_info uart1_info = {
	.rx	= 0xc0012000,
	.tx	= 0xc0012004,
	.ctrl	= 0xc0012008,
	.status	= 0xc001200c,
	.rx_ready_bit = BIT(4),
	.baud	= 0xc0012010,
	.possr	= 0xc0012014,
};

const struct uart_info uart2_info = {
	.rx	= 0xc0012218,
	.tx	= 0xc001221c,
	.ctrl	= 0xc0012204,
	.status	= 0xc001220c,
	.rx_ready_bit = BIT(14),
	.baud	= 0xc0012210,
	.possr	= 0xc0012214,
};

int uart_putc(int _c, void *p);

static FILE uart_stdout = {
	.putc = uart_putc,
};

void uart_set_stdio(const struct uart_info *info)
{
	uart_stdout.data = (void *)info;
	stdout = &uart_stdout;
}

void uart_reset(const struct uart_info *info, unsigned int baudrate)
{
	u32 parent_rate = get_ref_clk() * 1000000;

	/* set baudrate */
	writel(div_round_closest_u32(parent_rate, baudrate * 16), info->baud);

	/* set Programmable Oversampling Stack to 0, UART defaults to 16X scheme */
	writel(0, info->possr);

	/* reset FIFOs */
	writel(BIT(14) | BIT(15), info->ctrl);

	udelay(1);

	/* No Parity, 1 Stop */
	writel(0, info->ctrl);

	udelay(100);

	/* uart2 pinctrl enable */
	if (info == &uart2_info)
		setbitsl(NB_PINCTRL, BIT(19), BIT(19) | BIT(13) | BIT(14));
}

int uart_putc(int _c, void *p)
{
	const struct uart_info *info = p;
	unsigned char c = _c;

	if (c == '\n')
		uart_putc('\r', p);

	while (readl(info->status) & BIT(11))
		udelay(20);

	writel(c, info->tx);

	if (c == '\n') {
		while (!(readl(info->status) & BIT(6)))
			udelay(20);
	}

	return c;
}

int uart_getc(const struct uart_info *info)
{
	static u32 x;
	if (x % 100 == 0 && info == &uart2_info)
		setbitsl(NB_PINCTRL, BIT(19), BIT(19) | BIT(13) | BIT(14));
	++x;
	if (readl(info->status) & info->rx_ready_bit)
		return readl(info->rx) & 0xff;
	else
		return -EAGAIN;
}

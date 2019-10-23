#include "types.h"
#include "clock.h"
#include "io.h"
#include "irq.h"
#include "mbox.h"
#include "printf.h"

#define MBOX_IN_ARG(n)		(0x40000000 + (n) * 4)
#define MBOX_IN_CMD		0x40000040
#define MBOX_OUT_STATUS		0x40000080
#define MBOX_OUT_ARG(n)		(0x40000084 + (n) * 4)
#define MBOX_FIFO_STATUS	0x400000c4

#define SECURE_STATUS		0x40000104
#define AXPROT_CONTROL		0x400001a0
#define HOST_INT_SET		0x40000234
#define SP_INT_RESET		0x40000218
#define SP_INT_MASK		0x4000021c
#define SP_CONTROL		0x40000220

#define CMD_QUEUE_SIZE		8

static mbox_cmd_handler_t cmd_handlers[16];

typedef struct {
	u16 cmd;
	u32 args[MBOX_MAX_ARGS];
} cmd_request_t;

static cmd_request_t cmd_queue[CMD_QUEUE_SIZE];
static int cmd_queue_fill, cmd_queue_first;

void mbox_process_commands(void)
{
	while (cmd_queue_fill > 0) {
		cmd_request_t *req;
		u32 status, out_args[MBOX_MAX_ARGS];
		int i;

		/* out_args can contain sensitive stack values, rewrite them */
		for (i = 0; i < MBOX_MAX_ARGS; ++i)
			out_args[i] = 0;

		req = &cmd_queue[cmd_queue_first];

		status = cmd_handlers[req->cmd](req->args, out_args);
		if (MBOX_STS_CMD(status) == 0)
			status |= req->cmd;

		if (MBOX_STS_ERROR(status) != MBOX_STS_LATER)
			mbox_send(status, out_args);

		disable_irq();
		cmd_queue_first = (cmd_queue_first + 1) % CMD_QUEUE_SIZE;
		--cmd_queue_fill;
		enable_irq();
	}
}

int mbox_has_cmd(void)
{
	return !!(readl(SP_CONTROL) & CMD_REG_OCCUPIED_BIT);
}

void mbox_irq_handler(int irq)
{
	int i;
	u32 cmd;

	if (!mbox_has_cmd())
		return;

	if (cmd_queue_fill == CMD_QUEUE_SIZE) {
		setbitsl(HOST_INT_SET, HOST_INT_CMD_QUEUE_FULL_ACCESS,
			 HOST_INT_CMD_QUEUE_FULL_ACCESS);
		goto clear_irq;
	}

	cmd = readl(MBOX_IN_CMD) & MBOX_CMD_MASK;

	if (cmd < 16 && cmd_handlers[cmd]) {
		cmd_request_t *req;

		req = &cmd_queue[(cmd_queue_first + cmd_queue_fill) % CMD_QUEUE_SIZE];

		req->cmd = cmd;
		for (i = 0; i < MBOX_MAX_ARGS; ++i)
			req->args[i] = readl(MBOX_IN_ARG(i));

		++cmd_queue_fill;
	} else {
		mbox_send(MBOX_STS(cmd, 0, BADCMD), NULL);
	}

clear_irq:
	setbitsl(SP_INT_RESET, CMD_REG_OCCUPIED_RESET_BIT,
		 CMD_REG_OCCUPIED_RESET_BIT);
	setbitsl(SP_CONTROL, 0, CMD_REG_OCCUPIED_BIT);
}

void mbox_init(void)
{
	setbitsl(SP_INT_MASK, 0, CMD_REG_OCCUPIED_RESET_BIT);

	writel(BIT(8), MBOX_FIFO_STATUS);

	register_irq_handler(IRQn_SP, mbox_irq_handler);
	nvic_set_priority(IRQn_SP, 16);
	nvic_enable(IRQn_SP);
}

void mbox_register_cmd(u16 cmd, mbox_cmd_handler_t handler)
{
	if (cmd >= 16 || cmd_handlers[cmd])
		return;

	cmd_handlers[cmd] = handler;
}

void mbox_send(u32 status, u32 *args)
{
	int i;

	/*
	 * If AP did not read previous message yet, we must wait til it does.
	 * The opreation of reading this state and sending the message has
	 * to be atomic, since mbox_send can also be called from
	 * mbox_irq_handler. Thus we disable_irq in such a way as follows.
	 */
	while (1) {
		disable_irq();
		if (readl(HOST_INT_SET) & HOST_INT_CMD_COMPLETE_BIT)
			enable_irq();
		else
			break;

		wait_ns(100000);
	}

	if (args) {
		for (i = 0; i < MBOX_MAX_ARGS; i++)
			writel(args[i], MBOX_OUT_ARG(i));
	}

	writel(status, MBOX_OUT_STATUS);

	setbitsl(HOST_INT_SET, HOST_INT_CMD_COMPLETE_BIT,
		 HOST_INT_CMD_COMPLETE_BIT);

	enable_irq();
}

void start_ap_at(u32 addr)
{
	writel(addr, MBOX_OUT_ARG(0));
	writel(0x1003, MBOX_OUT_STATUS);
	setbitsl(HOST_INT_SET, HOST_INT_CMD_COMPLETE_BIT,
		 HOST_INT_CMD_COMPLETE_BIT);
}

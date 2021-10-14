/*
 * ***************************************************************************
 * Copyright (C) 2015 Marvell International Ltd.
 * ***************************************************************************
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 2 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ***************************************************************************
*/

#ifndef _MBOX_H_
#define _MBOX_H_

#define MBOX_MAX_ARGS			16
#define MBOX_CMD_MASK			0x0000FFFF

#define CMD_REG_OCCUPIED_RESET_BIT	BIT(1)
#define CMD_REG_OCCUPIED_BIT		BIT(0)
#define HOST_INT_CMD_COMPLETE_BIT	BIT(0)
#define HOST_INT_CMD_QUEUE_FULL_ACCESS	BIT(17)
#define HOST_INT_CMD_QUEUE_FULL		BIT(18)

#define MBOX_STS_SUCCESS		(0 << 30)
#define MBOX_STS_FAIL			(1 << 30)
#define MBOX_STS_BADCMD			(2 << 30)
#define MBOX_STS_LATER			(3 << 30)
#define MBOX_STS_ERROR(s)		((s) & (3 << 30))
#define MBOX_STS_VALUE(s)		(((s) >> 10) & 0xfffff)
#define MBOX_STS_CMD(s)			((s) & 0x3ff)
#define MBOX_STS(cmd,val,err)		(((cmd) & 0x3ff) | (((val) & 0xfffff) << 10) | MBOX_STS_##err)
#define MBOX_STS_MARVELL(err)		((err) == 0 ? 0 : (err) == ETIMEDOUT ? 2 : (err) == EINVAL ? 3 : (err) == ENOSYS ? 4 : 1)

enum mbox_cmd {
	MBOX_CMD_GET_RANDOM	= 1,
	MBOX_CMD_BOARD_INFO,
	MBOX_CMD_ECDSA_PUB_KEY,
	MBOX_CMD_HASH,
	MBOX_CMD_SIGN,
	MBOX_CMD_VERIFY,

	MBOX_CMD_OTP_READ,
	MBOX_CMD_OTP_WRITE,

	MBOX_CMD_REBOOT,

	/* OTP read commands supported by Marvell's fuse.bin firmware */
	MBOX_CMD_OTP_READ_1B	= 257,
	MBOX_CMD_OTP_READ_8B,
	MBOX_CMD_OTP_READ_32B,
	MBOX_CMD_OTP_READ_64B,
	MBOX_CMD_OTP_READ_256B,

	/* OTP write commands supported by Marvell's fuse.bin firmware */
	MBOX_CMD_OTP_WRITE_1B	= 513,
	MBOX_CMD_OTP_WRITE_8B,
	MBOX_CMD_OTP_WRITE_32B,
	MBOX_CMD_OTP_WRITE_64B,
	MBOX_CMD_OTP_WRITE_256B,
};

typedef u32 (*mbox_cmd_handler_t)(u32 *in_args, u32 *out_args);

extern void mbox_init(void);
extern void mbox_register_cmd(u16 cmd, mbox_cmd_handler_t handler);
extern int mbox_has_cmd(void);
extern void mbox_process_commands(void);
extern void mbox_send(u32 status, u32 *args);

#define ATF_ENTRY_ADDRESS		0x04100000

extern void start_ap_at(u32 addr);
static inline void start_ap(void)
{
	start_ap_at(ATF_ENTRY_ADDRESS);
}

#endif /* _MBOX_H_ */

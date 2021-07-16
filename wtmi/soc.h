#ifndef _SOC_H_
#define _SOC_H_

#define SOC_MBOX_RESET_CMD_MAGIC	0xdeadbeef

/* This constant is from TF-A */
#define PLAT_MARVELL_MAILBOX_BASE	0x64000400

extern void start_ap_workaround(void);
extern void reset_soc(void);
extern void mox_wdt_workaround(void);

#endif /* _SOC_H_ */

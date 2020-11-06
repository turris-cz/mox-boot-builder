#ifndef _SOC_H_
#define _SOC_H_

#define SOC_MBOX_RESET_CMD_MAGIC	0xdeadbeef

extern void start_ap_workaround(void);
extern void reset_soc(void);
extern void soc_wdt_workaround(void);

#endif /* _SOC_H_ */

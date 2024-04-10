#ifndef _UBOOT_ENV_H_
#define _UBOOT_ENV_H_

const char *uboot_env_get(const char *var);

int uboot_env_for_each(int (*cb)(const char *var, const char *val, void *data),
		       void *data);

#endif /* _UBOOT_ENV_H_ */

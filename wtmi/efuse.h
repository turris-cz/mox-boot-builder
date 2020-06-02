#ifndef _EFUSE_H_
#define _EFUSE_H_

extern int efuse_read_row(int row, u64 *val, int *lock);
extern int efuse_read_row_no_ecc(int row, u64 *val, int *lock);
extern int efuse_read_secure_buffer(int *pall_locked);
extern int efuse_write_row_no_ecc(int row, u64 val, int lock);
extern int efuse_write_row_with_ecc_lock(int row, u64 val);
extern int efuse_write_secure_buffer(u32 *priv);
#endif /* _EFUSE_H_ */

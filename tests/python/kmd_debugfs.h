#ifndef _KMD_DEBUGFS_H
#define _KMD_DEBUGFS_H

#include <stdint.h>

int OpenDebugfs(unsigned int device_id);
void CloseDebugfs(unsigned int device_id);
uint32_t read_cmd(uint64_t full_address);
void write_cmd(uint64_t full_address, uint32_t val);

#endif /* _KMD_DEBUGFS_H */

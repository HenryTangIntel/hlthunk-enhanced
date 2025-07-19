#ifndef _NO_PCI_DRIVER_H
#define _NO_PCI_DRIVER_H

#include <stdint.h>

/* OpenResourceInDevice - Maps a PCI bar to userspace process
 * @busid    - the PCI bdf address
 * @res_num  - the resource number in the sysfs (0 for BAR0, 2 for BAR2, etc.)
 * @res_size - the size of the resource to mmap
 * @offset   - the offset from the base resource address to mmap
 * returns file descriptor of that resource
 */
int OpenResourceInDevice(const char *busid, unsigned int res_num, size_t res_size,
			unsigned int offset);
void CloseResourceInDevice(int fd);

/* All the below functions receive the FD from the open. The offset value is the offset from
 * the start of the mmap base. Therefore, the effective address is:
 * resource base + offset given in open function + offset
 */
uint8_t read_8bit(int fd, uint64_t offset);
void write_8bit(int fd, uint64_t offset, uint8_t val);
uint16_t read_16bit(int fd, uint64_t offset);
void write_16bit(int fd, uint64_t offset, uint16_t val);
uint32_t read_32bit(int fd, uint64_t offset);
void write_32bit(int fd, uint64_t offset, uint32_t val);
uint64_t read_64bit(int fd, uint64_t offset);
void write_64bit(int fd, uint64_t offset, uint64_t val);

#endif /* _NO_PCI_DRIVER_H */

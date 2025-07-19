#include <string>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <sys/mman.h>

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

#define MAX_FD_NUM 24

#define BAR2_BASE_ADDR 0x7FF8000000ull

#define read8(addr) (*((volatile uint8_t *)(addr)))
#define write8(addr, v) (*((volatile uint8_t *)(addr)) = (v))
#define read16(addr) (*((volatile uint16_t *)(addr)))
#define write16(addr, v) (*((volatile uint16_t *)(addr)) = (v))
#define readl(addr) (*((volatile uint32_t *)(addr)))
#define writel(addr, v) (*((volatile uint32_t *)(addr)) = (v))
#define readq(addr) (*((volatile uint64_t *)(addr)))
#define writeq(addr, v) (*((volatile uint64_t *)(addr)) = (v))

void* mapbase[MAX_FD_NUM];
size_t mapsize[MAX_FD_NUM];

static int pci_mmap_fw(int fd, size_t size, unsigned int offset, void **mapbase, size_t *mapsz)
{
	/* calculate the page offset */
	unsigned int off_base = offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
	size_t map_sz = (offset - off_base) + size;
	int map_prot = PROT_READ | PROT_WRITE;
	void *base;

	base = mmap(NULL, map_sz, map_prot, MAP_SHARED, fd, off_base);
	if (base == MAP_FAILED) {
		printf("Failed to mmap the bar\n");
		return -1;
	}

	*mapbase = base;
	*mapsz = map_sz;

	return 0;
}

int OpenResourceInDevice(const char *busid, unsigned int res_num, size_t res_size,
			unsigned int offset)
{
	char resource_base[PATH_MAX];
	int fd, rc;

	if (!busid)
		return -1;

	if (res_num > 5)
		return -1;

	snprintf(resource_base, PATH_MAX, "/sys/bus/pci/devices/%s/resource%d",
					busid, res_num);

	fd = open(resource_base, O_RDWR | O_SYNC);
	if (fd < 0)
		printf("Failed to open resource %d of busid %s\n", res_num, busid);

	rc = pci_mmap_fw(fd, res_size, offset, &mapbase[fd], &mapsize[fd]);
	if (rc < 0)
		close(fd);

	return fd;
}

void CloseResourceInDevice(int fd)
{
	if (fd >= MAX_FD_NUM || fd < 0)
		return;

	munmap(mapbase[fd], mapsize[fd]);

	close(fd);
}

uint8_t read_8bit(int fd, uint64_t offset)
{
	uint8_t *ptr;

	if (fd >= MAX_FD_NUM || fd < 0)
		return 0;

	ptr = (uint8_t *) mapbase[fd];

	return read8(ptr + (offset - BAR2_BASE_ADDR));
}

void write_8bit(int fd, uint64_t offset, uint8_t val)
{
	uint8_t *ptr;

	if (fd >= MAX_FD_NUM || fd < 0)
		return;

	ptr = (uint8_t *) mapbase[fd];

	write8(ptr + (offset - BAR2_BASE_ADDR), val);
}
uint16_t read_16bit(int fd, uint64_t offset)
{
	uint8_t *ptr;

	if (fd >= MAX_FD_NUM || fd < 0)
		return 0;

	ptr = (uint8_t *) mapbase[fd];

	return read16(ptr + (offset - BAR2_BASE_ADDR));
}

void write_16bit(int fd, uint64_t offset, uint16_t val)
{
	uint8_t *ptr;

	if (fd >= MAX_FD_NUM || fd < 0)
		return;

	ptr = (uint8_t *) mapbase[fd];

	write16(ptr + (offset - BAR2_BASE_ADDR), val);
}
uint32_t read_32bit(int fd, uint64_t offset)
{
	uint8_t *ptr;

	if (fd >= MAX_FD_NUM || fd < 0)
		return 0;

	ptr = (uint8_t *) mapbase[fd];

	return readl(ptr + (offset - BAR2_BASE_ADDR));
}

void write_32bit(int fd, uint64_t offset, uint32_t val)
{
	uint8_t *ptr;

	if (fd >= MAX_FD_NUM || fd < 0)
		return;

	ptr = (uint8_t *) mapbase[fd];

	writel(ptr + (offset - BAR2_BASE_ADDR), val);
}

uint64_t read_64bit(int fd, uint64_t offset)
{
	uint8_t *ptr;

	if (fd >= MAX_FD_NUM || fd < 0)
		return 0;

	ptr = (uint8_t *) mapbase[fd];

	return readq(ptr + (offset - BAR2_BASE_ADDR));
}

void write_64bit(int fd, uint64_t offset, uint64_t val)
{
	uint8_t *ptr;

	if (fd >= MAX_FD_NUM || fd < 0)
		return;

	ptr = (uint8_t *) mapbase[fd];

	writeq(ptr + (offset - BAR2_BASE_ADDR), val);
}

#ifdef __cplusplus
}
#endif

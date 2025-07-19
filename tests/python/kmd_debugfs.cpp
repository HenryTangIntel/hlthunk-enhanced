#include <string>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

int m_addr_fd[8], m_data_fd[8];

int OpenDebugfs(unsigned int device_id)
{
    char addr_str[100], data_str[100], parent_device_str[100], parent_device[16];
    int parent_device_fd;
    ssize_t size;

    if (device_id >= 8) {
        printf("Invalid device id %d\n", device_id);
        return -1;
    }

    sprintf(parent_device_str, "/sys/class/accel/accel%d/device/parent_device", device_id);
    parent_device_fd = open(parent_device_str, O_RDONLY);
    if (parent_device_fd == -1)
	    return -EPERM;

    size = read(parent_device_fd, parent_device, sizeof(parent_device));
    if (size <= 0) {
	close(parent_device_fd);
	return -errno;
    }

    parent_device[strcspn(parent_device, "\n")] = '\0'; /* remove trailing newline character */
    close(parent_device_fd);

    sprintf(addr_str, "/sys/kernel/debug/accel/%s/addr", parent_device);
    sprintf(data_str, "/sys/kernel/debug/accel/%s/data32", parent_device);
    m_addr_fd[device_id] = open(addr_str, O_WRONLY);
    m_data_fd[device_id] = open(data_str, O_RDWR);

    if ((m_addr_fd[device_id] == -1) || (m_data_fd[device_id] == -1))
        return -EPERM;

    return 0;
}

void CloseDebugfs(unsigned int device_id)
{
    if (device_id >= 8) {
        printf("Invalid device id %d\n", device_id);
        return;
    }

    close(m_addr_fd[device_id]);
    close(m_data_fd[device_id]);
}

uint32_t read_cmd(unsigned int device_id, uint64_t full_address)
{
    char addr_str[64] = {0}, value[64] = {0};
    string val_str;

    if (device_id >= 8) {
        printf("Invalid device id %d\n", device_id);
        return 0;
    }

    sprintf(addr_str, "0x%lx", full_address);

    ssize_t bytes_written = write(m_addr_fd[device_id], addr_str,
                                  strlen(addr_str) + 1);

    if (bytes_written != (ssize_t) strlen(addr_str) + 1)
        return 0xFFFFFFFF;

    ssize_t bytes_read = pread(m_data_fd[device_id], value, sizeof(value), 0);
    if (bytes_read < 1)
        return 0xFFFFFFFF;

    val_str = value;

    return stol(val_str, nullptr, 16);
}

void write_cmd(unsigned int device_id, uint64_t full_address, uint32_t val)
{
    char addr_str[64] = {0}, val_str[64] = {0};

    if (device_id >= 8) {
        printf("Invalid device id %d\n", device_id);
        return;
    }

    sprintf(addr_str, "0x%lx", full_address);
    sprintf(val_str, "0x%x", val);

    ssize_t bytes_written = write(m_addr_fd[device_id], addr_str,
                                  strlen(addr_str) + 1);

    if (bytes_written != (ssize_t) strlen(addr_str) + 1)
        return;

    bytes_written = write(m_data_fd[device_id], val_str, strlen(val_str) + 1);
}

#ifdef __cplusplus
}
#endif

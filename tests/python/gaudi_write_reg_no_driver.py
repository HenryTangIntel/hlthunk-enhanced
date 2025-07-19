#!/usr/bin/env python
"""
@author: Oded Gabbay

This script reads every register on the ASIC
"""

import ctypes
import sys, getopt
import numpy as np

def hex2(n):
    return (n & 0xffffffff)

def hex64(n):
    return (n & 0xffffffffffffffff)

def print_usage():
    print ("-p <PCI BDF address> -a <address to write>, "
            "-v <value in hex>, -l <number of times to write (1)>, "
            "-r (Optional; for read after write) -q (Optional; write 64-bit value)")

def get_arguments(argv):
    pci_busid = ""
    address = ""
    val = ""
    loop = 1
    missing_arguments = 3
    read_after_write = 0
    quad_write = 0

    try:
        opts, args = getopt.getopt(argv,"hqrp:a:v:l:",
                                   ["pci=", "address=", "val=", "loop="])
    except getopt.GetoptError:
        print_usage()
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            print_usage()
            sys.exit()
        elif opt in ("-p", "--pci"):
            pci_busid = arg
            missing_arguments -= 1
        elif opt in ("-a", "--address"):
            address = arg
            missing_arguments -= 1
        elif opt in ("-v", "--val"):
            val = arg
            missing_arguments -= 1
        elif opt in ("-l", "--loop"):
            loop = int(arg)
        elif opt in ("-r"):
            read_after_write = 1
        elif opt in ("-q"):
            quad_write = 1
        else:
            print_usage()
            sys.exit(2)

    if (missing_arguments > 0):
        print ("Not enough arguments!!!")
        print_usage()
        sys.exit(2)

    return pci_busid, address, val, loop, read_after_write, quad_write

def write_64bit(pci_lib, fd, address, val, loop, read_after_write):
    if (read_after_write == 1):
        while(loop > 0):
            pci_lib.write_64bit(fd, ctypes.c_uint64(address), ctypes.c_uint64(val))
            read_val = pci_lib.read_64bit(fd, ctypes.c_uint64(address))
            print("%#016x" % address, " = %#016x" % hex64(read_val))
            loop -= 1
    else:
        while(loop > 0):
            pci_lib.write_64bit(fd, ctypes.c_uint64(address), ctypes.c_uint64(val))
            print("%#016x" % np.uint64(address), " <= %#016x" % np.uint64(val))
            loop -= 1

def write_32bit(pci_lib, fd, address, val, loop, read_after_write):
    if (read_after_write == 1):
        while(loop > 0):
            pci_lib.write_32bit(fd, ctypes.c_uint64(address), ctypes.c_uint32(val))
            read_val = pci_lib.read_32bit(fd, ctypes.c_uint64(address))
            print("%#016x" % address, " = %#x" % hex2(read_val))
            loop -= 1
    else:
        while(loop > 0):
            pci_lib.write_32bit(fd, ctypes.c_uint64(address), ctypes.c_uint32(val))
            print("%#016x" % np.uint64(address), " <= %#x" % np.uint32(val))
            loop -= 1

def main(argv):
    pci_busid = ""
    address = ""
    val = ""
    loop = 1
    read_after_write = 0

    pci_lib = ctypes.CDLL('./libno_pci_driver.so')

    pci_busid, address, val, loop, read_after_write, quad_write = get_arguments(argv)
    print ("Opening %s" % pci_busid)

    pci_busid_str = pci_busid.encode('utf-8')

    fd = pci_lib.OpenResourceInDevice(ctypes.c_char_p(pci_busid_str), 2, ctypes.c_size_t(0x8000000), 0)
    if (fd < 0):
        print ("failed to open resource")
        return -1

    if (quad_write == 1):
        write_64bit(pci_lib, fd, int(address, 16), int(val, 16), loop, read_after_write)
    else:
        write_32bit(pci_lib, fd, int(address, 16), int(val, 16), loop, read_after_write)
        
    pci_lib.CloseResourceInDevice(fd)

    return 0

if __name__ == "__main__":
    main(sys.argv[1:])

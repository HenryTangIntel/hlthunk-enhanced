#!/usr/bin/env python
"""
@author: Oded Gabbay

This script reads every register on the ASIC
"""

import ctypes
import sys, getopt
import numpy as np
import random

def print_usage():
    print ("-p <PCI BDF address> -r (Optional; for read after write)")

def hex2(n):
    return (n & 0xffffffff)

def get_arguments(argv):
    pci_busid = ""
    missing_arguments = 1
    read_after_write = 0

    try:
        opts, args = getopt.getopt(argv,"hrp:",["pci="])
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
        elif opt in ("-r"):
            read_after_write = 1
        else:
            print_usage()
            sys.exit(2)

    if (missing_arguments > 0):
        print ("Not enough arguments!!!")
        print_usage()
        sys.exit(2)

    return pci_busid, read_after_write

def write_only(pci_lib, fd):
    step = 4 

    start = 0
    while (start<10):
        random.seed(start)

        print (start)
        start += 1

        for i in range(np.uint64(0x7FFC000000), np.uint64(0x7FFD000000), step):
            val = ctypes.c_uint32(random.getrandbits(32))
            for j in range(10):
                pci_lib.write_32bit(fd, ctypes.c_uint64(i), val)

def write_and_read(pci_lib, fd):
    step = 4 

    start = 0
    while (start<10):
        random.seed(start)

        print (start)
        start += 1

        for i in range(np.uint64(0x7FFC000000), np.uint64(0x7FFD000000), step):
            #read write 10 times
            val = ctypes.c_uint32(random.getrandbits(32))
            for j in range(10):
                pci_lib.write_32bit(fd, ctypes.c_uint64(i), val)
                read_val = pci_lib.read_32bit(fd, ctypes.c_uint64(i))

def main(argv):
    pci_busid = ""

    pci_lib = ctypes.CDLL('./libno_pci_driver.so')

    pci_busid, read_after_write = get_arguments(argv)
    print ("Opening %s" % pci_busid)

    pci_busid_str = pci_busid.encode('utf-8')

    fd = pci_lib.OpenResourceInDevice(ctypes.c_char_p(pci_busid_str), 2, ctypes.c_size_t(0x8000000), 0)
    if (fd < 0):
        print ("failed to open resource")
        return -1

    if (read_after_write == 1):
        write_and_read(pci_lib, fd)
    else:
        write_only(pci_lib, fd)

    pci_lib.CloseResourceInDevice(fd)

    return 0

if __name__ == "__main__":
    main(sys.argv[1:])

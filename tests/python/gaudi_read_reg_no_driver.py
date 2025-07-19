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
    print ("-p <PCI BDF address> -a <address to read>, "
           "-l <number of times to read (1)> -q (Optional; read 64-bit value)")

def get_arguments(argv):
    pci_busid = ""
    address = ""
    loop = 1
    quad_read = 0
    missing_arguments = 2

    try:
        opts, args = getopt.getopt(argv,"hqp:a:l:",["pci=", "address=", "loop="])
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
        elif opt in ("-l", "--loop"):
            loop = int(arg)
        elif opt in ("-q"):
            quad_read = 1
        else:
            print_usage()
            sys.exit(2)

    if (missing_arguments > 0):
        print ("Not enough arguments!!!")
        print_usage()
        sys.exit(2)

    return pci_busid, address, loop, quad_read

def main(argv):
    pci_busid = ""
    address = ""
    loop = 1

    pci_lib = ctypes.CDLL('./libno_pci_driver.so')

    pci_busid, address, loop, quad_read = get_arguments(argv)
    print ("Opening %s" % pci_busid)

    pci_busid_str = pci_busid.encode('utf-8')

    fd = pci_lib.OpenResourceInDevice(ctypes.c_char_p(pci_busid_str), 2, ctypes.c_size_t(0x8000000), 0)
    if (fd < 0):
        print ("failed to open resource")
        return -1

    if (quad_read == 1):
        while(loop > 0):
            val = pci_lib.read_64bit(fd, ctypes.c_uint64(int(address, 16)))
            print("%#016x" % int(address, 16), "= %#016x" % hex64(val))
            loop -= 1
    else:
        while(loop > 0):
            val = pci_lib.read_32bit(fd, ctypes.c_uint64(int(address, 16)))
            print("%#016x" % int(address, 16), "= %#x" % hex2(val))
            loop -= 1

    pci_lib.CloseResourceInDevice(fd)

    return 0

if __name__ == "__main__":
    main(sys.argv[1:])

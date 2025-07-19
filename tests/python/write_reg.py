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

def print_usage():
    print ("-d <device index in /dev/accel/accelX> -a <address to write>, "
            "-v <value in hex>, -l <number of times to write (1)>, "
            "-r (Optional; for read after write)")

def get_arguments(argv):
    hl_id = 0
    address = ""
    val = ""
    loop = 1
    missing_arguments = 3
    read_after_write = 0

    try:
        opts, args = getopt.getopt(argv,"hrd:a:v:l:",
                                   ["device=", "address=", "val=", "loop="])
    except getopt.GetoptError:
        print_usage()
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            print_usage()
            sys.exit()
        elif opt in ("-d", "--device"):
            hl_id = int(arg)
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
        else:
            print_usage()
            sys.exit(2)

    if (missing_arguments > 0):
        print ("Not enough arguments!!!")
        print_usage()
        sys.exit(2)

    return hl_id, address, val, loop, read_after_write

def main(argv):
    hl_id = 0
    address = ""
    val = ""
    loop = 1

    kmdDeb = ctypes.CDLL('./libkmd_debugfs.so')

    hl_id, address, val, loop, read_after_write = get_arguments(argv)
    print ("Opening /dev/accel/accel%#d" % hl_id)

    rc = kmdDeb.OpenDebugfs(hl_id)
    if (rc == -1):
        print ("failed to open files")
        return -1

    if (read_after_write == 1):
        while(loop > 0):
            kmdDeb.write_cmd(hl_id, ctypes.c_uint64(int(address, 16)), ctypes.c_uint32(int(val, 16)))
            read_val = kmdDeb.read_cmd(hl_id, ctypes.c_uint64(int(address, 16)))
            print("%#016x" % int(address, 16), " = %#x" % hex2(read_val))
            loop -= 1
    else:
        while(loop > 0):
            kmdDeb.write_cmd(hl_id, ctypes.c_uint64(int(address, 16)), ctypes.c_uint32(int(val, 16)))
            print("%#016x" % np.uint64(int(address, 16)), " <= %#x" % np.uint32(int(val, 16)))
            loop -= 1

    kmdDeb.CloseDebugfs(hl_id)

    return 0

if __name__ == "__main__":
    main(sys.argv[1:])

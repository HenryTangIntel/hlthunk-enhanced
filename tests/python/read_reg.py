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
           "-l <number of times to write (1)>")

def get_arguments(argv):
    hl_id = 0
    address = ""
    loop = 1
    missing_arguments = 2

    try:
        opts, args = getopt.getopt(argv,"hd:a:l:",["device=", "address=", "loop="])
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
        elif opt in ("-l", "--loop"):
            loop = int(arg)
        else:
            print_usage()
            sys.exit(2)

    if (missing_arguments > 0):
        print ("Not enough arguments!!!")
        print_usage()
        sys.exit(2)

    return hl_id, address, loop

def main(argv):
    hl_id = 0
    address = ""
    loop = 1

    kmdDeb = ctypes.CDLL('./libkmd_debugfs.so')

    hl_id, address, loop = get_arguments(argv)
    print ("Opening /dev/accel/accel%#d" % hl_id)

    rc = kmdDeb.OpenDebugfs(hl_id)
    if (rc == -1):
        print ("failed to open files")
        return -1

    while(loop > 0):
        val = kmdDeb.read_cmd(hl_id, ctypes.c_uint64(int(address, 16)))
        print("%#016x" % int(address, 16), " = %#x" % hex2(val))
        loop -= 1

    kmdDeb.CloseDebugfs(hl_id)

    return 0

if __name__ == "__main__":
    main(sys.argv[1:])

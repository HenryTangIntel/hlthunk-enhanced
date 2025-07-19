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

def get_dev_id(argv):
    try:
        opts, args = getopt.getopt(argv,"hd:",["device="])
    except getopt.GetoptError:
        print ("cgm.py -d <device index in /dev/accel/accelX>")
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print ("cgm.py -d <device index in /dev/accel/accelX>")
            sys.exit()
        elif opt in ("-d", "--device"):
            return int(arg)
        else:
            print ("cgm.py -d <device index in /dev/accel/accelX>")
            sys.exit(2)

def main(argv):
    hl_id = 0

    kmdDeb = ctypes.CDLL('./libkmd_debugfs.so')

    hl_id = get_dev_id(argv)
    print ("Opening /dev/accel/accel%#d" % hl_id)

    rc = kmdDeb.OpenDebugfs(hl_id)
    if (rc == -1):
        print ("failed to open files")
        return -1

    for i in range(np.uint64(0x7FFC000000), np.uint64(0x8000000000), 4):
        val = kmdDeb.read_cmd(hl_id, ctypes.c_uint64(i))
        print("%#016x" % np.uint64(i), " = %#x" % hex2(val))

    kmdDeb.CloseDebugfs(hl_id)

    return 0

if __name__ == "__main__":
    main(sys.argv[1:])

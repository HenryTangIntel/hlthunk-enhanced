#!/usr/bin/env python
"""
@author: Oded Gabbay

This script enables the CGM of MME0 and then tries to access MME0
registers.
"""

import ctypes
import sys, getopt

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

    val = kmdDeb.read_cmd(hl_id, ctypes.c_uint64(0x7FFC068C78))
    print("MME0 CGM CFG1 = %#x" % hex2(val))
    val = kmdDeb.read_cmd(hl_id, ctypes.c_uint64(0x7FFC068C70))
    print("MME0 CGM CFG = %#08x" % hex2(val))

    print("Enabling CGM for MME0")
    kmdDeb.write_cmd(hl_id, ctypes.c_uint64(0x7FFC068C78), 0xA)
    kmdDeb.write_cmd(hl_id, ctypes.c_uint64(0x7FFC068C70), ctypes.c_uint32(0x8F0A0020))

    val = kmdDeb.read_cmd(hl_id, ctypes.c_uint64(0x7FFC068C78))
    print("MME0 CGM CFG1 = %#x" % hex2(val))
    val = kmdDeb.read_cmd(hl_id, ctypes.c_uint64(0x7FFC068C70))
    print("MME0 CGM CFG = %#08x" % hex2(val))

    for i in range(1):
        val = kmdDeb.write_cmd(hl_id, ctypes.c_uint64(0x7FFC020000), 0x1)

    kmdDeb.CloseDebugfs(hl_id)

    return 0

if __name__ == "__main__":
    main(sys.argv[1:])

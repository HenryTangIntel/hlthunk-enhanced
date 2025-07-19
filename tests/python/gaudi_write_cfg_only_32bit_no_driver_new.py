#!/usr/bin/env python
"""
@author: Oded Gabbay

This script reads every register on the ASIC
"""

import ctypes
import sys, getopt
import numpy as np
import random, argparse
def print_usage():
    print ("-p <PCI BDF address> -r (Optional; for read after write)")

def hex2(n):
    return (n & 0xffffffff)

def check_exclude(addr, exlude_list):
    for start,end in exlude_list:
        if addr >= start and addr < end:
            return False
    return True


def write_and_read(pci_lib, fd, start_address, end_address, loop_num, read_after_write):
    step = 4

    start = 0

    exlude_list = [(np.uint64(0x7FFC500000),np.uint64(0x7FFC600000)), (np.uint64(0x7FFC483000),np.uint64(0x7FFC487000)), (np.uint64(0x7FFC48A000),np.uint64(0x7FFC490000)),
                   (np.uint64(0x7FFC4A3000),np.uint64(0x7FFC4A7000)), (np.uint64(0x7FFC4AA000),np.uint64(0x7FFC4B0000)), (np.uint64(0x7FFC4C3000),np.uint64(0x7FFC4C7000)),
                   (np.uint64(0x7FFC4CA000),np.uint64(0x7FFC4D0000)), (np.uint64(0x7FFC4E3000),np.uint64(0x7FFC4E7000)), (np.uint64(0x7FFC4EA000),np.uint64(0x7FFC4F0000))]
    while (start < loop_num):
        random.seed(start)
        print (start)
        start += 1
        for i in range(np.uint64(start_address), np.uint64(end_address), step):
            if check_exclude(i, exlude_list):
                val = ctypes.c_uint32(random.getrandbits(32))
                for j in range(10):
                    pci_lib.write_32bit(fd, ctypes.c_uint64(i), val)
                    if read_after_write:
                        read_val = pci_lib.read_32bit(fd, ctypes.c_uint64(i))

def main(args):
    pci_lib = ctypes.CDLL('./libno_pci_driver.so')
    read_after_write = args.read_after_write
    pci_busid = args.pci_address
    print ("Opening %s" % pci_busid)

    pci_busid_str = pci_busid.encode('utf-8')

    fd = pci_lib.OpenResourceInDevice(ctypes.c_char_p(pci_busid_str), 2, ctypes.c_size_t(0x8000000), 0)
    if (fd < 0):
        print ("failed to open resource")
        return -1

    write_and_read(pci_lib, fd, int(args.start_address, 16), int(args.end_address, 16), args.loop_num, read_after_write)
    pci_lib.CloseResourceInDevice(fd)

    return 0

if __name__ == "__main__":
    script_name = sys.argv[0]
    example_text = ["usage examples:\n",
                    f"python3.6 ./{script_name} -r -l 10 -s 0x7FFC000000 -e 0x7FFD000000 -p 0000:07:00.0\n",
                    ]

    usage_examples = "\n".join([_.format(script_name) for _ in example_text])
    arg_parser = argparse.ArgumentParser(epilog=usage_examples)
    arg_parser.add_argument('-s', '--start_address',
                            type=str, default='0x7FFC000000',
                            help='Start address')
    arg_parser.add_argument('-e', '--end_address',
                            type=str, default='0x7FFD000000',
                            help='End address')
    arg_parser.add_argument('-r', '--read_after_write',
                            type=bool, default=False, nargs='?', const=True,
                            help='read after write')
    arg_parser.add_argument('-p', '--pci_address',
                            type=str, default=None,
                            help='pci address')
    arg_parser.add_argument('-l', '--loop_num',
                            type=int, default=10,
                            help='loop number, default is 10')
    args = arg_parser.parse_args()
    assert args.pci_address != None
    print(f'start address: {args.start_address}, end address: {args.end_address}, loop: {args.loop_num}, read after write mode: {args.read_after_write}')
    main(args)

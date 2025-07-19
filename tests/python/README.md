# Python scripts to read/write from/to device

There are two libraries:
1. libkmd_debugfs.so - access device through debugfs interface of driver
2. libno_pci_driver.so - access device CFG space with/without driver

Working with the debugfs interface allows you to access all address
space of the device (CFG, SRAM, HBM). However, it limits you in the
size and alignment of the transactions that can be done. i.e. only
32-bit aligned accesses can be made.

Working with the no_pci_driver library allows you to generate
either aligned or unaligned 8/16/32/64-bits transactions, but only to
the CFG bar.

# Building

You need to build the two small C libraries that provides the API
to read and write to the device

```sh
$ make
```

# Running python script

Always run the python script from the same folder the libraries are
located in.

Always run the python script with root permissions (either as root or
as sudo).

For scripts that work with the debugfs library, you need to tell the
script on which /dev/accel/accelX device to work on

For example, this will run the cgm script on device /dev/accel/accel1

```sh
$ sudo python cgm.py --device=1
```

For scripts that work with the no_pci_driver library, you need to tell
the script the full PCI BDF address of the device to work on

For example, this will run the read register script

```sh
$ sudo python gaudi_read_reg_no_driver.py -p 0000:07:00.0 -a 0x7ffc520014
```

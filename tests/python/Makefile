all:
	g++ -std=c++11 -Wall -fPIC -O2 -c kmd_debugfs.cpp
	g++ -std=c++11 -shared -o libkmd_debugfs.so kmd_debugfs.o

	g++ -std=c++11 -Wall -fPIC -O2 -c no_pci_driver.cpp
	g++ -std=c++11 -shared -o libno_pci_driver.so no_pci_driver.o

clean:
	rm -f libkmd_debugfs.so kmd_debugfs.o libno_pci_driver.so no_pci_driver.o

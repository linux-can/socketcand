socketcand
==========

Socketcand is a daemon that provides access to CAN interfaces on a machine via a network interface. The communication protocol uses a TCP/IP connection and a specific protocol to transfer CAN frames and control commands. The protocol specification can be found in ./doc/protocol.md.

Installation
------------

To build and run socketcand make sure you have the following tools installed:

* make
* gcc or another C compiler
* a kernel that includes the SocketCAN modules
* the headers for your kernel version
* the libconfig (with headers)

First run

    $ ./configure

to check your system and create the Makefile. If you want to install scripts for a init system other than SysVinit check the available settings with './configure -h'.
To compile and install the socketcand run

    $ make
    $ make install

Service discovery
-----------------

The daemon uses a simple UDP beacon mechanism for service discovery. A beacon containing the service name, type and address is sent to the broadcast address (port 42000) at minimum every 3 seconds. A client only has to listen for messages of this type to detect all SocketCAN daemons in the local network.

Usage
-----

    socketcand [-v | --verbose] [-i interfaces | --interfaces interfaces] [-p port | --port port] [-l ip_addr | --listen ip_addr] [-h | --help]

###Description of the options
* **-v** activates verbose output to STDOUT
* **-i interfaces** is used to specify the SocketCAN interfaces the daemon shall provide access to
* **-p port** changes the default port (28600) the daemon is listening at
* **-l ip_addr** changes the default ip address (127.0.0.1) the daemon will bind to
* **-h** prints a help message

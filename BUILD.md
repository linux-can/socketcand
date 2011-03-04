To build and run socketcand make sure you have the following tools and packets installed:

* make
* gcc or another C compiler
* a kernel that includes the Socket CAN modules
* the headers for your kernel version
* an init system if you want to run the socketcand as a daemon

To compile the socketcand simply run
    $ make
You can install the socketcand and the manpage in /usr/local and also place a init script in /etc/init.d/ with
    $ make install

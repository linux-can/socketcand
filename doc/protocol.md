Socketcand protocol
===================

The socketcand provides a network interface to a number of CAN busses on the host. It can be controlled over a single TCP socket and supports transmission and reception of CAN frames. The used protocol is ASCII based and has some states in which different commandy may be used.

## Mode NO_BUS ##
After connecting to the socket the client is greeted with '< hi >'. The open command is used to select one of the CAN busses that were announced in the broadcast beacon. The syntax is:
    < open canbus >
where canbus may be at maximum 16 characters long. If the client is allowed to access the bus the server will respond with '< ok >'. Otherwise an error is returned and the connection is terminated.
After a bus was opened the mode is switched to BCM mode. The Mode NO_BUS is the only mode where bittimings or other bus configuration settings may be done.

##### Configure the bittiming #####
The protocol enables the client to change the bittiming of a given bus as provided by set link. Automatic bitrate configuration by the kernel is not supported because it is not guaranteed that the corresponding option was enabled during compile time (e.g. in Ubuntu 10.10 it is not). This way it it also easier to implement the function in a microcontroller based adapter.
    < can0 B bitrate sample_point tq prop_seg phase_seg1 phase_seg2 sjw brp >

##### Set the controlmode #####
The control mode controls if the bus is set to listen only, if sent packages are looped back and if the controller is configured to take three samples. The following command provides access to these settings. Each field must be set to '0' or '1' to disable or enable the setting.

    < can0 C listen_only loopback three_samples >

## Mode BCM ##
After the client has successfully opened a bus the mode is switched to BCM mode. In this mode a BCM socket to the bus will be opened and can be controlled over the connection. The following commands are understood:

### Commands for transmission ###
There are a few commands that control the transmission of CAN frames. Most of them are interval based and the Socket CAN broadcast manager guarantees that the frames are sent cyclic with the given interval. To be able to control these transmission jobs they are automatically removed when the BCM server socket is closed.

##### Add a new frame for transmission #####
This command adds a new frame to the BCM queue. An interval can be configured to have the frame sent cyclic.

Examples:
Send the CAN frame 123#1122334455667788 every second
    < add 1 0 123 8 11 22 33 44 55 66 77 88 >

Send the CAN frame 123#1122334455667788 every 10 usecs
    < add 0 10 123 8 11 22 33 44 55 66 77 88 >

Send the CAN frame 123#42424242 every 20 msecs
    < add 0 20000 123 4 42 42 42 42 >

##### Update a frame #####
This command updates a frame transmission job that was created via the 'add' command with new content. The transmission timers are not touched

Examle:
Update the CAN frame 123#42424242 with 123#112233 - no change of timers
    < update 123 3 11 22 33 >

##### Delete a send job #####
A send job can be removed with the 'delete' command.

Example:
Delete the cyclic send job from above
    < delete 123 >

##### Send a single frame #####
This command is used to send a single CAN frame.
    < send can_id can_dlc [data]* >

Example:
Send a single CAN frame without cyclic transmission
    < send 123 0 >

### Commands for reception ###
The commands for reception are 'subscribe' , 'unsubscribe' and 'filter'.

##### Content filtering #####
This command is used to configure the broadcast manager for reception of frames with a given CAN ID. Frames are only sent when they match the pattern that is provided.

Examples: 
Receive CAN ID 0x123 and check for changes in the first byte
    < filter 0 0 123 1 FF >

Receive CAN ID 0x123 and check for changes in given mask
    < filter 0 0 123 8 FF 00 F8 00 00 00 00 00 >

As above but throttle receive update rate down to 1.5 seconds
    < filter 1 500000 123 8 FF 00 F8 00 00 00 00 00 >

##### Subscribe to CAN ID #####
Adds a subscription a CAN ID. The frames are sent regardless of their content. An interval in seconds or microseconds may be set.
    < subscribe ival_s ival_us can_id >

Example:
Subscribe to CAN ID 0x123 without content filtering
    < subscribe 0 0 123 >

##### Delete a subscription or filter #####
This deletes all subscriptions or filters for a specific CAN ID.
    < unsubscribe can_id >

Example:
Delete receive filter ('R' or 'F') for CAN ID 0x123
    < unsubscribe 123 >

##### Echo command #####
After the server receives an '< echo >' it immediately returns the same string. This can be used to see if the connection is still up and to measure latencies.

##### Switch to RAW mode #####
A mode switch to RAW mode can be initiated by sending '< rawmode >'.

### Frame transmission ###
CAN messages received by the given filters are send in the format:
    < frame can_id seconds.useconds [data]* >

Example:
Reception of a CAN frame with CAN ID 0x123 , data length 4 and data 0x11, 0x22, 0x33 and 0x44 at time 23.424242>
    < frame 123 23.424242 11 22 33 44 >

## Mode RAW ##
After switching to RAW mode the BCM socket is closed and a RAW socket is opened. Now every frame on the bus will immediately be received. Therefore no commands to control which frames are received are supported, but the send command works as in BCM mode.

##### Switch to BCM mode #####
With '< bcmmode >' it is possible to switch back to BCM mode.

##### Echo command #####
The echo command is supported and works as described under mode BCM.

##### Statistics #####
In RAW mode it is possible to receive bus statistics. Transmission is enabled by the '< statistics ival >' command. Ival is the interval between two statistics transmissions in milliseconds. The ival may be set to '0' to deactivate transmission.
After enabling statistics transmission the data is send inline with normal CAN frames and other data. The daemon takes care of the interval that was specified. The information is transfered in the following format:
    < stat rbytes rpackets tbytes tpackets >
The reported bytes and packets are reported as unsigned integers.

Service discovery
-----------------

Because configuration shall be as easy as possible and the virtual CAN bus and the Kayak instance are not necessarily on the same machine a machanism for service discovery is necessary.

The server sends a UDP broadcast beacon to port 42000 on the subnet where the server port was bound. The interval for these discovery beacons shall not be longer than three seconds. Because the BCM server handles all communication (even for multiple busses) over a single TCP connection the broadcast must provide information about all busses that are accessible through the BCM server.

### Content ###

Required:

* Name of the device that provides access to the busses. On linux machines this could be the hostname
* Name of the busses (in case of socketCAN and embedded this should be the same as the device name)
* URL with port and IP address. If the server is listening on multiple sockets all of them should be included in the beacon
* Device type the service is running on

Optional:

* Description of the service in a human readable form

### Device types ####

* SocketCAN - general socketCAN service on a linux machine
* embedded - embedded linux with access to a bus over socketCAN
* adapter - e.g. microcontroller driven CAN to ethernet adapter

### Structure ###

For simple parsing and a human readable schema XML is used to structure the information in a CAN beacon.

### Example ###

    <CANBeacon name="HeartOfGold" type="SocketCAN" description="A human readable description">
        <URL>can://127.0.0.1:28600</URL>
        <Bus name="vcan0"/>
        <Bus name="vcan1"/>
    </CANBeacon>

Error frame transmission
------------------------

Error frames are sent inline with regular frames. 'data' is an integer with the encoded error data (see socketcan/can/error.h for further information):
    < error data >


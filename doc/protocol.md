Socketcand protocol
===================

The socketcand provides a network interface to a number of CAN busses on the host. It can be controlled over a single TCP socket and supports transmission and reception of CAN frames. The used protocol is ASCII based and has some states in which different commands may be used.

## Mode NO_BUS ##
After connecting to the socket the client is greeted with '< hi >'. The open command is used to select one of the CAN busses that were announced in the broadcast beacon. The syntax is '<open canbus>', e.g.

    < open can0 >

where canbus may be at maximum 16 characters long. If the client is allowed to access the bus the server will respond with '< ok >'. Otherwise an error is returned and the connection is terminated.
After a bus was opened the mode is switched to BCM mode. The Mode NO_BUS is the only mode where bittimings or other bus configuration settings may be done.

#### Configure the bittiming (to be implemented) ####
The protocol enables the client to change the bittiming of a given bus as provided by set link. Automatic bitrate configuration by the kernel is not supported because it is not guaranteed that the corresponding option was enabled during compile time (e.g. in Ubuntu 10.10 it is not). This way it it also easier to implement the function in a microcontroller based adapter.

    < can0 B bitrate sample_point tq prop_seg phase_seg1 phase_seg2 sjw brp >

#### Set the controlmode (to be implementend) ####
The control mode controls if the bus is set to listen only, if sent packages are looped back and if the controller is configured to take three samples. The following command provides access to these settings. Each field must be set to '0' or '1' to disable or enable the setting.

    < can0 C listen_only loopback three_samples >

## Mode BCM (default mode) ##
After the client has successfully opened a bus the mode is switched to BCM mode (DEFAULT). In this mode a BCM socket to the bus will be opened and can be controlled over the connection. The following commands are understood:

### Commands for transmission ###
There are a few commands that control the transmission of CAN frames. Most of them are interval based and the Socket CAN broadcast manager guarantees that the frames are sent cyclic with the given interval. To be able to control these transmission jobs they are automatically removed when the BCM server socket is closed.

#### Add a new frame for transmission ####
This command adds a new frame to the BCM queue. An interval can be configured to have the frame sent cyclic.

Examples:

Send the CAN frame 123#1122334455667788 every second

    < add 1 0 123 8 11 22 33 44 55 66 77 88 >

Send the CAN frame 123#1122334455667788 every 10 usecs

    < add 0 10 123 8 11 22 33 44 55 66 77 88 >

Send the CAN frame 123#42424242 every 20 msecs

    < add 0 20000 123 4 42 42 42 42 >

#### Update a frame ####
This command updates a frame transmission job that was created via the 'add' command with new content. The transmission timers are not touched

Examle:
Update the CAN frame 123#42424242 with 123#112233 - no change of timers

    < update 123 3 11 22 33 >

#### Delete a send job ####
A send job can be removed with the 'delete' command.

Example:
Delete the cyclic send job from above

    < delete 123 >

#### Send a single frame ####
This command is used to send a single CAN frame.

    < send can_id can_dlc [data]* >

Example:
Send a single CAN frame without cyclic transmission

    // ID: 123, no data
    < send 123 0 > 
    
    // ID: 1AAAAAAA, Length: 2, Data: 0x01 0xF1
    < send 1AAAAAAA 2 1 F1 > 

### Commands for reception ###
The commands for reception are 'subscribe' , 'unsubscribe' and 'filter'.

#### Content filtering ####
This command is used to configure the broadcast manager for reception of frames with a given CAN ID. Frames are only sent when they match the pattern that is provided. The time value given is used to throttle the incoming update rate.

    < filter secs usecs can_id can_dlc [data]* >

* secs - number of seconds (throttle update rate)
* usecs - number of microseconds (throttle update rate)
* can_id - CAN identifier
* can_dlc - data length code (values 0 .. 8)
* data - ASCII hex bytes depending on the can_dlc value

Examples:

Receive CAN ID 0x123 and check for changes in the first byte

    < filter 0 0 123 1 FF >

Receive CAN ID 0x123 and check for changes in given mask

    < filter 0 0 123 8 FF 00 F8 00 00 00 00 00 >

As above but throttle receive update rate down to 1.5 seconds

    < filter 1 500000 123 8 FF 00 F8 00 00 00 00 00 >

#### Content filtering for multiplex CAN messages ####
This command is used to configure the broadcast manager for reception of frames with a given CAN ID and a multiplex message filter mask. Frames are only sent when they match the pattern that is provided. The time value given is used to throttle the incoming update rate.


    < muxfilter secs usecs can_id nframes [data]+ >

* secs - number of seconds (throttle update rate)
* usecs - number of microseconds (throttle update rate)
* can_id - CAN identifier
* nframes - number of 8 byte filter tuples (one mux filter + 1..256 mux content filters)
* data - ASCII hex bytes depending on the nframes value multiplied by 8

Examples:

The first 8 byte tuple is the multiplex mask which identifies the position of multiplex identifier. The following 8 byte tuples contain the value of the multiplex id at this position and the filter for that specific multiplex identifier in the remaining content of its 8 byte tuple.

    FF 00 00 00 00 00 00 00 <--> multiplex mask (here: entire first byte)
    33 FF FF FF FF FF FF FF <--> filter mask 'FF FF FF FF FF FF FF' for multiplex id '33'
    56 FF 00 00 00 00 FF FF <--> filter mask 'FF 00 00 00 00 FF FF' for multiplex id '56'
    44 FF FF FF FF 00 00 FF <--> filter mask 'FF FF FF FF 00 00 FF' for multiplex id '44'
    ED 00 00 00 00 00 FF FF <--> filter mask '00 00 00 00 00 FF FF' for multiplex id 'ED'

Receive CAN ID 0x123 and check for changes in given mutiplex mask '33'. The fourth value '2' is the number of frames (2 <= nframes <= 257):

    < muxfilter 0 0 123 2 FF 00 00 00 00 00 00 00 33 FF FF FF FF FF FF FF >

#### Subscribe to CAN ID ####
Adds a subscription a CAN ID. The frames are sent regardless of their content. An interval in seconds or microseconds may be set.

    < subscribe ival_s ival_us can_id >

Example:
Subscribe to CAN ID 0x123 without content filtering

    < subscribe 0 0 123 >

#### Delete a subscription or filter ####
This deletes all subscriptions or filters for a specific CAN ID.

    < unsubscribe can_id >

Example:
Delete receive filter ('R' or 'F') for CAN ID 0x123

    < unsubscribe 123 >

#### Echo command ####
After the server receives an '< echo >' it immediately returns the same string. This can be used to see if the connection is still up and to measure latencies.

    < echo >

## Mode RAW ##
After switching to RAW mode the BCM socket is closed and a RAW socket is opened. Now every frame on the bus will immediately be received. Therefore no commands to control which frames are received are supported, but the send command works as in BCM mode.

#### Switch to RAW mode ####
A mode switch to RAW mode can be initiated by sending '< rawmode >'.

    < rawmode >

#### Frame transmission ####
CAN messages received by the given filters are send in the format:
    < frame can_id seconds.useconds [data]* >

Example:
Reception of a CAN frame with CAN ID 0x123 , data length 4 and data 0x11, 0x22, 0x33 and 0x44 at time 23.424242>

    < frame 123 23.424242 11 22 33 44 >

#### Switch to BCM mode ####
With '< bcmmode >' it is possible to switch back to BCM mode.

    < bcmmode >

#### Echo command ####
The echo command is supported and works as described under mode BCM.

## Mode CONTROL ##
With '< controlmode >' it is possible to enter the CONTROL mode. Here statistics can be enabled or disabled.

#### Statistics ####
In CONTROL mode it is possible to receive bus statistics. Transmission is enabled by the '< statistics ival >' command. Ival is the interval between two statistics transmissions in milliseconds. The ival may be set to '0' to deactivate transmission.
After enabling statistics transmission the data is send inline with normal CAN frames and other data. The daemon takes care of the interval that was specified. The information is transfered in the following format:
    < stat rbytes rpackets tbytes tpackets >
The reported bytes and packets are reported as unsigned integers.

Example for CAN interface 'can0' to enable statistics with interval of one second:

    < open can0 >< controlmode >< statistics 1000 >

## Mode ISO-TP ##
A transport protocol, such as ISO-TP, is needed to enable e.g. software updload via CAN. It organises the connection-less transmission of a sequence of data. An ISO-TP channel consists of two exclusive CAN IDs, one to transmit data and the other to receive data.
After configuration a single ISO-TP channel can be used. The ISO-TP mode can be used exclusively like the other modes (bcmmode, rawmode, isotpmode).

Switch to ISO-TP mode

    < isotpmode >

Configure the ISO-TP channel - optional parameters are in [ ] brackets.

    < isotpconf tx_id rx_id flags blocksize stmin [ wftmax txpad_content rxpad_content ext_address rx_ext_address ] >

* tx_id - CAN ID of channel to transmit data (from the host / src). CAN IDs 000h up to 7FFh (standard frame format) and 00000000h up to 1FFFFFFFh (extended frame format).
* rx_id - CAN ID of channel to receive data (to the host / dst). CAN IDs in same format as tx_id.
* flags - hex value built from the original flags from isotp.h. These flags define which of the following parameters is used/required in which way.
* blocksize - can take values from 0 (=off) to 15
* stmin - separation time minimum. Hex value from 00h - FFh according to ISO-TP specification
* wftmax - maximum number of wait frames (0 = off)
* txpad_content - padding value in the tx path (enable CAN_ISOTP_TX_PADDING in flags)
* rxpad_content - padding value in the rx path (enable CAN_ISOTP_RX_PADDING in flags)
* ext_address - extended adressing freature (value for tx and rx if not specified separately / enable CAN_ISOTP_EXTEND_ADDR in flags)
* rx_ext_address - extended adressing freature (separate value for rx / enable CAN_ISOTP_RX_EXT_ADDR in flags)

The flags contents are built from the original isotp.h file:

    #define CAN_ISOTP_LISTEN_MODE   0x001   /* listen only (do not send FC) */
    #define CAN_ISOTP_EXTEND_ADDR   0x002   /* enable extended addressing */
    #define CAN_ISOTP_TX_PADDING    0x004   /* enable CAN frame padding tx path */
    #define CAN_ISOTP_RX_PADDING    0x008   /* enable CAN frame padding rx path */
    #define CAN_ISOTP_CHK_PAD_LEN   0x010   /* check received CAN frame padding */
    #define CAN_ISOTP_CHK_PAD_DATA  0x020   /* check received CAN frame padding */
    #define CAN_ISOTP_HALF_DUPLEX   0x040   /* half duplex error state handling */
    #define CAN_ISOTP_FORCE_TXSTMIN 0x080   /* ignore stmin from received FC */
    #define CAN_ISOTP_FORCE_RXSTMIN 0x100   /* ignore CFs depending on rx stmin */
    #define CAN_ISOTP_RX_EXT_ADDR   0x200   /* different rx extended addressing */


Example: Configuration with 1F998877h as source and 1F998876h as destination id for this specific ISO-TP channel (host view) with tx padding AAh and rx padding 55h. The flags are calculated by adding CAN_ISOTP_TX_PADDING (4) and CAN_ISOTP_RX_PADDING (8) to the hex value Ch. The Blocksize is 4 and STmin is 0.

    < isotpconf 1F998877 1F998876 C 4 0 0 AA 55 >

As one can see only the needed parameters need to be provided, e.g. a set CAN_ISOTP_RX_PADDING flag requires the rxpad_content. The parameters up to stmin are mandatory, so the shortest isotpconf can be:

    < isotpconf 1F998877 1F998876 0 0 0 >

To send a protocol data unit (PDU) use the '< sendpdu ... >' command.

    < sendpdu pdudata >

Example: Sending of a PDU consisting of 16 bytes data 00112233445566778899AABBCCDDEEFF

    < sendpdu 00112233445566778899AABBCCDDEEFF >

Receiving of a PDU on the same channel is quite similar but is supplemented by a timestamp

    < pdu timestamp pdudata >

Example: Receiving of the same data as sent in the example above

    < pdu 1417687245.814579 00112233445566778899AABBCCDDEEFF >

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
        <URL>can://127.0.0.1:29536</URL>
        <Bus name="vcan0"/>
        <Bus name="vcan1"/>
    </CANBeacon>

Error frame transmission
------------------------

Error frames are sent inline with regular frames. 'data' is an integer with the encoded error data (see socketcan/can/error.h for further information):
    < error data >


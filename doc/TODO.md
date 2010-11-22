
* It shall be possible to subscribe to all frames that are on the bus. This is necessary if Kayak has no information about the messages on the bus and we simply want to display all frames that drop in. This was implemented, for example, in the cansniffer ( http://svn.berlios.de/wsvn/socketcan/trunk/can-utils/cansniffer.c ) were the BCM subscribes to all 
2048 (7FFh) 11-Bit identifiers via a for-loop. This is pragmatic solution, but will take longer for 29-Bit identifiers. 
Alternatively Kayak may use the RAW socket for simple dump functionality, showing the whole CAN bus traffic and the BCM functionality for closer inspection of messages and signals of interest.


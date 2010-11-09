socketcand
==========

Socketcand is a daemon that provides access to CAN interfaces on a machine via a network interface. The communication protocol uses a TCP/IP connection and a specific protocol to transfer CAN frames and control commands. The protocol specification can be found in ./doc/protocol.md.

Service discovery
-----------------

The daemon uses a simple UDP beacon mechanism for service discovery. A beacon containing the service name, type and address is sent to the broadcast address (port 42000) at minimum every 3 seconds. A client only has to listen for messages of this type to detect all Socket CAN daemons in the local network.    

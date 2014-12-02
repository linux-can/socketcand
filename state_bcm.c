#include "config.h"
#include "socketcand.h"
#include "statistics.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>

#include <linux/can.h>
#include <linux/can/bcm.h>
#include <linux/can/error.h>

#define RXLEN 128

int sc = -1;
fd_set readfds;
struct timeval tv;

inline void state_bcm() {
	int i, ret;
	struct sockaddr_can caddr;
	socklen_t caddrlen = sizeof(caddr);
	struct ifreq ifr;
	char rxmsg[RXLEN];
	char buf[MAXLEN];

	struct {
		struct bcm_msg_head msg_head;
		struct can_frame frame;
	} msg;

	if(previous_state != STATE_BCM) {
		/* open BCM socket */
		if ((sc = socket(PF_CAN, SOCK_DGRAM, CAN_BCM)) < 0) {
			PRINT_ERROR("Error while opening BCM socket %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}

		memset(&caddr, 0, sizeof(caddr));
		caddr.can_family = PF_CAN;
		/* can_ifindex is set to 0 (any device) => need for sendto() */

		PRINT_VERBOSE("connecting BCM socket...\n")
			if (connect(sc, (struct sockaddr *)&caddr, sizeof(caddr)) < 0) {
				PRINT_ERROR("Error while connecting BCM socket %s\n", strerror(errno));
				state = STATE_SHUTDOWN;
				return;
			}
		previous_state = STATE_BCM;
	}

	FD_ZERO(&readfds);
	FD_SET(sc, &readfds);
	FD_SET(client_socket, &readfds);

	/*
	 * Check if there are more elements in the element buffer before calling select() and
	 * blocking for new packets.
	 */
	if(more_elements) {
		FD_CLR(sc, &readfds);
	} else {
		ret = select((sc > client_socket)?sc+1:client_socket+1, &readfds, NULL, NULL, NULL);

		if(ret < 0) {
			PRINT_ERROR("Error in select()\n")
				state = STATE_SHUTDOWN;
			return;
		}
	}

	if (FD_ISSET(sc, &readfds)) {

		ret = recvfrom(sc, &msg, sizeof(msg), 0,
			       (struct sockaddr*)&caddr, &caddrlen);

		/* read timestamp data */
		if(ioctl(sc, SIOCGSTAMP, &tv) < 0) {
			PRINT_ERROR("Could not receive timestamp\n");
		}

		/* Check if this is an error frame */
		if(msg.msg_head.can_id & CAN_ERR_FLAG) {
			if(msg.frame.can_dlc != CAN_ERR_DLC) {
				PRINT_ERROR("Error frame has a wrong DLC!\n")
					} else {
				snprintf(rxmsg, RXLEN, "< error %03X %ld.%06ld ", msg.msg_head.can_id, tv.tv_sec, tv.tv_usec);

				for ( i = 0; i < msg.frame.can_dlc; i++)
					snprintf(rxmsg + strlen(rxmsg), RXLEN - strlen(rxmsg), "%02X ",
						 msg.frame.data[i]);

				snprintf(rxmsg + strlen(rxmsg), RXLEN - strlen(rxmsg), " >");
				send(client_socket, rxmsg, strlen(rxmsg), 0);
			}
		} else {
			if(msg.msg_head.can_id & CAN_EFF_FLAG) {
				snprintf(rxmsg, RXLEN, "< frame %08X %ld.%06ld ",
					 msg.msg_head.can_id & CAN_EFF_MASK, tv.tv_sec, tv.tv_usec);
			} else {
				snprintf(rxmsg, RXLEN, "< frame %03X %ld.%06ld ",
					 msg.msg_head.can_id & CAN_SFF_MASK, tv.tv_sec, tv.tv_usec);
			}

			for ( i = 0; i < msg.frame.can_dlc; i++)
				snprintf(rxmsg + strlen(rxmsg), RXLEN - strlen(rxmsg), "%02X ",
					 msg.frame.data[i]);

			snprintf(rxmsg + strlen(rxmsg), RXLEN - strlen(rxmsg), " >");
			send(client_socket, rxmsg, strlen(rxmsg), 0);
		}
	}

	if (FD_ISSET(client_socket, &readfds)) {
		int items;

		ret = receive_command(client_socket, buf);

		if(ret != 0) {
			state = STATE_SHUTDOWN;
			return;
		}

		/* prepare bcm message settings */
		memset(&msg, 0, sizeof(msg));
		msg.msg_head.nframes = 1;

		strncpy(ifr.ifr_name, bus_name, IFNAMSIZ);

		if (state_changed(buf, state)) {
			close(sc);
			strcpy(buf, "< ok >");
			send(client_socket, buf, strlen(buf), 0);
			return;
		}

		if(!strcmp("< echo >", buf)) {
			send(client_socket, buf, strlen(buf), 0);
			return;
		}

		/* Send a single frame */
		if(!strncmp("< send ", buf, 7)) {
			items = sscanf(buf, "< %*s %x %hhu "
				       "%hhx %hhx %hhx %hhx %hhx %hhx "
				       "%hhx %hhx >",
				       &msg.msg_head.can_id,
				       &msg.frame.can_dlc,
				       &msg.frame.data[0],
				       &msg.frame.data[1],
				       &msg.frame.data[2],
				       &msg.frame.data[3],
				       &msg.frame.data[4],
				       &msg.frame.data[5],
				       &msg.frame.data[6],
				       &msg.frame.data[7]);

			if ( (items < 2) ||
			     (msg.frame.can_dlc > 8) ||
			     (items != 2 + msg.frame.can_dlc)) {
				PRINT_ERROR("Syntax error in send command\n")
					return;
			}
            
			/* check if this is standard or extended identifier */
			int start=0;
			int stop=0;
			int curr=0;
			int i=0;
			for(;i<strlen(buf);i++) {
				if(buf[i] == ' ') {

					switch(curr) {
					case 1:
						start=i;
						break;
					case 2:
						stop=i;
						break;
					default:
						break;
					}
					curr++;
				}
			}

			if((stop-start) > 4) {
				msg.msg_head.can_id |= CAN_EFF_FLAG;
			}

			msg.msg_head.opcode = TX_SEND;
			msg.frame.can_id = msg.msg_head.can_id;

			if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
				caddr.can_ifindex = ifr.ifr_ifindex;
				sendto(sc, &msg, sizeof(msg), 0,
				       (struct sockaddr*)&caddr, sizeof(caddr));
			}
			/* Add a send job */
		} else if(!strncmp("< add ", buf, 6)) {
			items = sscanf(buf, "< %*s %lu %lu %x %hhu "
				       "%hhx %hhx %hhx %hhx %hhx %hhx "
				       "%hhx %hhx >",
				       &msg.msg_head.ival2.tv_sec,
				       &msg.msg_head.ival2.tv_usec,
				       &msg.msg_head.can_id,
				       &msg.frame.can_dlc,
				       &msg.frame.data[0],
				       &msg.frame.data[1],
				       &msg.frame.data[2],
				       &msg.frame.data[3],
				       &msg.frame.data[4],
				       &msg.frame.data[5],
				       &msg.frame.data[6],
				       &msg.frame.data[7]);

			if( (items < 4) ||
			    (msg.frame.can_dlc > 8) ||
			    (items != 4 + msg.frame.can_dlc) ) {
				PRINT_ERROR("Syntax error in add command.\n");
				return;
			}
            
			/* check if this is standard or extended identifier */
			int start=0;
			int stop=0;
			int curr=0;
			int i=0;
			for(;i<strlen(buf);i++) {
				if(buf[i] == ' ') {

					switch(curr) {
					case 3:
						start=i;
						break;
					case 4:
						stop=i;
						break;
					default:
						break;
					}
					curr++;
				}
			}

			if((stop-start) > 4) {
				msg.msg_head.can_id |= CAN_EFF_FLAG;
			}

			msg.msg_head.opcode = TX_SETUP;
			msg.msg_head.flags |= SETTIMER | STARTTIMER;
			msg.frame.can_id = msg.msg_head.can_id;

			if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
				caddr.can_ifindex = ifr.ifr_ifindex;
				sendto(sc, &msg, sizeof(msg), 0,
				       (struct sockaddr*)&caddr, sizeof(caddr));
			}
			/* Update send job */
		} else if(!strncmp("< update ", buf, 9)) {
			items = sscanf(buf, "< %*s %x %hhu "
				       "%hhx %hhx %hhx %hhx %hhx %hhx "
				       "%hhx %hhx >",
				       &msg.msg_head.can_id,
				       &msg.frame.can_dlc,
				       &msg.frame.data[0],
				       &msg.frame.data[1],
				       &msg.frame.data[2],
				       &msg.frame.data[3],
				       &msg.frame.data[4],
				       &msg.frame.data[5],
				       &msg.frame.data[6],
				       &msg.frame.data[7]);

			if ( (items < 2) ||
			     (msg.frame.can_dlc > 8) ||
			     (items != 2 + msg.frame.can_dlc)) {
				PRINT_ERROR("Syntax error in update send job command\n")
					return;
			}
            
			/* check if this is standard or extended identifier */
			int start=0;
			int stop=0;
			int curr=0;
			int i=0;
			for(;i<strlen(buf);i++) {
				if(buf[i] == ' ') {

					switch(curr) {
					case 1:
						start=i;
						break;
					case 2:
						stop=i;
						break;
					default:
						break;
					}
					curr++;
				}
			}

			if((stop-start) > 4) {
				msg.msg_head.can_id |= CAN_EFF_FLAG;
			}

			msg.msg_head.opcode = TX_SETUP;
			msg.msg_head.flags  = 0;
			msg.frame.can_id = msg.msg_head.can_id;

			if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
				caddr.can_ifindex = ifr.ifr_ifindex;
				sendto(sc, &msg, sizeof(msg), 0,
				       (struct sockaddr*)&caddr, sizeof(caddr));
			}
			/* Delete a send job */
		} else if(!strncmp("< delete ", buf, 9)) {
			items = sscanf(buf, "< %*s %x >",
				       &msg.msg_head.can_id);

			if (items != 1)  {
				PRINT_ERROR("Syntax error in delete job command\n")
					return;
			}
            
			/* check if this is standard or extended identifier */
			int start=0;
			int stop=0;
			int curr=0;
			int i=0;
			for(;i<strlen(buf);i++) {
				if(buf[i] == ' ') {

					switch(curr) {
					case 1:
						start=i;
						break;
					case 2:
						stop=i;
						break;
					default:
						break;
					}
					curr++;
				}
			}
            
			if((stop-start) > 4) {
				msg.msg_head.can_id |= CAN_EFF_FLAG;
			}

			msg.msg_head.opcode = TX_DELETE;
			msg.frame.can_id = msg.msg_head.can_id;

			if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
				caddr.can_ifindex = ifr.ifr_ifindex;
				sendto(sc, &msg, sizeof(msg), 0,
				       (struct sockaddr*)&caddr, sizeof(caddr));
			}
			/* Receive CAN ID with content matching */
		} else if(!strncmp("< filter ", buf, 9)) {
			items = sscanf(buf, "< %*s %lu %lu %x %hhu "
				       "%hhx %hhx %hhx %hhx %hhx %hhx "
				       "%hhx %hhx >",
				       &msg.msg_head.ival2.tv_sec,
				       &msg.msg_head.ival2.tv_usec,
				       &msg.msg_head.can_id,
				       &msg.frame.can_dlc,
				       &msg.frame.data[0],
				       &msg.frame.data[1],
				       &msg.frame.data[2],
				       &msg.frame.data[3],
				       &msg.frame.data[4],
				       &msg.frame.data[5],
				       &msg.frame.data[6],
				       &msg.frame.data[7]);

			if( (items < 4) ||
			    (msg.frame.can_dlc > 8) ||
			    (items != 4 + msg.frame.can_dlc) ) {
				PRINT_ERROR("syntax error in filter command.\n")
					return;
			}
            
			/* check if this is standard or extended identifier */
			int start=0;
			int stop=0;
			int curr=0;
			int i=0;
			for(;i<strlen(buf);i++) {
				if(buf[i] == ' ') {

					switch(curr) {
					case 3:
						start=i;
						break;
					case 4:
						stop=i;
						break;
					default:
						break;
					}
					curr++;
				}
			}
            
			if((stop-start) > 4) {
				msg.msg_head.can_id |= CAN_EFF_FLAG;
			}

			msg.msg_head.opcode = RX_SETUP;
			msg.msg_head.flags  = SETTIMER;
			msg.frame.can_id = msg.msg_head.can_id;

			if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
				caddr.can_ifindex = ifr.ifr_ifindex;
				sendto(sc, &msg, sizeof(msg), 0,
				       (struct sockaddr*)&caddr, sizeof(caddr));
			}
			/* Add a filter */
		} else if(!strncmp("< subscribe ", buf, 12)) {
			items = sscanf(buf, "< %*s %lu %lu %x >",
				       &msg.msg_head.ival2.tv_sec,
				       &msg.msg_head.ival2.tv_usec,
				       &msg.msg_head.can_id);

			if (items != 3) {
				PRINT_ERROR("syntax error in subscribe command\n")
					return;
			}
            
			/* check if this is standard or extended identifier */
			int start=0;
			int stop=0;
			int curr=0;
			int i=0;
			for(;i<strlen(buf);i++) {
				if(buf[i] == ' ') {

					switch(curr) {
					case 3:
						start=i;
						break;
					case 4:
						stop=i;
						break;
					default:
						break;
					}
					curr++;
				}
			}
            
			if((stop-start) > 4) {
				msg.msg_head.can_id |= CAN_EFF_FLAG;
			}

			msg.msg_head.opcode = RX_SETUP;
			msg.msg_head.flags  = RX_FILTER_ID | SETTIMER;
			msg.frame.can_id = msg.msg_head.can_id;

			if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
				caddr.can_ifindex = ifr.ifr_ifindex;
				sendto(sc, &msg, sizeof(msg), 0,
				       (struct sockaddr*)&caddr, sizeof(caddr));
			}
			/* Delete filter */
		} else if(!strncmp("< unsubscribe ", buf, 14)) {
			items = sscanf(buf, "< %*s %x >",
				       &msg.msg_head.can_id);

			if (items != 1) {
				PRINT_ERROR("syntax error in unsubscribe command\n")
					return;
			}

			/* check if this is standard or extended identifier */
			int start=0;
			int stop=0;
			int curr=0;
			int i=0;
			for(;i<strlen(buf);i++) {
				if(buf[i] == ' ') {

					switch(curr) {
					case 1:
						start=i;
						break;
					case 2:
						stop=i;
						break;
					default:
						break;
					}
					curr++;
				}
			}
            
			if((stop-start) > 4) {
				msg.msg_head.can_id |= CAN_EFF_FLAG;
			}
			msg.msg_head.opcode = RX_DELETE;
			msg.frame.can_id = msg.msg_head.can_id;
            
			if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
				caddr.can_ifindex = ifr.ifr_ifindex;
				sendto(sc, &msg, sizeof(msg), 0,
				       (struct sockaddr*)&caddr, sizeof(caddr));
			}
		} else {
			PRINT_ERROR("unknown command '%s'.\n", buf)
				strcpy(buf, "< error unknown command >");
			send(client_socket, buf, strlen(buf), 0);
		}
	}
}

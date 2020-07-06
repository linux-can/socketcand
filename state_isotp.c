/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */

#include "config.h"
#include "socketcand.h"

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
#include <linux/can/isotp.h>
#include <linux/can/error.h>
#include <linux/sockios.h>

int si = -1;

void state_isotp() {
	int i, items, ret;

	struct sockaddr_can addr;
	struct ifreq ifr;
	static struct can_isotp_options opts;
	static struct can_isotp_fc_options fcopts;

	char rxmsg[MAXLEN]; /* can to inet */
	char buf[MAXLEN]; /* inet commands to can */
	unsigned char isobuf[ISOTPLEN+1]; /* binary buffer for isotp socket */
	unsigned char tmp;
	fd_set readfds;
	
	while(previous_state != STATE_ISOTP) {

		ret = receive_command(client_socket, buf);
		if(ret != 0) {
			state = STATE_SHUTDOWN;
			return;
		}

		strncpy(ifr.ifr_name, bus_name, IFNAMSIZ);

		if (state_changed(buf, state)) {
			/* ensure proper handling in other states */
			previous_state = STATE_ISOTP;
			strcpy(buf, "< ok >");
			send(client_socket, buf, strlen(buf), 0);
			return;
		}

		if(!strcmp("< echo >", buf)) {
			send(client_socket, buf, strlen(buf), 0);
			continue;
		}

		memset(&opts, 0, sizeof(opts));
		memset(&fcopts, 0, sizeof(fcopts));
		memset(&addr, 0, sizeof(addr));

		/* get configuration to open the socket */
		if(!strncmp("< isotpconf ", buf, 12)) {
			items = sscanf(buf, "< %*s %x %x %x "
				       "%hhu %hhx %hhu "
				       "%hhx %hhx %hhx %hhx >",
				       &addr.can_addr.tp.tx_id,
				       &addr.can_addr.tp.rx_id,
				       &opts.flags,
				       &fcopts.bs,
				       &fcopts.stmin,
				       &fcopts.wftmax,
				       &opts.txpad_content,
				       &opts.rxpad_content,
				       &opts.ext_address,
				       &opts.rx_ext_address);

			/* < isotpconf XXXXXXXX ... > check for extended identifier */
			if(element_length(buf, 2) == 8)
				addr.can_addr.tp.tx_id |= CAN_EFF_FLAG;

			if(element_length(buf, 3) == 8)
				addr.can_addr.tp.rx_id |= CAN_EFF_FLAG;

			if ((opts.flags & CAN_ISOTP_RX_EXT_ADDR && items < 10) ||
			    (opts.flags & CAN_ISOTP_EXTEND_ADDR && items < 9) ||
			    (opts.flags & CAN_ISOTP_RX_PADDING && items < 8) ||
			    (opts.flags & CAN_ISOTP_TX_PADDING && items < 7) ||
			    (items < 5)) {
				PRINT_ERROR("Syntax error in isotpconf command\n");
				/* try it once more */
				continue;
			}

			/* open ISOTP socket */
			if ((si = socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP)) < 0) {
				PRINT_ERROR("Error while opening ISOTP socket %s\n", strerror(errno));
				/* ensure proper handling in other states */
				previous_state = STATE_ISOTP;
				state = STATE_SHUTDOWN;
				return;
			}

			strcpy(ifr.ifr_name, bus_name);
			if(ioctl(si, SIOCGIFINDEX, &ifr) < 0) {
				PRINT_ERROR("Error while searching for bus %s\n", strerror(errno));
				/* ensure proper handling in other states */
				previous_state = STATE_ISOTP;
				state = STATE_SHUTDOWN;
				return;
			}

			addr.can_family = PF_CAN;
			addr.can_ifindex = ifr.ifr_ifindex;

			/* only change the built-in defaults when required */
			if (opts.flags)
				setsockopt(si, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &opts, sizeof(opts));

			setsockopt(si, SOL_CAN_ISOTP, CAN_ISOTP_RECV_FC, &fcopts, sizeof(fcopts));

			PRINT_VERBOSE("binding ISOTP socket...\n")
			if (bind(si, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
				PRINT_ERROR("Error while binding ISOTP socket %s\n", strerror(errno));
				/* ensure proper handling in other states */
				previous_state = STATE_ISOTP;
				state = STATE_SHUTDOWN;
				return;
			}
			
			/* ok we made it and have a proper isotp socket open */
			previous_state = STATE_ISOTP;
		}
	} /* while */

	FD_ZERO(&readfds);
	FD_SET(si, &readfds);
	FD_SET(client_socket, &readfds);

	/*
	 * Check if there are more elements in the element buffer before calling select() and
	 * blocking for new packets.
	 */
	if(more_elements) {
		FD_CLR(si, &readfds);
	} else {
		ret = select((si > client_socket)?si+1:client_socket+1, &readfds, NULL, NULL, NULL);
		if(ret < 0) {
			PRINT_ERROR("Error in select()\n")
				state = STATE_SHUTDOWN;
			return;
		}
	}

	if (FD_ISSET(si, &readfds)) {

		struct timeval tv = {0};

		items = read(si, isobuf, ISOTPLEN);

		/* read timestamp data */
		if(ioctl(si, SIOCGSTAMP, &tv) < 0) {
			PRINT_ERROR("Could not receive timestamp\n");
		}

		if (items > 0 && items <= ISOTPLEN) {

			int startlen;

			sprintf(rxmsg, "< pdu %ld.%06ld ", tv.tv_sec, tv.tv_usec);
			startlen = strlen(rxmsg);

			for (i=0; i < items; i++)
				sprintf(rxmsg + startlen + 2*i, "%02X", isobuf[i]);

			sprintf(rxmsg + strlen(rxmsg), " >");
			send(client_socket, rxmsg, strlen(rxmsg), 0);
		}
	}

	if (FD_ISSET(client_socket, &readfds)) {

		ret = receive_command(client_socket, buf);
		if(ret != 0) {
			state = STATE_SHUTDOWN;
			return;
		}

		if (state_changed(buf, state)) {
			close(si);
			strcpy(buf, "< ok >");
			send(client_socket, buf, strlen(buf), 0);
			return;
		}

		if(!strcmp("< echo >", buf)) {
			send(client_socket, buf, strlen(buf), 0);
			return;
		}

		if(!strncmp("< sendpdu ", buf, 10)) {
			items = element_length(buf, 2);
			if (items & 1) {
				PRINT_ERROR("odd number of ASCII Hex values\n");
				return;
			}

			items /= 2;
			if (items > ISOTPLEN) {
				PRINT_ERROR("PDU too long\n");
				return;
			}

			for (i = 0; i < items; i++) {

				tmp = asc2nibble(buf[(2*i) + 10]);
				if (tmp > 0x0F)
					return;
				isobuf[i] = (tmp << 4);
				tmp = asc2nibble(buf[(2*i) + 11]);
				if (tmp > 0x0F)
					return;
				isobuf[i] |= tmp;
			}

			ret = write(si, isobuf, items);
			if(ret != items) {
				PRINT_ERROR("Error in write()\n")
					state = STATE_SHUTDOWN;
				return;
			}
		} else {
			PRINT_ERROR("unknown command '%s'.\n", buf)
				strcpy(buf, "< error unknown command >");
			send(client_socket, buf, strlen(buf), 0);
		}
	}
}

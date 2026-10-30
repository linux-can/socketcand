/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */

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
#include <linux/can/raw.h>

static int raw_socket;
static struct ifreq ifr;
static struct sockaddr_can addr;
static struct msghdr msg;
static struct canfd_frame frame;
static struct iovec iov;
static char ctrlmsg[CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(__u32))];
static struct timeval tv;
static struct cmsghdr *cmsg;
static int canfd_on;

/* round up to the next valid CAN FD data length (0..8, 12, 16, 20, 24, 32, 48, 64) */
static __u8 canfd_valid_len(unsigned int len)
{
	static const __u8 valid_len[] = { 8, 12, 16, 20, 24, 32, 48, 64 };
	unsigned int i;

	for (i = 0; i < sizeof(valid_len); i++) {
		if (len <= valid_len[i])
			return len <= CAN_MAX_DLEN ? len : valid_len[i];
	}

	return CANFD_MAX_DLEN;
}

void state_raw(void)
{
	char buf[MAXLEN];
	int i, ret, items;
	fd_set readfds;

	if (previous_state != STATE_RAW) {
		if ((raw_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
			PRINT_ERROR("Error while creating RAW socket %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}

		strcpy(ifr.ifr_name, bus_name);
		if (ioctl(raw_socket, SIOCGIFINDEX, &ifr) < 0) {
			PRINT_ERROR("Error while searching for bus %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}

		addr.can_family = AF_CAN;
		addr.can_ifindex = ifr.ifr_ifindex;

		/*
		 * Enable CAN FD frames if the interface supports them (CAN FD
		 * capable interfaces have at least CANFD_MTU). On classic CAN
		 * interfaces only CAN_MTU sized frames are exchanged; the
		 * canfd_frame buffer is layout-compatible with struct can_frame.
		 */
		canfd_on = 0;
		if (ioctl(raw_socket, SIOCGIFMTU, &ifr) == 0 && ifr.ifr_mtu >= (int)CANFD_MTU) {
			const int canfd_frames = 1;
			if (setsockopt(raw_socket, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &canfd_frames, sizeof(canfd_frames)) < 0) {
				PRINT_ERROR("Could not enable CAN FD frames\n");
				state = STATE_SHUTDOWN;
				return;
			}
			canfd_on = 1;
		}

		const int timestamp_on = 1;
		if (setsockopt(raw_socket, SOL_SOCKET, SO_TIMESTAMP, &timestamp_on, sizeof(timestamp_on)) < 0) {
			PRINT_ERROR("Could not enable CAN timestamps\n");
			state = STATE_SHUTDOWN;
			return;
		}

		if (error_mask != 0) {
			can_err_mask_t err_mask = (error_mask & CAN_ERR_MASK);
			if (setsockopt(raw_socket, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, sizeof(err_mask)) < 0) {
				PRINT_ERROR("Could not enable CAN_RAW_ERR_FILTER\n");
				state = STATE_SHUTDOWN;
				return;
			}
		}

		if (bind(raw_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			PRINT_ERROR("Error while binding RAW socket %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}

		iov.iov_base = &frame;
		msg.msg_name = &addr;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = &ctrlmsg;

		previous_state = STATE_RAW;
	}

	FD_ZERO(&readfds);
	FD_SET(raw_socket, &readfds);
	FD_SET(client_socket, &readfds);

	/*
	 * Check if there are more elements in the element buffer before calling select() and
	 * blocking for new packets.
	 */
	if (more_elements) {
		FD_CLR(raw_socket, &readfds);
	} else {
		ret = select((raw_socket > client_socket) ? raw_socket + 1 : client_socket + 1, &readfds, NULL, NULL, NULL);

		if (ret < 0) {
			PRINT_ERROR("Error in select()\n");
			state = STATE_SHUTDOWN;
			return;
		}
	}

	if (FD_ISSET(raw_socket, &readfds)) {
		iov.iov_len = sizeof(frame);
		msg.msg_namelen = sizeof(addr);
		msg.msg_flags = 0;
		msg.msg_controllen = sizeof(ctrlmsg);

		ret = recvmsg(raw_socket, &msg, 0);
		if (ret != (int)CAN_MTU && ret != (int)CANFD_MTU) {
			PRINT_ERROR("Error reading frame from RAW socket\n");
		} else {
			/* read timestamp data */
			for (cmsg = CMSG_FIRSTHDR(&msg);
			     cmsg && (cmsg->cmsg_level == SOL_SOCKET);
			     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
				if (cmsg->cmsg_type == SO_TIMESTAMP) {
					tv = *(struct timeval *)CMSG_DATA(cmsg);
				}
			}

			if (frame.can_id & CAN_ERR_FLAG) {
				canid_t class = frame.can_id & CAN_EFF_MASK;
				ret = sprintf(buf, "< error %03X %lld.%06lld >", class,
					      (signed long long) tv.tv_sec, (signed long long) tv.tv_usec);
				send(client_socket, buf, strlen(buf), 0);
				tcp_quickack(client_socket);
			} else if (frame.can_id & CAN_RTR_FLAG) {
				/* TODO implement */
			} else if (ret == (int)CANFD_MTU) {
				/* CAN FD frame: < fdframe can_id flags seconds.useconds [data]* > */
				__u8 flags = frame.flags & (CANFD_BRS | CANFD_ESI);

				if (frame.can_id & CAN_EFF_FLAG) {
					ret = sprintf(buf, "< fdframe %08X %02X %lld.%06lld ", frame.can_id & CAN_EFF_MASK,
						      flags, (signed long long) tv.tv_sec, (signed long long) tv.tv_usec);
				} else {
					ret = sprintf(buf, "< fdframe %03X %02X %lld.%06lld ", frame.can_id & CAN_SFF_MASK,
						      flags, (signed long long) tv.tv_sec, (signed long long) tv.tv_usec);
				}
				for (i = 0; i < frame.len && i < CANFD_MAX_DLEN; i++) {
					ret += sprintf(buf + ret, "%02X", frame.data[i]);
				}
				sprintf(buf + ret, " >");
				send(client_socket, buf, strlen(buf), 0);
				tcp_quickack(client_socket);
			} else {
				if (frame.can_id & CAN_EFF_FLAG) {
					ret = sprintf(buf, "< frame %08X %lld.%06lld ", frame.can_id & CAN_EFF_MASK,
						      (signed long long) tv.tv_sec, (signed long long) tv.tv_usec);
				} else {
					ret = sprintf(buf, "< frame %03X %lld.%06lld ", frame.can_id & CAN_SFF_MASK,
						      (signed long long) tv.tv_sec, (signed long long) tv.tv_usec);
				}
				for (i = 0; i < frame.len; i++) {
					ret += sprintf(buf + ret, "%02X", frame.data[i]);
				}
				sprintf(buf + ret, " >");
				send(client_socket, buf, strlen(buf), 0);
				tcp_quickack(client_socket);
			}
		}
	}

	if (FD_ISSET(client_socket, &readfds)) {
		ret = receive_command(client_socket, (char *)&buf);

		if (ret == 0) {
			if (state_changed(buf, state)) {
				close(raw_socket);
				strcpy(buf, "< ok >");
				send(client_socket, buf, strlen(buf), 0);
				tcp_quickack(client_socket);
				return;
			}

			if (!strcmp("< echo >", buf)) {
				send(client_socket, buf, strlen(buf), 0);
				tcp_quickack(client_socket);
				return;
			}

			/* Send a single frame */
			if (!strncmp("< send ", buf, 7)) {
				memset(&frame, 0, sizeof(frame));
				items = sscanf(buf, "< %*s %x %hhu "
					       "%hhx %hhx %hhx %hhx %hhx %hhx "
					       "%hhx %hhx >",
					       &frame.can_id,
					       &frame.len,
					       &frame.data[0],
					       &frame.data[1],
					       &frame.data[2],
					       &frame.data[3],
					       &frame.data[4],
					       &frame.data[5],
					       &frame.data[6],
					       &frame.data[7]);

				if ((items < 2) ||
				    (frame.len > CAN_MAX_DLEN) ||
				    (items != 2 + frame.len)) {
					PRINT_ERROR("Syntax error in send command\n");
					return;
				}

				/* < send XXXXXXXX ... > check for extended identifier */
				if (element_length(buf, 2) == 8)
					frame.can_id |= CAN_EFF_FLAG;

				ret = send(raw_socket, &frame, CAN_MTU, 0);
				if (ret == -1) {
					state = STATE_SHUTDOWN;
					return;
				}

				/* Send a single CAN FD frame */
			} else if (!strncmp("< fdsend ", buf, 9)) {
				unsigned int can_id, flags, len;
				int pos = 0, n = 0;

				if (!canfd_on) {
					PRINT_ERROR("CAN FD is not supported by bus %s\n", bus_name);
					return;
				}

				memset(&frame, 0, sizeof(frame));
				items = sscanf(buf, "< %*s %x %x %u %n", &can_id, &flags, &len, &pos);
				if ((items != 3) ||
				    (len > CANFD_MAX_DLEN) ||
				    (flags & ~(CANFD_BRS | CANFD_ESI))) {
					PRINT_ERROR("Syntax error in fdsend command\n");
					return;
				}

				for (i = 0; i < (int)len; i++) {
					if (sscanf(buf + pos, "%hhx %n", &frame.data[i], &n) != 1) {
						PRINT_ERROR("Syntax error in fdsend command\n");
						return;
					}
					pos += n;
				}

				if (strcmp(buf + pos, ">")) {
					PRINT_ERROR("Syntax error in fdsend command\n");
					return;
				}

				/* < fdsend XXXXXXXX ... > check for extended identifier */
				if (element_length(buf, 2) == 8) {
					if (can_id > CAN_EFF_MASK) {
						PRINT_ERROR("Syntax error in fdsend command\n");
						return;
					}
					frame.can_id = can_id | CAN_EFF_FLAG;
				} else {
					if (can_id > CAN_SFF_MASK) {
						PRINT_ERROR("Syntax error in fdsend command\n");
						return;
					}
					frame.can_id = can_id;
				}

				/* pad with zeros up to the next valid CAN FD data length */
				frame.len = canfd_valid_len(len);
				frame.flags = flags | CANFD_FDF;

				ret = send(raw_socket, &frame, CANFD_MTU, 0);
				if (ret == -1) {
					state = STATE_SHUTDOWN;
					return;
				}

			} else {
				PRINT_ERROR("unknown command '%s'\n", buf);
				strcpy(buf, "< error unknown command >");
				send(client_socket, buf, strlen(buf), 0);
				tcp_quickack(client_socket);
			}
		} else {
			state = STATE_SHUTDOWN;
			return;
		}
	} else {
		ret = read(client_socket, &buf, 0);
		tcp_quickack(client_socket);
		if (ret == -1) {
			state = STATE_SHUTDOWN;
			return;
		}
	}
}

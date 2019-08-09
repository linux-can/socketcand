/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */
/*
 * Authors:
 * Andre Naujoks
 * Oliver Hartkopp
 * Jan-Niklas Meier
 * Felix Obenhuber
 *
 * Copyright (c) 2002-2012 Volkswagen Group Electronic Research
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Volkswagen nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Alternatively, provided that this notice is retained in full, this
 * software may be distributed under the terms of the GNU General
 * Public License ("GPL") version 2, in which case the provisions of the
 * GPL apply INSTEAD OF those given above.
 *
 * The provided data structures and external interfaces from this code
 * are not restricted to be used by modules with a GPL compatible license.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Send feedback to <linux-can@vger.kernel.org>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <linux/can.h>

#define MAXLEN 4000
#define PORT 29536

#define STATE_INIT 0
#define STATE_CONNECTED 1
#define STATE_SHUTDOWN 2

#define PRINT_INFO(...) printf(__VA_ARGS__);
#define PRINT_ERROR(...) fprintf(stderr, __VA_ARGS__);
#define PRINT_VERBOSE(...) printf(__VA_ARGS__);

void print_usage(void);
void sigint();
int receive_command(int socket, char *buf);
void state_connected();

int server_socket;
int raw_socket;
int port;
int verbose_flag=0;
int cmd_index=0;
int more_elements=0;
int state, previous_state;
char ldev[IFNAMSIZ];
char rdev[IFNAMSIZ];
char buf[MAXLEN];
char cmd_buffer[MAXLEN];


int main(int argc, char **argv)
{
	int i;
	struct sockaddr_in serveraddr;
	struct hostent *server_ent;
	struct sigaction sigint_action;
	char buf[MAXLEN];
	char* server_string;

	/* set default config settings */
	port = PORT;
	strcpy(ldev, "can0");
	strcpy(rdev, "can0");
	server_string = malloc(strlen("localhost"));


	/* Parse commandline arguments */
	for(;;) {
		/* getopt_long stores the option index here. */
		int c, option_index = 0;
		static struct option long_options[] = {
			{"verbose", no_argument, 0, 'v'},
			{"interfaces",  required_argument, 0, 'i'},
			{"server", required_argument, 0, 's'},
			{"port", required_argument, 0, 'p'},
			{"version", no_argument, 0, 'z'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "vhi:p:l:s:", long_options, &option_index);

		if(c == -1)
			break;

		switch(c) {
		case 0:
			/* If this option set a flag, do nothing else now. */
			if(long_options[option_index].flag != 0)
				break;
			break;

		case 'v':
			puts("Verbose output activated\n");
			verbose_flag = 1;
			break;

		case 'p':
			port = atoi(optarg);
			break;

		case 's':
			server_string = realloc(server_string, strlen(optarg)+1);
			strcpy(server_string, optarg);
			break;

		case 'i':
			strcpy(rdev, strtok(optarg, ","));
			strcpy(ldev, strtok(NULL, ","));
			break;

		case 'h':
			print_usage();
			return 0;

		case 'z':
			printf("socketcandcl version '%s'\n", "0.1");
			return 0;

		case '?':
			print_usage();
			return 0;

		default:
			print_usage();
			return -1;
		}
	}


	sigint_action.sa_handler = &sigint;
	sigemptyset(&sigint_action.sa_mask);
	sigint_action.sa_flags = 0;
	sigaction(SIGINT, &sigint_action, NULL);

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket < 0) {
		perror("socket");
		exit(1);
	}

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);

	server_ent = gethostbyname(server_string);
	if(server_ent == 0) {
		perror(server_string);
		exit(1);
	}

	memcpy(&(serveraddr.sin_addr.s_addr), server_ent->h_addr,
	      server_ent->h_length);


	if(connect(server_socket, (struct sockaddr*)&serveraddr,
		   sizeof(serveraddr)) != 0) {
		perror("connect");
		exit(1);
	}


	for(;;) {
		switch(state) {
		case STATE_INIT:
			/*  has to start with a command */
			i = receive_command(server_socket, (char *) &buf);
			if(i != 0) {
				PRINT_ERROR("Connection terminated while waiting for command.\n");
				state = STATE_SHUTDOWN;
				previous_state = STATE_INIT;
				break;
			}

			if(!strncmp("< hi", buf, 4)) {
				/* send open and rawmode command */
				sprintf(buf, "< open %s >", rdev);
				send(server_socket, buf, strlen(buf), 0);

				/* send rawmode command */
				strcpy(buf, "< rawmode >");
				send(server_socket, buf, strlen(buf), 0);
				state = STATE_CONNECTED;
			}
			break;

		case STATE_CONNECTED:
			state_connected();
			break;
		case STATE_SHUTDOWN:
			PRINT_VERBOSE("Closing client connection.\n");
			close(server_socket);
			return 0;
		}
	}
	return 0;
}


inline void state_connected()
{

	int ret;
	static struct can_frame frame;
	static struct ifreq ifr;
	static struct sockaddr_can addr;
	fd_set readfds;

	if(previous_state != STATE_CONNECTED) {

		if((raw_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
			PRINT_ERROR("Error while creating RAW socket %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}

		strcpy(ifr.ifr_name, ldev);
		if(ioctl(raw_socket, SIOCGIFINDEX, &ifr) < 0) {
			PRINT_ERROR("Error while searching for bus %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}

		addr.can_family = AF_CAN;
		addr.can_ifindex = ifr.ifr_ifindex;

		/* turn on timestamp */
		const int timestamp_on = 0;
		if(setsockopt(raw_socket, SOL_SOCKET, SO_TIMESTAMP,
			      &timestamp_on, sizeof(timestamp_on)) < 0) {
			PRINT_ERROR("Could not enable CAN timestamps\n");
			state = STATE_SHUTDOWN;
			return;
		}
		/* bind socket */
		if(bind(raw_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			PRINT_ERROR("Error while binding RAW socket %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}
		previous_state = STATE_CONNECTED;
	}


	if(fork()) {

		for(;;) {

			FD_ZERO(&readfds);
			FD_SET(server_socket, &readfds);

			/*
			 * Check if there are more elements in the element buffer before
			 * calling select() and blocking for new packets.
			 */
			if(!more_elements) {
				ret = select(server_socket+1, &readfds, NULL, NULL, NULL);

				if(ret < 0) {
					PRINT_ERROR("Error in select()\n")
						state = STATE_SHUTDOWN;
					return;
				}
			}

			if(FD_ISSET(server_socket, &readfds) || more_elements) {
				ret = receive_command(server_socket, (char *) &buf);
				if(ret == 0) {
					if(!strncmp("< frame", buf, 7)) {
						char data_str[2*8];

						sscanf(buf, "< frame %x %*d.%*d %s >", &frame.can_id,
						       data_str);

						char* s = buf + 7;
						for(; ++s;) {
							if(*s== ' ') {
								break;
							}
						}
						if((s - buf - 7) > 4)
							frame.can_id |= CAN_EFF_FLAG;

						frame.can_dlc = strlen(data_str) / 2;

						sscanf(data_str, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
						       &frame.data[0], &frame.data[1],
						       &frame.data[2], &frame.data[3],
						       &frame.data[4], &frame.data[5],
						       &frame.data[6], &frame.data[7]);

						ret = write(raw_socket, &frame, sizeof(struct can_frame));
						if(ret<sizeof(struct can_frame)) {
							perror("Writing CAN frame to can socket\n");
						}
					}
				}
			} else {
				ret = read(server_socket, &buf, 0);
				if(ret == -1) {
					state = STATE_SHUTDOWN;
					return;
				}
			}
		}
	} else {

		for(;;) {

			FD_ZERO(&readfds);
			FD_SET(raw_socket, &readfds);

			ret = select(raw_socket+1, &readfds, NULL, NULL, NULL);
			if(ret < 0) {
				PRINT_ERROR("Error in select()\n")
					state = STATE_SHUTDOWN;
				return;
			}

			if(FD_ISSET(raw_socket, &readfds)) {
				ret = recv(raw_socket, &frame, sizeof(struct can_frame), MSG_WAITALL);
				if(ret < sizeof(struct can_frame)) {
					PRINT_ERROR("Error reading frame from RAW socket\n")
						perror("Reading CAN socket\n");
				} else {
					if(frame.can_id & CAN_ERR_FLAG) {
						/* TODO implement */
					} else if(frame.can_id & CAN_RTR_FLAG) {
						/* TODO implement */
					} else {
						int i;
						if(frame.can_id & CAN_EFF_FLAG) {
							ret = sprintf(buf, "< send %08X %d ",
								      frame.can_id & CAN_EFF_MASK, frame.can_dlc);
						} else {
							ret = sprintf(buf, "< send %03X %d ",
								      frame.can_id & CAN_SFF_MASK, frame.can_dlc);
						}
						for(i=0; i<frame.can_dlc; i++) {
							ret += sprintf(buf+ret, "%02x ", frame.data[i]);
						}
						sprintf(buf+ret, " >");

						const size_t len = strlen(buf);
						ret = send(server_socket, buf, len, 0);
						if(ret < sizeof(len)) {
							perror("Error sending TCP frame\n");
						}
					}
				}
			}
		}
	}

}

/* reads all available data from the socket into the command buffer.
 * returns '-1' if no command could be received.
 */
int receive_command(int socket, char *buffer)
{
	int i, start, stop;

	/* if there are no more elements in the buffer read more data from the
	 * socket.
	 */
	if(!more_elements) {
		cmd_index += read(socket, cmd_buffer+cmd_index, MAXLEN-cmd_index);
	}

	more_elements = 0;

	/* find first '<' in string */
	start = -1;
	for(i=0; i<cmd_index; i++) {
		if(cmd_buffer[i] == '<') {
			start = i;
			break;
		}
	}

	/*
	 * if there is no '<' in string it makes no sense to keep data because
	 * we will never be able to construct a command of it
	 */
	if(start == -1) {
		cmd_index = 0;
		return -1;
	}

	/* check whether the command is completely in the buffer */
	stop = -1;
	for(i=1; i<cmd_index; i++) {
		if(cmd_buffer[i] == '>') {
			stop = i;
			break;
		}
	}

	/* if no '>' is in the string we have to wait for more data */
	if(stop == -1) {
		return -1;
	}

	/* copy string to new destination and correct cmd_buffer */
	for(i=start; i<=stop; i++) {
		buffer[i-start] = cmd_buffer[i];
	}
	buffer[i-start] = '\0';


	/* if only this message was in the buffer we're done */
	if(stop == cmd_index-1) {
		cmd_index = 0;
	} else {
		/* check if there is a '<' after the stop */
		start = -1;
		for(i=stop; i<cmd_index; i++) {
			if(cmd_buffer[i] == '<') {
				start = i;
				break;
			}
		}

		/* if there is none it is only garbage we can remove */
		if(start == -1) {
			cmd_index = 0;
			return 0;
			/* otherwise we copy the valid data to the beginning of the buffer */
		} else {
			for(i=start; i<cmd_index; i++) {
				cmd_buffer[i-start] = cmd_buffer[i];
			}
			cmd_index -= start;

			/* check if there is at least one full element in the buffer */
			stop = -1;
			for(i=1; i<cmd_index; i++) {
				if(cmd_buffer[i] == '>') {
					stop = i;
					break;
				}
			}

			if(stop != -1) {
				more_elements = 1;
			}
		}
	}
	return 0;
}


void print_usage(void)
{
	printf("Usage: socketcandcl [-v | --verbose] [-i interfaces | --interfaces interfaces]\n\t\t[-s server | --server server ]\n\t\t[-p port | --port port]\n");
	printf("Options:\n");
	printf("\t-v activates verbose output to STDOUT\n");
	printf("\t-s server hostname\n");
	printf("\t-i SocketCAN interfaces to use: device_server,device_client \n");
	printf("\t-p port changes the default port (%d) the client connects to\n", PORT);
	printf("\t-h prints this message\n");
}


void childdied()
{
	wait(NULL);
}


void sigint()
{
	if(verbose_flag)
		PRINT_ERROR("received SIGINT\n")

			if(server_socket != -1) {
				if(verbose_flag)
					PRINT_INFO("closing server socket\n")
						close(server_socket);
			}

	if(raw_socket != -1) {
		if(verbose_flag)
			PRINT_INFO("closing can socket\n")
				close(raw_socket);
	}

	exit(0);
}

/* eof */

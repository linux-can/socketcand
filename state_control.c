/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */

#include "config.h"
#include "socketcand.h"
#include "statistics.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void state_control() {
	char buf[MAXLEN];
	int i, items;

	if(previous_state != STATE_CONTROL) {
		PRINT_VERBOSE("starting statistics thread...\n")
			pthread_create(&statistics_thread, NULL, &statistics_loop, NULL);

		previous_state = STATE_CONTROL;
	}

	i = receive_command(client_socket, (char *) &buf);

	if(i != 0) {
		PRINT_ERROR("Connection terminated while waiting for command.\n");
		state = STATE_SHUTDOWN;
		return;
	}

	if (state_changed(buf, state)) {
		pthread_cancel(statistics_thread);
		strcpy(buf, "< ok >");
		send(client_socket, buf, strlen(buf), 0);
		return;
	}

	if(!strcmp("< echo >", buf)) {
		send(client_socket, buf, strlen(buf), 0);
		return;
	}

	if(!strncmp("< statistics ", buf, 13)) {
		items = sscanf(buf, "< %*s %u >",
			       &i);

		if (items != 1) {
			PRINT_ERROR("Syntax error in statistics command\n")
				} else {
			statistics_ival = i;
		}
	} else {
		PRINT_ERROR("unknown command '%s'.\n", buf)
			strcpy(buf, "< error unknown command >");
		send(client_socket, buf, strlen(buf), 0);
	}
}

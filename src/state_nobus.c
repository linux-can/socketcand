/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */

#include "canctl.h"
#include "config.h"
#include "socketcand.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static int check_bus(const char *bus_name)
{
	int found = 0, i;

	for (i = 0; i < interface_count; i++) {
		if (!strcmp(interface_names[i], bus_name))
			found = 1;
	}
	return found;
}

void state_nobus(void)
{
	int i;
	char buf[MAXLEN], op;

	if (previous_state != STATE_NO_BUS) {
		strcpy(buf, "< hi >");
		send(client_socket, buf, strlen(buf), 0);
		tcp_quickack(client_socket);
		previous_state = STATE_NO_BUS;
	}

	/* client has to start with a command */
	i = receive_command(client_socket, buf);
	if (i != 0) {
		PRINT_ERROR("Connection terminated while waiting for command.\n");
		state = STATE_SHUTDOWN;
		return;
	}

	if (!strncmp("< open ", buf, 7)) {
		sscanf(buf, "< open %s>", bus_name);

		/* check if access to this bus is allowed */
		if (!check_bus(bus_name)) {
			PRINT_INFO("client tried to access unauthorized bus.\n");
			strcpy(buf, "< error could not open bus >");
			goto shutdown_err;
		}
		if (canctl_start_iface(bus_name)) {
			PRINT_INFO("Error starting CAN interface.\n");
			snprintf(buf, MAXLEN, "< error could not start interface %s >", bus_name);
			goto shutdown_err;
		}
		state = STATE_BCM;
		goto return_ok;
	} else if (sscanf(buf, "< %s %c ", bus_name, &op) == 2) {
		/* check if access to this bus is allowed */
		if (!check_bus(bus_name)) {
			PRINT_INFO("client tried to access unauthorized bus.\n");
			strcpy(buf, "< error could not open bus >");
			goto shutdown_err;
		}

		/* Check the configuration operation */
		switch (op) {
		case 'B':
			if (canctl_set_bittiming(bus_name, buf, strlen(buf))) {
				PRINT_ERROR("Error configuring bus bittiming.\n");
				snprintf(buf, MAXLEN, "< error configuring interface %s > \n", bus_name);
				goto shutdown_err;
			}
			break;
		case 'C':
			if (canctl_set_control_modes(bus_name, buf, strlen(buf))) {
				PRINT_ERROR("Error configuring bus control modes.\n");
				snprintf(buf, MAXLEN, "< error configuring interface %s > \n", bus_name);
				goto shutdown_err;
			}
			break;
		default:
			PRINT_ERROR("Configuration operation not supported\n");
			snprintf(buf, MAXLEN, "< configuration operation not supported %c > \n", op);
			goto shutdown_err;
			break;
		}

		goto return_ok;
	} else {
		PRINT_ERROR("unknown command '%s'.\n", buf);
		strcpy(buf, "< error unknown command >");
		send(client_socket, buf, strlen(buf), 0);
		tcp_quickack(client_socket);
	}

return_ok:
	strcpy(buf, "< ok >");
	send(client_socket, buf, strlen(buf), 0);
	tcp_quickack(client_socket);
	return;
shutdown_err:
	send(client_socket, buf, strlen(buf), 0);
	tcp_quickack(client_socket);
	state = STATE_SHUTDOWN;
}

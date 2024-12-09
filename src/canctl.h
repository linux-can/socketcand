/**
 * canctrl.h
 *
 * Functions to configure the CAN interfaces (bitrates, control modes, etc...)
 */

#ifndef __CANCTRL_H__
#define __CANCTRL_H__

#ifdef HAVE_LIBSOCKETCAN

#include <libsocketcan.h>

/**
 * canctl_set_bittiming - Set the interface bittiming.
 *
 * @param bus_name name of the can interface to configure.
 * @param buff_bittiming_conf buffer which contains the remaining config of the interface.
 * @param buff_length config buffer length.
 *
 * returns 0 - ok / -1 error
 */
int canctl_set_bittiming(const char *bus_name, const char *buff_bittiming_conf, int buff_length);

/**
 * canctl_start_iface - Start the can interface.
 *
 * @param bus_name name of the can interface to start.
 *
 * returns 0 - ok / -1 error
 */
static inline int canctl_start_iface(const char *bus_name)
{
	return can_do_start(bus_name);
}

/**
 * canctl_set_control_modes - set up control modes for given interface.
 *
 * @param bus_name name of the can interface to configure.
 * @param buff_control_modes_conf buffer which contains the control modes configuration.
 * @param buff_length config buffer length.
 *
 * returns 0 - ok / -1 error
 */
int canctl_set_control_modes(const char *bus_name, const char *buff_control_modes_conf, int buff_length);

#else

#include <errno.h>

static inline int canctl_set_bittiming(const char *bus_name, const char *buff_bittiming_conf, int buff_length)
{
	return -EINVAL;
}

static inline int canctl_start_iface(const char *bus_name)
{
	return 0;
}

static inline int canctl_set_control_modes(const char *bus_name, const char *buff_control_modes_conf, int buff_length)
{
	return -EINVAL;
}

#endif
#endif

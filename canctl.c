#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libsocketcan.h>

int canctl_set_bittiming(const char *bus_name, const char *buff_bittiming_conf, int buff_length)
{
	int bitrate, sample_point, tq, prop_seg, phase_seg1, phase_seg2, sjw, brp, items;

	items = sscanf(buff_bittiming_conf, "< %*s B %d %d %d %d %d %d %d %d >", 
		&bitrate, 
		&sample_point,
		&tq, 
		&prop_seg,
		&phase_seg1,
		&phase_seg2, 
		&sjw,
		&brp
	);

	if (items != 8)	{
		return -1;
	}

	struct can_bittiming bt;
	memset(&bt, 0, sizeof(bt));
	if (bitrate >= 0) {
		bt.bitrate = bitrate;
	}

	if (sample_point >= 0) {
		bt.sample_point = sample_point;
	}

	if (tq >= 0) {
		bt.tq = tq;
	}

	if (prop_seg >= 0) {
		bt.prop_seg = prop_seg;
	}

	if (phase_seg1 >= 0) {
		bt.phase_seg1 = phase_seg1;
	}

	if (phase_seg2 >= 0) {
		bt.phase_seg2 = phase_seg2;
	}
	
	if (sjw >= 0) {
		bt.sjw = sjw;
	}

	if (brp >= 0) {
		bt.brp = brp;
	}

	return !(can_do_stop(bus_name) || can_set_bittiming(bus_name, &bt)) ? 0 : -1;
}

int canctl_set_control_modes(const char *bus_name, const char *buff_control_modes_conf, int buff_length)
{
	int listen_only, loopback, three_samples, items;

	items = sscanf(buff_control_modes_conf, "< %*s C %d %d %d >", 
		&listen_only, &loopback, &three_samples);

	if (items != 3) {
		return -1;
	}

	struct can_ctrlmode cm = {
		.mask = CAN_CTRLMODE_LISTENONLY | CAN_CTRLMODE_LOOPBACK | CAN_CTRLMODE_3_SAMPLES,
	};

	if (listen_only)
		cm.flags |= CAN_CTRLMODE_LISTENONLY;
	if (loopback)
		cm.flags |= CAN_CTRLMODE_LOOPBACK;
	if (three_samples)
		cm.flags |= CAN_CTRLMODE_3_SAMPLES;
	
	return !(can_do_stop(bus_name) || can_set_ctrlmode(bus_name, &cm)) ? 0 : -1;
}
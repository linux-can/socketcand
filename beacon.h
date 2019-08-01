/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */

#define BROADCAST_PORT 42000
#define BEACON_LENGTH 2048
#define BEACON_TYPE "SocketCAN"
#define BEACON_DESCRIPTION "socketcand"

void *beacon_loop(void *ptr);

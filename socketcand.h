/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */

#include <pthread.h>
#include <syslog.h>

/* max. length for ISO 15765-2 PDUs */
#define ISOTPLEN 4095

/* receive buffer length from inet socket for an isotp PDU plus command */
#define MAXLEN (2 * ISOTPLEN + 100) /* 4095 * 2 + cmd stuff */

#define MAX_BUSNAME 16+1
#define PORT 29536
#define DEFAULT_INTERFACE "eth0"
#define DEFAULT_BUSNAME "vcan0"

#define STATE_NO_BUS 0
#define STATE_BCM 1
#define STATE_RAW 2
#define STATE_SHUTDOWN 3
#define STATE_CONTROL 4
#define STATE_ISOTP 5

#define PRINT_INFO(...) if(daemon_flag) syslog(LOG_INFO, __VA_ARGS__); else printf(__VA_ARGS__);
#define PRINT_ERROR(...) if(daemon_flag) syslog(LOG_ERR, __VA_ARGS__); else fprintf(stderr, __VA_ARGS__);
#define PRINT_VERBOSE(...) if(verbose_flag && !daemon_flag) printf(__VA_ARGS__);

#ifndef VERSION_STRING
#define VERSION_STRING "SNAPSHOT"
#endif

#undef DEBUG_RECEPTION

void state_bcm();
void state_raw();
void state_isotp();
void state_control();

extern int client_socket;
extern char **interface_names;
extern int interface_count;
extern int port;
extern int verbose_flag;
extern int daemon_flag;
extern int state;
extern int previous_state;
extern char bus_name[];
extern char* description;
extern char* afuxname;
extern pthread_t statistics_thread;
extern int more_elements;
extern struct sockaddr_in broadcast_addr;
extern struct sockaddr_in saddr;

int receive_command(int socket, char *buf);
int state_changed(char *buf, int current_state);
char *element_start(char *buf, int element);
int element_length(char *buf, int element);
int asc2nibble(char c);

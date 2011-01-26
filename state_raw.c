#include "socketcand.h"
#include "statistics.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <getopt.h>

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
#include <linux/can/netlink.h>


inline void state_raw() {
    if(previous_state != STATE_RAW) {
        PRINT_VERBOSE("starting statistics thread...\n")
        pthread_mutex_init( &stat_mutex, NULL );
        pthread_cond_init( &stat_condition, NULL );
        pthread_mutex_lock( &stat_mutex );
        pthread_create( &statistics_thread, NULL, &statistics_loop, NULL );
        pthread_cond_wait( &stat_condition, &stat_mutex );
        pthread_mutex_unlock( &stat_mutex );
        previous_state = STATE_RAW;
    }
}

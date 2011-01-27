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

int raw_socket;
struct ifreq ifr;
struct sockaddr_can addr;
fd_set readfds;
struct msghdr msg;
struct can_frame frame;
struct iovec iov;

inline void state_raw() {
    char buf[MAXLEN];
    int items;
    int i, ret;

    if(previous_state != STATE_RAW) {
        PRINT_VERBOSE("starting statistics thread...\n")
        pthread_mutex_init( &stat_mutex, NULL );
        pthread_cond_init( &stat_condition, NULL );
        pthread_mutex_lock( &stat_mutex );
        pthread_create( &statistics_thread, NULL, &statistics_loop, NULL );
        pthread_cond_wait( &stat_condition, &stat_mutex );
        pthread_mutex_unlock( &stat_mutex );

        if((raw_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
            PRINT_ERROR("Error while creating RAW socket %s\n", strerror(errno));
            state = STATE_SHUTDOWN;
            return;
        }

        strcpy(ifr.ifr_name, bus_name);
        if(ioctl(raw_socket, SIOCGIFINDEX, &ifr) < 0) {
            PRINT_ERROR("Error while searching for bus %s\n", strerror(errno));
            state = STATE_SHUTDOWN;
            return;
        }

        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;

        if(bind(raw_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            PRINT_ERROR("Error while binding RAW socket %s\n", strerror(errno));
            state = STATE_SHUTDOWN;
            return;
        }

        iov.iov_base = &frame;
        msg.msg_name = &addr;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        iov.iov_len = sizeof(frame);
        msg.msg_namelen = sizeof(addr);
        msg.msg_flags = 0;

        previous_state = STATE_RAW;
    }
            
    FD_ZERO(&readfds);
    FD_SET(raw_socket, &readfds);
    FD_SET(client_socket, &readfds);

    ret = select((raw_socket > client_socket)?raw_socket+1:client_socket+1, &readfds, NULL, NULL, NULL);
    if(ret < 0) {
        PRINT_ERROR("Error in select()\n")
        state = STATE_SHUTDOWN;
        return;
    }

    if(FD_ISSET(raw_socket, &readfds)) {
        printf("hallo\n");
        ret = recvmsg(raw_socket, &msg, 0);
        if(ret < sizeof(struct can_frame)) {
            PRINT_ERROR("Error reading frame from RAW socket\n")
        } else {
            if(frame.can_id & CAN_ERR_FLAG) {
                /* TODO implement */
            } else if(frame.can_id & CAN_RTR_FLAG) {
                /* TODO implement */
            } else {
                ret = sprintf(buf, "< frame %X %u", frame.can_id, frame.can_dlc);
                for(i=0;i<frame.can_dlc;i++) {
                    ret += sprintf(buf+ret, " %X", frame.data[i]);
                }
                sprintf(buf+ret, " >");
                send(client_socket, buf, strlen(buf), 0);
            }
        }
    }

    if(FD_ISSET(client_socket, &readfds)) {
        ret = receive_command(client_socket, (char *) &buf);

        if(ret == 0) {
            if(!strncmp("< statistics ", buf, 13)) {
                items = sscanf(buf, "< %*s %u >",
                    &i);

                if (items != 1) {
                    PRINT_ERROR("Syntax error in statistics command\n")
                } else {
                    set_statistics(bus_name, i);
                }
            } else if(!strcmp("< bcmmode >", buf)) {
                pthread_cancel(statistics_thread);
                close(raw_socket);
                state = STATE_BCM; 
                strcpy(buf, "< ok >");
                send(client_socket, buf, strlen(buf), 0);
            } else if(!strcmp("< echo >", buf)) {
                send(client_socket, buf, strlen(buf), 0);
            }else {
                PRINT_ERROR("unknown command '%s'\n", buf);
                strcpy(buf, "< error unknown command >");
                send(client_socket, buf, strlen(buf), 0);
            }
        }
    }
}


/*
 *
 * Authors:
 * Andre Naujoks (the socket server stuff)
 * Oliver Hartkopp (the rest)
 * Jan-Niklas Meier (extensions for use with kayak)
 *
 * Copyright (c) 2002-2009 Volkswagen Group Electronic Research
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
 * Send feedback to <socketcan-users@lists.berlios.de>
 *
 */

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
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <syslog.h>

#include <linux/can.h>
#include <linux/can/bcm.h>
#include <linux/can/error.h>
#include <linux/can/netlink.h>

#include <libsocketcan.h>

#include "socketcand.h"
#include "statistics.h"
#include "beacon.h"
#include "state_bcm.h"
#include "state_raw.h"

void print_usage(void);
void sigint();
void childdied();
int receive_command(int socket, char *buf);

int sl, client_socket;
pthread_t beacon_thread, statistics_thread;
char **interface_names;
int interface_count=0;
int port=PORT;
struct in_addr laddr;
int verbose_flag=0;
int daemon_flag=0;
int uid;
int state = STATE_NO_BUS;
int previous_state = -1;
char bus_name[MAX_BUSNAME];
char cmd_buffer[MAXLEN];
int cmd_index=0;

int main(int argc, char **argv)
{
    int i, found;
    struct sockaddr_in  saddr, clientaddr;
    socklen_t sin_size = sizeof(clientaddr);
    struct sigaction signalaction, sigint_action;
    sigset_t sigset;
    char buf[MAXLEN];
    int c;

    uid = getuid();
    if(uid != 0) {
        printf("You are not running socketcand as root. This is highly recommended because otherwise you won't be able to change bitrate settings, etc.\n");
    }

    /* default is to listen on 127.0.0.1 only */
    laddr.s_addr = inet_addr( "127.0.0.1" );

    /* Parse commandline arguments */
    while (1) {
        /* getopt_long stores the option index here. */
        int option_index = 0;
        static struct option long_options[] = {
            {"verbose", no_argument, 0, 'v'},
            {"interfaces",  required_argument, 0, 'i'},
            {"port", required_argument, 0, 'p'},
            {"listen", required_argument, 0, 'l'},
            {"daemon", no_argument, 0, 'd'},
            {0, 0, 0, 0}
        };
    
        c = getopt_long (argc, argv, "vhi:p:l:d", long_options, &option_index);
    
        if (c == -1)
            break;
    
        switch (c) {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
            break;
    
    
            case 'v':
                puts ("Verbose output activated\n");
                verbose_flag = 1;
                break;
    
            case 'p':
                port = atoi(optarg);
                printf("Using Port %d\n", port);
                break;
    
            case 'i':
                for(i=0;;i++) {
                    if(optarg[i] == '\0')
                        break;
                    if(optarg[i] == ',')
                        interface_count++;
                }
                interface_count++;
    
                interface_names = malloc(sizeof(char *) * interface_count);
    
                interface_names[0] = strtok(optarg, ",");
    
                for(i=1;i<interface_count;i++) {
                    interface_names[i] = strtok(NULL, ",");
                }
                break;

            case 'l':
                laddr.s_addr = inet_addr( optarg );
                break;

            case 'h':
                print_usage();
                return 0;

            case 'd':
                daemon_flag=1;
                break;
                
            case '?':
                print_usage();
                return -1;
    
            default:
                print_usage();
                return -1;
        }
    }

    /* if daemon mode was activated the syslog must be opened */
    if(daemon_flag) {
        openlog("socketcand", 0, LOG_DAEMON);
    }


    sigemptyset(&sigset);
    signalaction.sa_handler = &childdied;
    signalaction.sa_mask = sigset;
    signalaction.sa_flags = 0;
    sigaction(SIGCHLD, &signalaction, NULL);  /* signal for dying child */
    
    sigint_action.sa_handler = &sigint;
    sigint_action.sa_mask = sigset;
    sigint_action.sa_flags = 0;
    sigaction(SIGINT, &sigint_action, NULL);

    if((sl = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("inetsocket");
        exit(1);
    }

#ifdef DEBUG
    if(verbose_flag)
        printf("setting SO_REUSEADDR\n");
    i = 1;
    if(setsockopt(sl, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) <0) {
        perror("setting SO_REUSEADDR failed");
    }
#endif

    saddr.sin_family = AF_INET;
    saddr.sin_addr = laddr;
    saddr.sin_port = htons(port);

    PRINT_VERBOSE("creating broadcast thread...\n")
    i = pthread_create(&beacon_thread, NULL, &beacon_loop, NULL);
    if(i)
        PRINT_ERROR("could not create broadcast thread.\n");

    PRINT_VERBOSE("binding socket to %s:%d\n", inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port))
    if(bind(sl,(struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("bind");
        exit(-1);
    }

    if (listen(sl,3) != 0) {
        perror("listen");
        exit(1);
    }

    while (1) { 
        client_socket = accept(sl,(struct sockaddr *)&clientaddr, &sin_size);
        if (client_socket > 0 ){
            int flag;
            flag = 1;
            setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
            if (fork())
                close(client_socket);
            else
                break;
        }
        else {
            if (errno != EINTR) {
                /*
                 * If the cause for the error was NOT the
                 * signal from a dying child => give an error
                 */
                perror("accept");
                exit(1);
            }
        }
    }

    PRINT_VERBOSE("client connected\n")
    
#ifdef DEBUG
    PRINT_VERBOSE("setting SO_REUSEADDR\n")
    i = 1;
    if(setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) <0) {
        perror("setting SO_REUSEADDR failed");
    }
#endif

    /* main loop with state machine */
    while(1) {
        switch(state) {
            case STATE_NO_BUS:
                if(previous_state != STATE_NO_BUS) {
                    strcpy(buf, "< hi >");
                    send(client_socket, buf, strlen(buf), 0);
                    previous_state = STATE_NO_BUS;
                }
                /* client has to start with a command */
                i = receive_command(client_socket, (char *) &buf);
                if(i != 0) {
                    PRINT_ERROR("Connection terminated while waiting for command.\n");
                    state = STATE_SHUTDOWN;
                    break;
                }

                if(!strncmp("< open ", buf, 7)) {
                    sscanf(buf, "< open %s>", bus_name);

                    /* check if access to this bus is allowed */
                    found = 0;
                    for(i=0;i<interface_count;i++) {
                        if(!strcmp(interface_names[i], bus_name))
                        found = 1;
                    }

                    if(found) {
                        strcpy(buf, "< ok >");
                        send(client_socket, buf, strlen(buf), 0);
                        state = STATE_BCM;
                        break;
                    } else {
                        PRINT_INFO("client tried to access unauthorized bus.\n");
                        strcpy(buf, "< error could not open bus >");
                        send(client_socket, buf, strlen(buf), 0);
                        state = STATE_SHUTDOWN;
                    }
                } else if(!strncmp("< bittiming", buf, 11)) {
                    struct can_bittiming timing;
                    char bus_name[IFNAMSIZ];
                    int items;

                    memset(&timing, 0, sizeof(timing));

                    items = sscanf(buf, "< %*s %s %x %x %x %x %x %x %x %x >",
                        bus_name,
                        &timing.bitrate,
                        &timing.sample_point,
                        &timing.tq,
                        &timing.prop_seg,
                        &timing.phase_seg1,
                        &timing.phase_seg2,
                        &timing.sjw,
                        &timing.brp);

                    if (items != 9) {
                        PRINT_ERROR("Syntax error in set bitrate command\n")
                    } else {
                        can_set_bittiming(bus_name, &timing);
                        strcpy(buf, "< ok >");
                        send(client_socket, buf, strlen(buf), 0);
                    }
                } else if(!strncmp(buf, "< controlmode ", 14)) {
                    int i,j,k;
                    struct can_ctrlmode ctrlmode;
                    int items;

                    memset(&ctrlmode, 0, sizeof(ctrlmode));
                    ctrlmode.mask = CAN_CTRLMODE_LOOPBACK | CAN_CTRLMODE_LISTENONLY | CAN_CTRLMODE_3_SAMPLES;

                    items = sscanf(buf, "< %*s %s %u %u %u >",
                        bus_name,
                        &i,
                        &j,
                        &k);

                    if (items != 4) {
                        PRINT_ERROR("Syntax error in set controlmode command\n")
                    } else {
                        if(i)
                            ctrlmode.flags |= CAN_CTRLMODE_LISTENONLY;
                        if(j)
                            ctrlmode.flags |= CAN_CTRLMODE_LOOPBACK;
                        if(k)
                            ctrlmode.flags |= CAN_CTRLMODE_3_SAMPLES;

                        can_set_ctrlmode(bus_name, &ctrlmode);
                        strcpy(buf, "< ok >");
                        send(client_socket, buf, strlen(buf), 0);
                    }
                } else {
                    PRINT_ERROR("unknown command '%s'.\n", buf)
                    strcpy(buf, "< error unknown command >");
                    send(client_socket, buf, strlen(buf), 0);
                }
                break;

            case STATE_BCM:
                state_bcm();
                break;
            case STATE_RAW:
                state_raw();
                break;

            case STATE_SHUTDOWN:
                PRINT_VERBOSE("Closing client connection.\n");
                close(client_socket);
                return 0;
        }
    }
    return 0;
}

/* reads all available data from the socket into the command buffer. 
 * returns '-1' if no command could be received.
 */
int receive_command(int socket, char *buffer) {
    int i, start, stop;

    /* read what we can get */
    cmd_index += read(socket, cmd_buffer+cmd_index, MAXLEN-cmd_index);

    /* find first '<' in string */
    start = -1;
    for(i=0;i<cmd_index;i++) {
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
    for(i=1;i<cmd_index;i++) {
        if(cmd_buffer[i] == '>') {
            stop = i;
            break;
        }
    }

    /* if no '>' is in the string we have to wait for more data */
    if(stop == -1)
        return -1;

    /* copy string to new destination and correct cmd_buffer */
    for(i=start;i<=stop;i++) {
        buffer[i] = cmd_buffer[i+start];
    }
    buffer[i] = '\0';

    /* if only this message was in the buffer we're done */
    if(stop == cmd_index-1) {
        cmd_index = 0;
    } else {
        /* check if there is a '<' after the stop */
        start = -1;
        for(i=stop;i<cmd_index;i++) {
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
            for(i=start;i<cmd_index;i++) {
                cmd_buffer[i-start] = cmd_buffer[i];
            }
            cmd_index -= start;
        }
    }
    return 0;
}

void print_usage(void) {
    printf("Socket CAN daemon\n");
    printf("Usage: socketcand [-v | --verbose] [-i interfaces | --interfaces interfaces]\n\t\t[-p port | --port port] [-l ip_addr | --listen ip_addr]\n\n");
    printf("Options:\n");
    printf("\t-v activates verbose output to STDOUT\n");
    printf("\t-i interfaces is used to specify the Socket CAN interfaces the daemon\n\t\tshall provide access to\n");
    printf("\t-p port changes the default port (28600) the daemon is listening at\n");
    printf("\t-l ip_addr changes the default ip address (127.0.0.1) the daemon will\n\t\tbind to\n");
    printf("\t-h prints this message\n");
}

void childdied() {
    wait(NULL);
}

void sigint() {
    if(verbose_flag)
        PRINT_ERROR("received SIGINT\n")

    if(sl != -1) {
        if(verbose_flag)
            PRINT_INFO("closing listening socket\n")
        if(!close(sl))
            sl = -1;
    }

    if(client_socket != -1) {
        if(verbose_flag)
            PRINT_INFO("closing client socket\n")
        if(!close(client_socket))
            client_socket = -1;
    }

    pthread_cancel(beacon_thread);
    pthread_cancel(statistics_thread);

    closelog();

    exit(0);
}

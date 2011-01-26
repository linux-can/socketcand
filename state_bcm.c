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

#include <libsocketcan.h>

inline void state_bcm() {
    int i, ret;
    int sc = -1;
    struct sockaddr_can caddr;
    socklen_t caddrlen = sizeof(caddr);
    struct ifreq ifr;
    fd_set readfds;
    char rxmsg[50];
    char buf[MAXLEN];

    struct {
        struct bcm_msg_head msg_head;
        struct can_frame frame;
    } msg;

    if(previous_state != STATE_BCM) {
        /* open BCM socket */
        if ((sc = socket(PF_CAN, SOCK_DGRAM, CAN_BCM)) < 0) {
            PRINT_ERROR("Error while opening BCM socket %s\n", strerror(errno));
            state = STATE_SHUTDOWN;
            return;
        }

        memset(&caddr, 0, sizeof(caddr));
        caddr.can_family = PF_CAN;
        /* can_ifindex is set to 0 (any device) => need for sendto() */
        
        PRINT_VERBOSE("connecting BCM socket...\n")
        if (connect(sc, (struct sockaddr *)&caddr, sizeof(caddr)) < 0) {
            PRINT_ERROR("Error while connecting BCM socket %s\n", strerror(errno));
            state = STATE_SHUTDOWN;
            return;
        }
        previous_state = STATE_BCM;
    }

    FD_ZERO(&readfds);
    FD_SET(sc, &readfds);
    FD_SET(client_socket, &readfds);

    ret = select((sc > client_socket)?sc+1:client_socket+1, &readfds, NULL, NULL, NULL);

    if (FD_ISSET(sc, &readfds)) {

        ret = recvfrom(sc, &msg, sizeof(msg), 0,
                (struct sockaddr*)&caddr, &caddrlen);

        ifr.ifr_ifindex = caddr.can_ifindex;
        ioctl(sc, SIOCGIFNAME, &ifr);

        /* Check if this is an error frame */
        if(msg.msg_head.can_id & 0x20000000) {
            if(msg.frame.can_dlc != CAN_ERR_DLC) {
                PRINT_ERROR("Error frame has a wrong DLC!\n")
            } else {
                sprintf(rxmsg, "< %s e %03X ", ifr.ifr_name,
                    msg.msg_head.can_id);

                for ( i = 0; i < msg.frame.can_dlc; i++)
                    sprintf(rxmsg + strlen(rxmsg), "%02X ",
                        msg.frame.data[i]);

                /* delimiter '\0' for Adobe(TM) Flash(TM) XML sockets */
                strcat(rxmsg, ">\0");
                send(client_socket, rxmsg, strlen(rxmsg) + 1, 0);
            }
        } else {
            sprintf(rxmsg, "< %s f %03X %d ", ifr.ifr_name,
                msg.msg_head.can_id, msg.frame.can_dlc);

            for ( i = 0; i < msg.frame.can_dlc; i++)
                sprintf(rxmsg + strlen(rxmsg), "%02X ",
                    msg.frame.data[i]);

            /* delimiter '\0' for Adobe(TM) Flash(TM) XML sockets */
            strcat(rxmsg, ">\0");

            send(client_socket, rxmsg, strlen(rxmsg) + 1, 0);
        }
    }
    
    if (FD_ISSET(client_socket, &readfds)) {

        char cmd;
        char bus_name[6];
        int found=0;
        int items;
        int i,j,k;
        struct can_bittiming timing;
        struct can_ctrlmode ctrlmode;

        receive_command(client_socket, buf);

        PRINT_VERBOSE("Received '%s'\n", buf)

        /* Extract busname and command */
        sscanf(buf, "< %6s %c ", bus_name, &cmd);

        /* Check if we work on this bus */
        found = 0;
        for(i=0;i<interface_count;i++) {
            if(!strcmp(interface_names[i], bus_name))
                found = 1;
        }
        if(found == 0) {
            PRINT_ERROR("Wrong bus name was specified!\n")
        } else {
            /* prepare bcm message settings */
            memset(&msg, 0, sizeof(msg));
            msg.msg_head.nframes = 1;

            switch (cmd) {
                case 'S': /* Send a single frame */
                    items = sscanf(buf, "< %6s %c %x %hhu "
                        "%hhx %hhx %hhx %hhx %hhx %hhx "
                        "%hhx %hhx >",
                        ifr.ifr_name,
                        &cmd, 
                        &msg.msg_head.can_id,
                        &msg.frame.can_dlc,
                        &msg.frame.data[0],
                        &msg.frame.data[1],
                        &msg.frame.data[2],
                        &msg.frame.data[3],
                        &msg.frame.data[4],
                        &msg.frame.data[5],
                        &msg.frame.data[6],
                        &msg.frame.data[7]);

                    if ( (items < 4) ||
                        (msg.frame.can_dlc > 8) ||
                        (items != 4 + msg.frame.can_dlc)) {
                        PRINT_ERROR("Syntax error in send command\n")
                        break;
                    }

                    msg.msg_head.opcode = TX_SEND;
                    msg.frame.can_id = msg.msg_head.can_id;

                    if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
                        caddr.can_ifindex = ifr.ifr_ifindex;
                        sendto(sc, &msg, sizeof(msg), 0,
                            (struct sockaddr*)&caddr, sizeof(caddr));
                    }

                    break;
                case 'A': /* Add a send job */
                    items = sscanf(buf, "< %6s %c %lu %lu %x %hhu "
                        "%hhx %hhx %hhx %hhx %hhx %hhx "
                        "%hhx %hhx >",
                        ifr.ifr_name,
                        &cmd, 
                        &msg.msg_head.ival2.tv_sec,
                        &msg.msg_head.ival2.tv_usec,
                        &msg.msg_head.can_id,
                        &msg.frame.can_dlc,
                        &msg.frame.data[0],
                        &msg.frame.data[1],
                        &msg.frame.data[2],
                        &msg.frame.data[3],
                        &msg.frame.data[4],
                        &msg.frame.data[5],
                        &msg.frame.data[6],
                        &msg.frame.data[7]);

                    if (items < 6)
                        break;
                    if (msg.frame.can_dlc > 8)
                        break;
                    if (items != 6 + msg.frame.can_dlc)
                        break;

                    msg.msg_head.opcode = TX_SETUP;
                    msg.msg_head.flags |= SETTIMER | STARTTIMER;
                    msg.frame.can_id = msg.msg_head.can_id;

                    if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
                        caddr.can_ifindex = ifr.ifr_ifindex;
                        sendto(sc, &msg, sizeof(msg), 0,
                            (struct sockaddr*)&caddr, sizeof(caddr));
                    }

                    break;
                case 'U': /* Update send job */
                    items = sscanf(buf, "< %6s %c %x %hhu "
                        "%hhx %hhx %hhx %hhx %hhx %hhx "
                        "%hhx %hhx >",
                        ifr.ifr_name,
                        &cmd, 
                        &msg.msg_head.can_id,
                        &msg.frame.can_dlc,
                        &msg.frame.data[0],
                        &msg.frame.data[1],
                        &msg.frame.data[2],
                        &msg.frame.data[3],
                        &msg.frame.data[4],
                        &msg.frame.data[5],
                        &msg.frame.data[6],
                        &msg.frame.data[7]);

                    if ( (items < 4) ||
                        (msg.frame.can_dlc > 8) ||
                        (items != 4 + msg.frame.can_dlc)) {
                        PRINT_ERROR("Syntax error in update send job command\n")
                        break;
                    }
                    
                    msg.msg_head.opcode = TX_SETUP;
                    msg.msg_head.flags  = 0;
                    msg.frame.can_id = msg.msg_head.can_id;

                    if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
                        caddr.can_ifindex = ifr.ifr_ifindex;
                        sendto(sc, &msg, sizeof(msg), 0,
                            (struct sockaddr*)&caddr, sizeof(caddr));
                    }

                    break;
                case 'D': /* Delete a send job */
                    items = sscanf(buf, "< %6s %c %x >",
                        ifr.ifr_name,
                        &cmd, 
                        &msg.msg_head.can_id);

                    if (items != 3)  {
                        PRINT_ERROR("Syntax error in delete job command\n")
                        break;
                    }

                    msg.msg_head.opcode = TX_DELETE;
                    msg.frame.can_id = msg.msg_head.can_id;

                    if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
                        caddr.can_ifindex = ifr.ifr_ifindex;
                        sendto(sc, &msg, sizeof(msg), 0,
                            (struct sockaddr*)&caddr, sizeof(caddr));
                    }

                    break;

                case 'R': /* Receive CAN ID with content matching */
                    items = sscanf(buf, "< %6s %c %lu %lu %x %hhu "
                        "%hhx %hhx %hhx %hhx %hhx %hhx "
                        "%hhx %hhx >",
                        ifr.ifr_name,
                        &cmd, 
                        &msg.msg_head.ival2.tv_sec,
                        &msg.msg_head.ival2.tv_usec,
                        &msg.msg_head.can_id,
                        &msg.frame.can_dlc,
                        &msg.frame.data[0],
                        &msg.frame.data[1],
                        &msg.frame.data[2],
                        &msg.frame.data[3],
                        &msg.frame.data[4],
                        &msg.frame.data[5],
                        &msg.frame.data[6],
                        &msg.frame.data[7]);

                    if (items < 6)
                        break;
                    if (msg.frame.can_dlc > 8)
                        break;
                    if (items != 6 + msg.frame.can_dlc)
                        break;

                    msg.msg_head.opcode = RX_SETUP;
                    msg.msg_head.flags  = SETTIMER;
                    msg.frame.can_id = msg.msg_head.can_id;

                    if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
                        caddr.can_ifindex = ifr.ifr_ifindex;
                        sendto(sc, &msg, sizeof(msg), 0,
                            (struct sockaddr*)&caddr, sizeof(caddr));
                    }

                    break;
                case 'F': /* Add a filter */
                    items = sscanf(buf, "< %6s %c %lu %lu %x >",
                        ifr.ifr_name,
                        &cmd, 
                        &msg.msg_head.ival2.tv_sec,
                        &msg.msg_head.ival2.tv_usec,
                        &msg.msg_head.can_id);

                    if (items != 5) {
                        PRINT_ERROR("syntax error in add filter command\n")
                        break;
                    }

                    msg.msg_head.opcode = RX_SETUP;
                    msg.msg_head.flags  = RX_FILTER_ID | SETTIMER;
                    msg.frame.can_id = msg.msg_head.can_id;

                    if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
                        caddr.can_ifindex = ifr.ifr_ifindex;
                        sendto(sc, &msg, sizeof(msg), 0,
                            (struct sockaddr*)&caddr, sizeof(caddr));
                    }

                    break;
                case 'X': /* Delete filter */
                    items = sscanf(buf, "< %6s %c %x >",
                        ifr.ifr_name,
                        &cmd, 
                        &msg.msg_head.can_id);

                    if (items != 3) {
                        PRINT_ERROR("syntax error in delete filter command\n")
                        break;
                    }

                    msg.msg_head.opcode = RX_DELETE;
                    msg.frame.can_id = msg.msg_head.can_id;
                    
                    if (!ioctl(sc, SIOCGIFINDEX, &ifr)) {
                        caddr.can_ifindex = ifr.ifr_ifindex;
                        sendto(sc, &msg, sizeof(msg), 0,
                            (struct sockaddr*)&caddr, sizeof(caddr));
                    }

                    break;
                case 'B': /* Set bitrate */
                    memset(&timing, 0, sizeof(timing));

                    items = sscanf(buf, "< %6s %c %x %x %x %x %x %x %x %x >",
                        bus_name,
                        &cmd,
                        &timing.bitrate,
                        &timing.sample_point,
                        &timing.tq,
                        &timing.prop_seg,
                        &timing.phase_seg1,
                        &timing.phase_seg2,
                        &timing.sjw,
                        &timing.brp);

                    if (items != 10) {
                        PRINT_ERROR("Syntax error in set bitrate command\n")
                        break;
                    }

                    can_set_bittiming(bus_name, &timing);

                    break;
                case 'C': /* Set control mode */
                    memset(&ctrlmode, 0, sizeof(ctrlmode));
                    ctrlmode.mask = CAN_CTRLMODE_LOOPBACK | CAN_CTRLMODE_LISTENONLY | CAN_CTRLMODE_3_SAMPLES;

                    items = sscanf(buf, "< %6s %c %u %u %u >",
                        bus_name,
                        &cmd,
                        &i,
                        &j,
                        &k);

                    if (items != 5) {
                        PRINT_ERROR("Syntax error in set controlmode command\n")
                        break;
                    }

                    if(i)
                        ctrlmode.flags |= CAN_CTRLMODE_LISTENONLY;
                    if(j)
                        ctrlmode.flags |= CAN_CTRLMODE_LOOPBACK;
                    if(k)
                        ctrlmode.flags |= CAN_CTRLMODE_3_SAMPLES;

                    can_set_ctrlmode(bus_name, &ctrlmode);

                    break;
                case 'E': /* Enable or disable statistics */
                    items = sscanf(buf, "< %6s %c %u >",
                        bus_name,
                        &cmd,
                        &i);

                    if (items != 3) {
                        PRINT_ERROR("Syntax error in statistics command\n")
                        break;
                    }

                    set_statistics(bus_name, i);

                    break;
                default:
                    PRINT_ERROR("unknown command '%c'.\n", cmd)
                    exit(1);
            }
        }
    }
}

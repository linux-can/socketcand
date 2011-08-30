#include "config.h"
#include "statistics.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>

#include "socketcand.h"

int statistics_ival = 0;

struct timeval *last_fired;

void *statistics_loop(void *ptr) {
    int items, found;
    struct timeval current_time;
    int elapsed;
    char buffer[STAT_BUF_LEN];
    /*int state;
    struct can_berr_counter errorcnt;*/
    FILE *proc_net_dev;
    struct proc_stat_entry proc_entry;
    char line[PROC_LINESIZE];

    /* sync with main thread */
    pthread_mutex_lock( &stat_mutex );
    pthread_cond_signal( &stat_condition );
    pthread_mutex_unlock( &stat_mutex );

    while(1) {
        /* read /proc/net/dev */
        proc_net_dev = fopen( "/proc/net/dev", "r" );
        if( proc_net_dev == NULL ) {
            printf( "could not open /proc/net/dev" );
            sleep(1);
            continue;
        }

        while(1) {
            if( fgets( line , PROC_LINESIZE, proc_net_dev ) == NULL )
                break;

            items = sscanf( line, " %7s %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
                proc_entry.device_name,
                &proc_entry.rbytes,
                &proc_entry.rpackets,
                &proc_entry.rerrs,
                &proc_entry.rdrop,
                &proc_entry.rfifo,
                &proc_entry.rframe,
                &proc_entry.rcompressed,
                &proc_entry.rmulticast,
                &proc_entry.tbytes,
                &proc_entry.tpackets,
                &proc_entry.terrs,
                &proc_entry.tdrop,
                &proc_entry.tfifo,
                &proc_entry.tcolls,
                &proc_entry.tcarrier,
                &proc_entry.tcompressed );

            proc_entry.device_name[strlen(proc_entry.device_name)-1] = '\0';

            if( items == 17 ) {
                /* do we care for this device? */
                found=0;
                if( !strcmp( bus_name, proc_entry.device_name ) ) {
                    found=1;
                    break;
                }
            }
        }
        fclose( proc_net_dev );

        gettimeofday(&current_time, 0);
        pthread_mutex_lock(&stat_mutex);

        if( statistics_ival == 0 )
            continue;

        elapsed = ((current_time.tv_sec - last_fired->tv_sec) * 1000 
                + (current_time.tv_usec - last_fired->tv_usec)/1000.0) + 0.5;

        if(elapsed >= statistics_ival) {
            /* If we didn't find the device there is something wrong. */
            if(found) {
                fprintf(stderr, "could not find device %s in /proc/net/dev\n", bus_name);
                continue;
            }

            /*
                * TODO this does not work for virtual devices. therefore it is commented out until
                * a solution is found to identify virtual CAN devices 
                */
            /*if( can_get_state( current_entry.bus_name, &state ) ) {
                printf( "unable to get state of %s\n", current_entry.bus_name );
                continue;
            }
            if( can_get_berr_counter( current_entry.bus_name, &errorcnt ) ) {
                printf( "unable to get error count of %s\n", current_entry.bus_name );
                continue;
            }*/
            
            snprintf( buffer, STAT_BUF_LEN, "< stat %u %u %u %u >", 
                    proc_entry.rbytes, 
                    proc_entry.rpackets, 
                    proc_entry.tbytes, 
                    proc_entry.tpackets);

            /* no lock needed here because POSIX send is thread-safe and does locking itself */
            send( client_socket, buffer, strlen(buffer), 0 );

            last_fired->tv_sec = current_time.tv_sec;
            last_fired->tv_usec = current_time.tv_usec;
        }
        pthread_mutex_unlock(&stat_mutex);

        usleep(10000);
    }

    return NULL;
}

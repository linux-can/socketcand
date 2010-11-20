#include "statistics.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <libsocketcan.h>

#include "socketcand.h"

struct stat_entry *stat_entries;
int stat_entries_allocated = 0;
int stat_entry_cnt = 0;

/*
 * activate statistics for a given bus name. Ival must be specified in ms.
 * if ival is zero a statistics job will be deactivated
 */
void set_statistics(char *bus_name, int ival) {
    struct timeval *current_time;
    struct stat_entry *our_entry = NULL;
    int i;

    pthread_mutex_lock(&stat_mutex);

    /* check if we need to create a new entry or update the old one */
    for( i=0; i<stat_entry_cnt; i++ ) {
        if( !strcmp(bus_name, stat_entries[i].bus_name) )
            our_entry = &stat_entries[i];
    }

    /* entry was not found. create a new one */
    if( our_entry == NULL ) {
        if( stat_entries_allocated == 0 ) {
            stat_entries = (struct stat_entry *) malloc( 5 * sizeof( struct stat_entry ) );
            stat_entries_allocated = 5;
        }

        if( stat_entries_allocated == stat_entry_cnt ) {
            stat_entries = (struct stat_entry *) realloc( &stat_entries, sizeof( struct stat_entry ) * ( stat_entry_cnt+5 ) );
            stat_entries_allocated += 5;
        }

        our_entry = &stat_entries[stat_entry_cnt];
        stat_entry_cnt++;
    }

    /* fill entry with data */
    current_time = malloc( sizeof( struct timeval ) );
    gettimeofday(current_time, 0);
    our_entry->bus_name = bus_name;
    our_entry->ival = ival;
    our_entry->last_fired = current_time;

    pthread_mutex_unlock( &stat_mutex );
}

void *statistics_loop(void *ptr) {
    int i,j, items, found;
    struct timeval current_time;
    int elapsed;
    struct stat_entry current_entry;
    char buffer[STAT_BUF_LEN];
    int state;
    struct can_berr_counter errorcnt;
    FILE *proc_net_dev;
    struct proc_stat_entry proc_entries[PROC_LINECOUNT];
    int proc_entry_cnt=0;
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
        for( i=0; i<PROC_LINECOUNT; i++ ) {
            if( fgets( line , PROC_LINESIZE, proc_net_dev ) == NULL )
                break;
            
            items = sscanf( line, " %7s %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
                proc_entries[i].device_name,
                &proc_entries[i].rbytes,
                &proc_entries[i].rpackets,
                &proc_entries[i].rerrs,
                &proc_entries[i].rdrop,
                &proc_entries[i].rfifo,
                &proc_entries[i].rframe,
                &proc_entries[i].rcompressed,
                &proc_entries[i].rmulticast,
                &proc_entries[i].tbytes,
                &proc_entries[i].tpackets,
                &proc_entries[i].terrs,
                &proc_entries[i].tdrop,
                &proc_entries[i].tfifo,
                &proc_entries[i].tcolls,
                &proc_entries[i].tcarrier,
                &proc_entries[i].tcompressed );

                proc_entries[i].device_name[strlen(proc_entries[i].device_name)-1] = '\0';
            if( items == 17 ) {
                /* do we care for this device? */
                found=0;
                for ( j=0; j < stat_entry_cnt; j++) {
                    if( !strcmp( stat_entries[i].bus_name, proc_entries[i].device_name ) ) {
                        found=1;
                        break;
                    }
                }

                /* if we don't need this device we can overwrite the data of the proc entry
                 * in the following step.
                 */
                if( !found )
                    i--;
            }
            else
               i--;
        }
        proc_entry_cnt = i;
        printf("proc-enty-cnt %u\n", i);
        fclose( proc_net_dev );

        gettimeofday(&current_time, 0);
        pthread_mutex_lock(&stat_mutex);

        /* loop through all statistics jobs */
        for(i=0;i<stat_entry_cnt;i++) {
            current_entry = stat_entries[i];

            if( current_entry.ival == 0 )
                continue;

            elapsed = ((current_time.tv_sec - current_entry.last_fired->tv_sec) * 1000 
                    + (current_time.tv_usec - current_entry.last_fired->tv_usec)/1000.0) + 0.5;

            if(elapsed >= current_entry.ival) {

                /* get values */
                /*if( can_get_state( current_entry.bus_name, &state ) ) {
                    printf( "unable to get state of %s\n", current_entry.bus_name );
                    continue;
                }
                if( can_get_berr_counter( current_entry.bus_name, &errorcnt ) ) {
                    printf( "unable to get error count of %s\n", current_entry.bus_name );
                    continue;
                }
                
                snprintf( buffer, STAT_BUF_LEN, "< %6s s %u %u %u >", current_entry.bus_name, state, errorcnt.txerr, errorcnt.rxerr );*/
                /* no lock needed here because POSIX send is thread-safe and does locking itself */
                send( client_socket, buffer, strlen(buffer), 0 );

                current_entry.last_fired->tv_sec = current_time.tv_sec;
                current_entry.last_fired->tv_usec = current_time.tv_usec;
            }
        }
        pthread_mutex_unlock(&stat_mutex);

        usleep(10000);
    }

    return NULL;
}

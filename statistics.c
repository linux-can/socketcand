#include "statistics.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>

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
    int i;
    struct timeval current_time;
    int elapsed;
    struct stat_entry current_entry;
    char buffer[STAT_BUF_LEN];

    /* sync with main thread */
    pthread_mutex_lock( &stat_mutex );
    pthread_cond_signal( &stat_condition );
    pthread_mutex_unlock( &stat_mutex );

    while(1) {
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
                
                snprintf( buffer, STAT_BUF_LEN, "< %6s s >", current_entry.bus_name );
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

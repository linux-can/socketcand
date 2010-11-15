#include "statistics.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>

struct stat_entry *stat_entries = NULL;
int stat_entries_allocated = 0;
int stat_entry_cnt = 0;

void set_statistics(char *bus_name, int ival) {
    if(stat_entries == NULL) {
        stat_entries = malloc(5 * sizeof(struct stat_entry));
        stat_entries_allocated = 5;
    }

    if(stat_entries_allocated == stat_entry_cnt) {
        stat_entries = realloc(&stat_entries, sizeof(struct stat_entry) * (stat_entry_cnt+5));
        stat_entries_allocated +=5;
    }

    stat_entries[stat_entry_cnt].bus_name = bus_name;
    stat_entries[stat_entry_cnt].ival = ival;
    stat_entry_cnt++;
}

void *statistic_loop(void *ptr) {
    int i;
    struct timeval *current_time;
    int elapsed;

    while(1) {
        gettimeofday(current_time, 0);
        for(i=0;i<stat_entry_cnt;i++) {
            elapsed = ((current_time->tv_sec - stat_entries[i].last_fired->tv_sec) * 1000 + (current_time->tv_usec - stat_entries[i].last_fired->tv_usec)/1000.0) + 0.5;
            if(elapsed >= stat_entries[i].ival) {
                printf("test\n");
                stat_entries[i].last_fired = current_time;
            }
        }

        usleep(10000);
    }
    return NULL;
}

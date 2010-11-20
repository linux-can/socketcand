#include <pthread.h>

#define STAT_BUF_LEN 512

void set_statistics(char *bus_name, int ival);
void *statistics_loop(void *ptr);

struct stat_entry {
    char *bus_name;
    int ival;
    struct timeval *last_fired;
};

pthread_mutex_t stat_mutex;
pthread_cond_t stat_condition;

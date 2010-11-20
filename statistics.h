#include <pthread.h>

#define STAT_BUF_LEN 512
#define PROC_LINESIZE 256
#define PROC_LINECOUNT 32

void set_statistics(char *bus_name, int ival);
void *statistics_loop(void *ptr);

struct stat_entry {
    char *bus_name;
    int ival;
    struct timeval *last_fired;
};

struct proc_stat_entry {
    char device_name[6];
    unsigned int rbytes;
    unsigned int rpackets;
    unsigned int rerrs;
    unsigned int rdrop;
    unsigned int rfifo;
    unsigned int rframe;
    unsigned int rcompressed;
    unsigned int rmulticast;
    unsigned int tbytes;
    unsigned int tpackets;
    unsigned int terrs;
    unsigned int tdrop;
    unsigned int tfifo;
    unsigned int tcolls;
    unsigned int tcarrier;
    unsigned int tcompressed;
};

pthread_mutex_t stat_mutex;
pthread_cond_t stat_condition;

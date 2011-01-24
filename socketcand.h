#define MAXLEN 100
#define MAX_BUSNAME 16+1
#define PORT 28600

#define STATE_NO_BUS 0
#define STATE_BCM 1
#define STATE_RAW 2
#define STATE_SHUTDOWN 3

#define PRINT_INFO(...) if(daemon_flag) syslog(LOG_INFO, __VA_ARGS__); else printf(__VA_ARGS__);
#define PRINT_ERROR(...) if(daemon_flag) syslog(LOG_ERR, __VA_ARGS__); else fprintf(stderr, __VA_ARGS__);
#define PRINT_VERBOSE(...) if(verbose_flag && !daemon_flag) printf(__VA_ARGS__);

extern int client_socket;
extern char **interface_names;
extern int interface_count;
extern int port;
extern struct in_addr laddr;
extern int verbose_flag;
extern int daemon_flag;

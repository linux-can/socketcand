#define MAXLEN 100
#define PORT 28600

/*
 * these values are used to subscribe to all possible
 * identifiers on a bus with normal or extended
 * identifier length.
 */
#define CANID_BASE_ALL 0xffffffff
#define CANID_EXTENDED_ALL 0xfffffffe

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

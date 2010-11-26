#define MAXLEN 100
#define PORT 28600

/*
 * these values are used to subscribe to all possible
 * identifiers on a bus with normal or extended
 * identifier length.
 */
#define CANID_BASE_ALL 0xffffffff
#define CANID_EXTENDED_ALL 0xfffffffe

extern int client_socket;
extern char **interface_names;
extern int interface_count;
extern int port;
extern struct in_addr laddr;
extern int verbose_flag;

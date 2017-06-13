#ifndef CHAT_NETWORK_H_
#define CHAT_NETWORK_H_

#define UNAME_SIZE    ((int)24)
#define IP_STR_SIZE   ((int)24)
#define PORT_STR_SIZE ((int)6)


enum ConnectionMode {
	CONMODE_HOST,
	CONMODE_CLIENT
};


typedef struct ConnectionInfo {
	char host_uname[UNAME_SIZE];
	char client_uname[UNAME_SIZE];
	char host_ip[IP_STR_SIZE];
	char client_ip[IP_STR_SIZE];
	char port[PORT_STR_SIZE];
	char* local_uname;
	char* remote_uname;
	char* local_ip;
	char* remote_ip;
	int local_fd;
	int remote_fd;
	enum ConnectionMode mode;
} ConnectionInfo;


extern const ConnectionInfo* initialize_connection(enum ConnectionMode mode);
extern void terminate_connection(const ConnectionInfo* cinfo);



#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#define RBNAME "regboard0"
#define START_PORT 50017

//timeout in ms (almeno 400!!)
#define TIMEOUT 2000
#define MAXATTEMPTS 3


class FzUDP {
public:
	FzUDP();
	~FzUDP();
	
	int Send(int blk,int fee,int cmd,const char *data,uint8_t *reply,int verbose=0);
	int Meter(double *time,double *trig,int *bitmask);
	bool SockOK();
private:
	int Open();
	bool fSockOK;
	
	int sockfd;
	struct sockaddr src;
	struct sockaddr_in sock_to;
	struct sockaddr_in sock_from;
};


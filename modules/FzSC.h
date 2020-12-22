#ifndef FZSC
#define FZSC

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
/*******************************************************************************
*                                                                              *
*                         Simone Valdre' - 22/12/2020                          *
*                  distributed under GPL-3.0-or-later licence                  *
*                                                                              *
*******************************************************************************/

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <termios.h>
#include "ShellColors.h"

//Default host name for Regional Board
#define RBNAME "regboard0"
//Default start port for UDP
#define START_PORT 50017
//SC message maximum length
#define MLENG 1000
//Listen timeout (in ms)
#define LTIME 2000
//Maximum number of attempts in sending slow control queries
#define MAXAT 3


class FzSC {
public:
	//Using UDP protocol by default. With serial=1 RS232 protocol is used instead (set target=<device>, e.g. "/dev/ttyUSB0")
	FzSC(const bool serial=false, const char *target=RBNAME,const bool keithley=false);
	~FzSC();
	
	int Send(int blk,int fee,int cmd,const char *data,uint8_t *reply,int verb=0);
	int Meter(double *time,double *trig,int *bitmask);
	int KSend(const char *data,char *reply,int verb=0);
	bool SockOK();
private:
	void crc2ascii(const uint8_t crc, uint8_t *buffer);
	int SCParse(char *,const uint8_t *,const int);
	int TTYOpen(const char *, speed_t speed);
	int UDPOpen(const char *);
	
	bool fSockOK,fSerial,fKeith;
	
	int sockfd;
	struct sockaddr src;
	struct sockaddr_in sock_to;
	struct sockaddr_in sock_from;
};

#endif

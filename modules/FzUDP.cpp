#include "FzUDP.h"


FzUDP::FzUDP() {
	if(Open()<0) fSockOK=false;
	else fSockOK=true;
}

FzUDP::~FzUDP() {
	if(fSockOK) close(sockfd);
}

void crc2ascii(uint8_t crc, uint8_t *buffer) {
	char str[3];
	
	sprintf(str,"%02X",crc);
	buffer[0]=(uint8_t)(str[0]);
	buffer[1]=(uint8_t)(str[1]);
	return;
}

int interpreta(char *sout,uint8_t *buffer,int N) {
	int i;
	uint8_t crc;
	unsigned crc_check;
	char buf[3],data[1000],stmp[1010];
	
	sout[0]='\0';
	for(i=0,crc=0;i<N;i++) {
		if(i<N-3) crc^=buffer[i];
		if(i==0) {
			switch(buffer[i]) {
				case 0x2:
					sprintf(sout,"\033[1;32mSTX\033[0;39m "); break;
				case 0x6:
					sprintf(sout,"\033[1;32mACK\033[0;39m "); break;
				case 0x15:
					sprintf(sout,"\033[1;31mNAK\033[0;39m "); break;
				case 0x18:
					sprintf(sout,"\033[1;33mTIM\033[0;39m "); break;
				case 0x1a:
					sprintf(sout,"\033[1;31mERR\033[0;39m "); break;
				case 0x1b:
					sprintf(sout,"\033[1;34mESC\033[0;39m "); break;
				default:
					sprintf(sout,"\033[1;31mx%02x\033[0;39m ",buffer[i]);
			}
		}
		if((i>0)&&(i<=4)) {
			if((buffer[i]>31)&&(buffer[i]<128)) data[i-1]=(char)(buffer[i]);
			else data[i-1]='?';
		}
		if(i==4) {
			data[4]='\0';
			if((data[0]=='E')||(data[0]=='e')) {
				sprintf(stmp,"\033[1mReg. brd\033[0m");
				strcat(sout,stmp);
			}
			else {
				sprintf(stmp,"B\033[1m%c%c%c\033[0m",data[0],data[1],data[2]);
				strcat(sout,stmp);
				switch(data[3]) {
					case '9':
						sprintf(stmp,"\033[1m  BC\033[0m");
						strcat(sout,stmp);
						break;
					case '8':
						sprintf(stmp,"\033[1m  PS\033[0m");
						strcat(sout,stmp);
						break;
					default:
						sprintf(stmp," FE\033[1m%c\033[0m",data[3]);
						strcat(sout,stmp);
				}
			}
			
			
		}
		if(i==5) {
			sprintf(stmp,". CMD \033[1m%02X\033[0m",buffer[i]);
			strcat(sout,stmp);
		}
		if((i>5)&&(i<N-3)) {
			if((buffer[i]>31)&&(buffer[i]<128)) data[i-6]=(char)(buffer[i]);
			else data[i-6]='?';
		}
		if((i>5)&&(i==N-3)) {
			data[i-6]='\0';
			if(strlen(data)>0) {
				sprintf(stmp,": \033[1m%s\033[0m",data);
				strcat(sout,stmp);
			}
			if(buffer[i]==3) sprintf(stmp," \033[1;32mETX\033[0;39m");
			else sprintf(stmp," \033[1;31mx%02x\033[0;39m",buffer[i]);
			strcat(sout,stmp);
		}
	}
	buf[0]=buffer[N-2];
	buf[1]=buffer[N-1];
	buf[2]='\0';
	sscanf(buf,"%X",&crc_check);
	sprintf(stmp," CRC=%2s",buf);
	strcat(sout,stmp);
	if(crc==crc_check) return 0;
	else return -1;
}

int FzUDP::Open() {
	int value,pcip;
	int pc_port=START_PORT,rb_port=50016;
	char nsrc[100],ndst[100];
	unsigned len;
	
	strcpy(ndst,RBNAME);
	gethostname(nsrc,sizeof(nsrc));
	
	//define transmitting socket structure
	sock_to.sin_addr.s_addr=*((int *)(gethostbyname(ndst)->h_addr_list[0]));
	sock_to.sin_port = ((rb_port & 0xff) << 8) | ((rb_port & 0xff00) >> 8);
	sock_to.sin_family = AF_INET;
	
	//define receiving socket structure
	pcip=*((int *)(gethostbyname(nsrc)->h_addr_list[0]));
	sock_from.sin_addr.s_addr = pcip;
	sock_from.sin_port = ((pc_port & 0xff) << 8) | ((pc_port & 0xff00) >> 8);
	sock_from.sin_family = AF_INET;
	
	//open socket (the same for tx and rx!)
	sockfd = socket(AF_INET,SOCK_DGRAM,0);
	if(getsockopt(sockfd,SOL_SOCKET,SO_KEEPALIVE,&value,&len)<0) {
		perror("getsockopt");
		return -1;
	}
	
	//bind socket
	errno = 0;
	while(bind(sockfd,(struct sockaddr *)&sock_from,16) == -1) {
		if(errno != 48) {
			if(errno == EADDRINUSE) {
				pc_port++;
				sock_from.sin_port = ((pc_port & 0xff) << 8) | ((pc_port & 0xff00) >> 8);
			}
			else {
				perror(ndst);
				close(sockfd);
				return -1;
			}
		}
	}
	printf("\033[1;32mUDP socket opened...\033[0;39m sending from %s:%d (%d.%d.%d.%d) to %s:%d\n\n",nsrc,rb_port,pcip&0xff,(pcip>>8)&0xff,(pcip>>16)&0xff,(pcip>>24)&0xff,ndst,pc_port);
	return 0;
}

int FzUDP::Send(int blk,int fee,int cmd,const char *data,uint8_t *reply,int verbose /*=0*/) {
	if(!fSockOK) {
		printf("\033[1;31mSCSocket::Send\033[0;39m Socket is not open...\n");
		return -10;
	}
	
	char intmess[1000],intreply[1000],ctmp[10];
	uint8_t answer[1000],crc;
	int N,i,j,jtime,h,tentativo,crc_ok,err=0;
	unsigned dim;
	int rep;
	double oldavt,avt=0,sigt=0;
	int k=0,trep;
	struct timeval tstart,tstop,delta;
	
	crc=0;
	answer[0]=(uint8_t)(0x02);
	crc^=(uint8_t)(0x02);
	sprintf(ctmp,"%03X",blk);
	for(i=0;i<3;i++) {
		answer[i+1]=(uint8_t)(ctmp[i]);
		crc^=(uint8_t)(ctmp[i]);
	}
	sprintf(ctmp,"%1X",fee);
	answer[4]=(uint8_t)(ctmp[0]);
	crc^=(uint8_t)(ctmp[0]);
	answer[5]=(uint8_t)(cmd);
	crc^=(uint8_t)(cmd);
	for(N=6,i=0;i<(int)(strlen(data));i++,N++) {
		answer[6+i]=(uint8_t)(data[i]);
		crc^=(uint8_t)(data[i]);
	}
	answer[N++]=3;
	crc2ascii(crc,answer+N);
	N+=2;
	if(interpreta(intmess,answer,N)) return -2;
	if(verbose) {printf("\033[1;32m[sending]\033[0;39m %s\n",intmess);}
	jtime=0;
	for(tentativo=0;tentativo<MAXATTEMPTS;tentativo++) {
		if(jtime==0) {
			i=sendto(sockfd,(void *)answer,N,0,(struct sockaddr *)&sock_to,sizeof(struct sockaddr));
			gettimeofday(&tstart,NULL);
			rep=0;
		}
		for(;jtime<TIMEOUT;jtime++) {
			usleep(1000);
			i=recvfrom(sockfd,(void *)reply,100000,MSG_DONTWAIT,&src,&dim);
			if(i>=0) {
				gettimeofday(&tstop,NULL);
				reply[i]=(uint8_t)'\0';
				break;
			}
		}
		if(i>=0) {
			timersub(&tstop,&tstart,&delta);
			trep=1000*delta.tv_sec+(delta.tv_usec+500)/1000;
			crc_ok=interpreta(intreply,reply,i);
			if(((reply[0]==6)||(reply[0]==0x1b))&&(crc_ok==0)) {
				for(h=1;h<6;h++) {
					if(answer[h]!=reply[h]) {
						rep++;
					}
				}
				if(rep==0) {
					k++; oldavt=avt;
					avt+=(trep-avt)/k;
					sigt+=(trep-oldavt)*(trep-avt);
					if(verbose) printf("\033[1;34m[ reply ]\033[0;39m %s in %4d ms\n",intreply,trep);
					break;
				}
			}
		}
		if(i<0) {
			jtime=0;
			err=-1;
			if(verbose) printf("\033[1;33m[timeout]\033[0;39m %s\n",intmess);
		}
		else {
			if(crc_ok) {
				jtime=0;
				err=-2;
				if(verbose) {
					printf("\033[1;31m[CRC err]\033[1;34m");
					for(j=0;j<i;j++) {
						printf(" %02X",reply[j]);
					}
					printf("\033[0;39m");
					printf("\n");
				}
			}
			else if(rep) {
				//IN QUESTO CASO NON RESETTO IL TIMEOUT POICHÉ SI RISCHIANO EFFETTI A VALANGA
				err=-3;
				if(verbose) printf("\033[1;35m[INC.REP]\033[0;39m %s\n",intreply);
			}
			else if(reply[0]==0x18) {
				jtime=0;
				err=-4;
				if(verbose) printf("\033[1;33m[TIMEOUT]\033[0;39m %s\n",intreply);
			}
			else {
				jtime=0;
				err=-5;
				if(verbose) printf("\033[1;31m[ ERROR ]\033[0;39m %s\n",intreply);
			}
		}
	}
	usleep(20000);
	if(tentativo>=MAXATTEMPTS) {
		return err;
	}
	N=strlen((char *)reply);
	for(i=6;i<N-3;i++) {
		reply[i-6]=reply[i];
	}
	reply[i-6]='\0';
	return 0;
}

int FzUDP::Meter(double *time,double *trig,int *bitmask) {
	if(!fSockOK) {
		printf("\033[1;31mSCSocket::Meter\033[0;39m Socket is not open...\n");
		return -10;
	}
	
	uint8_t reply[1000];
	char data[3];
	unsigned long long r;
	int i;
	
	if(Send(0xe00,0,0x85,"3",reply)<0) return -1;
	if(sscanf((char *)reply,"0|%04LX",&r)) *bitmask=(int)r;
	else return -1;
	if(Send(0xe00,0,0x85,"83",reply)<0) return -1;
	if(sscanf((char *)reply,"0|%04LX",&r)) *time=((double)(r<<16))/150.e6;
	else return -1;
	for(i=0;i<12;i++) {
		sprintf(data,"%02X",0x84+i);
		if(Send(0xe00,0,0x85,data,reply)<0) return -1;
		if(sscanf((char *)reply,"0|%04LX",&r)) trig[i]=(double)r;
		else return -1;
	}
	return 0;
}

bool FzUDP::SockOK() {
	return fSockOK;
}

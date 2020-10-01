#include "FzSC.h"

FzSC::FzSC(const bool serial, const char *target) {
	if(serial) {
		fSerial=1;
		if(TTYOpen(target)<0) fSockOK=false;
		else fSockOK=true;
	}
	else {
		fSerial=0;
		if(UDPOpen(target)<0) fSockOK=false;
		else fSockOK=true;
	}
}

FzSC::~FzSC() {
	if(fSockOK) close(sockfd);
}

void FzSC::crc2ascii(uint8_t crc, uint8_t *buffer) {
	char str[3];
	
	sprintf(str,"%02X",crc);
	buffer[0]=(uint8_t)(str[0]);
	buffer[1]=(uint8_t)(str[1]);
	return;
}
//Function used to read SC messages and print them in human readable format
int FzSC::SCParse(char *sout,const uint8_t *buffer,const int N) {
	int i;
	uint8_t crc;
	unsigned crc_check;
	char buf[3],data[1000],stmp[1020];
	
	sout[0]='\0';
	for(i=0,crc=0;i<N;i++) {
		if(i<N-3) crc^=buffer[i];
		if(i==0) {
			switch(buffer[i]) {
				case 0x2:
					sprintf(sout,GRN "STX " NRM); break;
				case 0x6:
					sprintf(sout,GRN "ACK " NRM); break;
				case 0x15:
					sprintf(sout,RED "NAK " NRM); break;
				case 0x18:
					sprintf(sout,YEL "TIM " NRM); break;
				case 0x1a:
					sprintf(sout,RED "ERR " NRM); break;
				case 0x1b:
					sprintf(sout,BLU "ESC " NRM); break;
				default:
					sprintf(sout,RED "x%02x " NRM,buffer[i]);
			}
		}
		if((i>0)&&(i<=4)) {
			if((buffer[i]>31)&&(buffer[i]<128)) data[i-1]=(char)(buffer[i]);
			else data[i-1]='?';
		}
		if(i==4) {
			data[4]='\0';
			if((data[0]=='E')||(data[0]=='e')) {
				sprintf(stmp,BLD "Reg. brd" NRM);
				strcat(sout,stmp);
			}
			else {
				sprintf(stmp,"B" BLD "%c%c%c" NRM,data[0],data[1],data[2]);
				strcat(sout,stmp);
				switch(data[3]) {
					case '9':
						sprintf(stmp,BLD "  BC" NRM);
						strcat(sout,stmp);
						break;
					case '8':
						sprintf(stmp,BLD "  PS" NRM);
						strcat(sout,stmp);
						break;
					default:
						sprintf(stmp," FE" BLD"%c" NRM,data[3]);
						strcat(sout,stmp);
				}
			}
		}
		if(i==5) {
			sprintf(stmp,". CMD " BLD "%02X" NRM,buffer[i]);
			strcat(sout,stmp);
		}
		if((i>5)&&(i<N-3)) {
			if((buffer[i]>31)&&(buffer[i]<128)) data[i-6]=(char)(buffer[i]);
			else data[i-6]='?';
		}
		if((i>5)&&(i==N-3)) {
			data[i-6]='\0';
			if(strlen(data)>0) {
				sprintf(stmp,": " BLD "%s" NRM,data);
				strcat(sout,stmp);
			}
			if(buffer[i]==3) sprintf(stmp,GRN " ETX" NRM);
			else sprintf(stmp,RED " x%02x" NRM,buffer[i]);
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

int FzSC::TTYOpen(const char *device) {
	struct termios config;
	
	//Open device
	if((sockfd=open(device, O_RDWR | O_NOCTTY | O_NDELAY))<0) { perror(device); return -1;}
	
	//Check if the file descriptor is pointing to a TTY device or not
	if(!isatty(sockfd)) { perror(device); close(sockfd); return -1;}
	
	//Get the current configuration of the serial interface
	if(tcgetattr(sockfd,&config)<0) { perror(device); close(sockfd); return -1;}
	
	// Input flags - Turn off input processing
	config.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
	
	// Output flags - Turn off output processing
	config.c_oflag = 0;
	
	// No line processing
	config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
	
	// Turn off character processing
	config.c_cflag &= ~(CSIZE | PARENB); config.c_cflag |= CS8;
	
	// One input byte is enough to return from read(); inter-character timer off
	config.c_cc[VMIN]  = 1; config.c_cc[VTIME] = 0;
	
	// Communication speed (simple version, using the predefined constants)
	if(cfsetispeed(&config,B115200)<0 || cfsetospeed(&config,B115200)<0) { perror(device); close(sockfd); return -1;}
	
	// Finally, apply the configuration
	if(tcsetattr(sockfd,TCSAFLUSH,&config)<0) { perror(device); close(sockfd); return -1;}
	return 0;
}

int FzSC::UDPOpen(const char *ndst) {
	int value,pcip;
	int pc_port=START_PORT,rb_port=50016;
	char nsrc[100];
	unsigned len;
	
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
	printf(GRN "FzSC::Open" NRM " sending from %s:%d (%d.%d.%d.%d) to %s:%d\n\n",nsrc,rb_port,pcip&0xff,(pcip>>8)&0xff,(pcip>>16)&0xff,(pcip>>24)&0xff,ndst,pc_port);
	return 0;
}

int FzSC::Send(int blk,int fee,int cmd,const char *data,uint8_t *reply,int verb/*=0*/) {
	//Check file descriptor
	if(!fSockOK) {
		printf(RED "FzSC::Send" NRM " socket is not open...\n");
		return -10;
	}
	
	char intmess[MLENG],intreply[MLENG],ctmp[10];
	uint8_t answer[MLENG],part[MLENG],crc=0;
	int N,M,i,j,trep,tent,crc_ok,err=0;
	unsigned dim;
	ssize_t ret;
	struct timeval tstart,tstop,delta;
	
	//// Preparing the query message
	//STX
	answer[0]=(uint8_t)(0x02);
	crc^=(uint8_t)(0x02);
	//Block number
	sprintf(ctmp,"%03X",blk);
	for(i=0;i<3;i++) {
		answer[i+1]=(uint8_t)(ctmp[i]);
		crc^=(uint8_t)(ctmp[i]);
	}
	//FEE number
	sprintf(ctmp,"%1X",fee);
	answer[4]=(uint8_t)(ctmp[0]);
	crc^=(uint8_t)(ctmp[0]);
	//Command
	answer[5]=(uint8_t)(cmd);
	crc^=(uint8_t)(cmd);
	//DATA
	for(N=6,i=0;i<(int)(strlen(data));i++,N++) {
		answer[6+i]=(uint8_t)(data[i]);
		crc^=(uint8_t)(data[i]);
	}
	//ETX
	answer[N++]=3;
	//CRC
	crc2ascii(crc,answer+N);
	N+=2;
	//Consistency check and construction of human readable message
	if(SCParse(intmess,answer,N)) {
		printf(RED "FzSC::Send" NRM " query construction failed\n");
		return -2;
	}
	if(verb) printf(GRN "[sending]" NRM" %s\n",intmess);
	
	//Loop for 3 query attempts
	for(tent=0;tent<MAXAT;tent++) {
		//Send SC query and store the time
		if(fSerial) ret=write(sockfd,answer,N);
		else ret=sendto(sockfd,(void *)answer,N,0,(struct sockaddr *)&sock_to,sizeof(struct sockaddr));
		if(ret!=N) {err=-10; break;}
		gettimeofday(&tstart,NULL);
		M=0; err=0; j=-1;
		//In case of RS232 Wait at least 5ms (no data arrive before)
		if(fSerial) usleep(5000);
		for(;;) {
			//Read reply message piece by piece
			if(fSerial) ret=read(sockfd,part,MLENG);
			else ret=recvfrom(sockfd,(void *)reply,100000,MSG_DONTWAIT,&src,&dim);
			if(ret<0&&errno==EAGAIN) ret=0;
			if(ret<0) {err=-10; break;}
			if(ret+M>=MLENG) {err=-3; break;} //Too long answer is treated as an inconsistent reply
			gettimeofday(&tstop,NULL);
			timersub(&tstop,&tstart,&delta);
			trep=1000000*delta.tv_sec+delta.tv_usec;
			//Putting together the pieces
			for(i=0;i<ret;i++) {
				reply[M+i]=part[i];
				if(part[i]==3) j=2; //After ETX (0x03) two more bytes are expected (CRC)
				else j--;
			}
			M+=ret;
			if(j==0) {reply[M]=(uint8_t)'\0'; break;} //Message concluded
			if(trep>=LTIME*1000) break; //TIMEOUT
			usleep(((int)((trep+1500)/1000))*1000-trep); //Sleep the time needed to keep a 1ms interval between two read calls
		}
		//Bad communication
		if(err==-10) break;
		if(j) err=-1; //If j!=0 message is not concluded
		if(fSerial) usleep(1000); //Wait NL char to be sent by FEE
		
		if(!err) {
			if((crc_ok=SCParse(intreply,reply,M))) err=-2;
			else if((reply[0]==6)||(reply[0]==0x1b)) {
				j=0;
				for(i=1;i<6;i++) if(answer[i]!=reply[i]) j++;
				if(j) err=-3; //Inconsistent reply
				else { //Good reply
					if(verb) printf("\033[1;34m[ reply ]\033[0;39m %s in %4d ms\n",intreply,(trep+500)/1000);
					break;
				}
			}
			else if(reply[0]==0x18) err=-4;
			else err=-5;
		}
		switch(err) {
			case -1:
				if(verb) printf(YEL "[timeout]" NRM " %s\n",intmess);
				break;
			case -2:
				if(verb) {
					printf(RED "[CRC err]" BLU);
					for(i=0;i<M;i++) {
						printf(" %02X",reply[i]);
					}
					printf(NRM "\n");
				}
				break;
			case -3:
				if(verb) printf(MAG "[INC.REP]" NRM " %s\n",intreply);
				usleep(LTIME*1000);
				break;
			case -4:
				if(verb) printf(YEL "[TIMEOUT]" NRM " %s\n",intreply);
				break;
			default:
				if(verb) printf(RED "[ ERROR ]" NRM" %s\n",intreply);
				break;
		}
		if(fSerial) tcflush(sockfd,TCIOFLUSH);
		else ret=recvfrom(sockfd,(void *)reply,100000,MSG_DONTWAIT,&src,&dim);
	}
	if(err==-10) {
		printf(RED "FzSC::Send" NRM " socked died\n");
		return err;
	}
	
	//Returning data only
	if(!err) {
		N=strlen((char *)reply);
		for(i=6;i<N-3;i++) {
			reply[i-6]=reply[i];
		}
		reply[i-6]='\0';
	}
	if(fSerial) tcflush(sockfd,TCIOFLUSH);
	return err;
}

int FzSC::Meter(double *time,double *trig,int *bitmask) {
	if(!fSockOK) {
		printf(RED "FzSC::Meter" NRM " socket is not open...\n");
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

bool FzSC::SockOK() {
	return fSockOK;
}

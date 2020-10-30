#include "FzTest.h"

int main(int argc, char *argv[]) {
	int c,ret,tmp,blk=0,fee=0,dump=0;
	bool verb=false,serial=true,keith=false,autom=true,hv=false;
	char device[SLENG]="/dev/ttyUSB0",kdevice[SLENG]="/dev/ttyUSB1",hname[SLENG]="regboard0",romfile[MLENG]="";
	
	//Decode options
	while((c=getopt(argc,argv,"hvmud:n:k:b:f:Hr:w:"))!=-1) {
		switch(c) {
			case 'v':
				verb=true;
				break;
			case 'u':
				serial=false;
				break;
			case 'm':
				autom=false;
				break;
			case 'd':
				if(optarg!=NULL && strlen(optarg)<SLENG) strcpy(device,optarg);
				break;
			case 'n':
				if(optarg!=NULL && strlen(optarg)<SLENG) strcpy(hname,optarg);
				break;
			case 'k':
				if(optarg!=NULL && strlen(optarg)<SLENG) strcpy(kdevice,optarg);
				keith=true;
				break;
			case 'b':
				if(optarg!=NULL) tmp=atoi(optarg);
				if(tmp>=0 && tmp<3584) blk=tmp;
				break;
			case 'f':
				if(optarg!=NULL) tmp=atoi(optarg);
				if(tmp>=0 && tmp<8) fee=tmp;
				break;
			case 'H': hv=true; break;
			case 'r':
				if(optarg!=NULL && strlen(optarg)<MLENG) strcpy(romfile,optarg);
				dump= 1;
				break;
			case 'w':
				if(optarg!=NULL && strlen(optarg)<MLENG) strcpy(romfile,optarg);
				dump=-1;
				break;
			case 'h': default:
				if(c!='h') printf(YEL "fz-test " NRM ": Unrecognized option\n");
				printf("\n**********  HELP **********\n");
				printf("    -h         this help\n");
				printf("    -v         enable verbose output\n");
				printf("    -m         manual operation (skip automatic checks)\n");
				printf("    -u         use UDP protocol (via RB) [by default direct RS232 is used]\n");
				printf("    -d <dev>   specify the FEE serial device (used only without UDP) [default: /dev/ttyUSB0]\n");
				printf("    -n <dev>   specify the RB hostname (used only with UDP) [default: regboard0]\n");
				printf("    -k <dev>   specify the Keithley 2000 device [default: disabled]\n");
				printf("    -b <blk>   specify the block ID [0-3584, default: 0]\n");
				printf("    -f <fee>   specify the FEE ID   [0-   7, default: 0]\n");
				printf("    -H         test HV outputs (WARNING: don't touch the card and be sure that no detector is connected!)\n");
				printf("    -r <file>  read all the EEPROM data and dump it to a file\n");
				printf("    -w <file>  write a file content to the EEPROM (line structure: \"<address (DEC)> <content (HEX)>\")\n");
				printf("\n");
				return 0;
		}
	}
	
	//Open FEE or RB
	FzSC sock(serial,serial?device:hname);
	if(!(sock.SockOK())) return 0;
	
	//Open Keithley
	FzSC *ksock=nullptr;
	if(keith&&(!dump)) {
		ksock=new FzSC(true,kdevice,true);
		if(ksock->SockOK()==false) {
			delete ksock;
			ksock=nullptr;
		}
	}
	
	FzTest test(&sock,blk,fee,verb);
	if(ksock!=nullptr) test.KeithleySetup(ksock);
	
	if(dump) {
		if(strlen(romfile)<=0) {
			printf(RED "fz-test " NRM "empty file name!\n");
			return -1;
		}
		if(dump>0) {
			if((ret=test.FullRead(romfile))<0) goto err;
		}
		else {
			if((ret=test.FullWrite(romfile))<0) goto err;
		}
	}
	else if(autom) {
		if((ret=test.FastTest(hv))<0) goto err;
		test.Report();
		if((ret=test.Guided())<0) goto err;
	}
	else {
		if((ret=test.Manual())<0) goto err;
	}
	return 0;
	
	err:
	switch(ret) {
		case -1: printf(RED "fz-test " NRM "many timeouts occurred, check FEE geo number or RS232 connection\n"); break;
		case -2: case -3: case -4: case -5:
			printf(RED "fz-test " NRM "FEE is connected but many errors occurred (%d), check RS232 connection\n",ret); break;
		case -10: printf(RED "fz-test " NRM "device is not ready (check the software)\n"); break;
		case -20: printf(RED "fz-test " NRM "anomalous SC replies (check the software and/or the firmware)\n"); break;
		default: printf(RED "fz-test " NRM "unhandled error...\n");
	}
	return -1;
}

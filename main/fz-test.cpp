#include "FzSC.h"

//String maximum length
#define SLENG 100
//Last versions of PIC and FPGA firmwares
#define LASTVPICV3 "28/01/2016"
#define LASTVPICV4 "13/03/2018"
#define LASTVFPGA  "19/09/2019"

//Info structure
typedef struct {
	int v4,sn;
	char vPIC[11],vFPGA[2][11];
	int temp[6],lv[19],gomask,bl[12],hvmask;
} FEEData;

//Constants
static const int reg[3]={1036,1336,1536};
static const char lFPGA[2][2]={"A","B"};
static const char lChan[3][4]={"Si1","Si2","CsI"};
static const char lADC[6][4]={"QH1"," I1","QL1"," Q2"," I2"," Q3"};
static const char lvlabel[19][50]={Mag "      VP5REFA - M121" NRM,Mag "      VP5REFB - M54 " NRM,Mag "         VM27 - M78 " NRM,Cya "       VM2A1A - M92 " NRM,Cya "       VM2A2A - M114" NRM,Cya "       VM2A1B - M37 " NRM,Cya "       VM2A2B - M109" NRM,Blu "         VP37 - M33 " NRM,Cya "       VP3A1A - M104" NRM,Cya "       VP3A2A - M116" NRM,Cya "       VP3A1B - M47 " NRM,Cya "       VP3A2B - M111" NRM,Blu "        VP33A - M89 " NRM,Blu "        VP33D - M85 " NRM,Blu "         VP25 - M3  " NRM,Blu "          VP1 - M119" NRM,Blu "        VP18A - M103" NRM,Blu "        VP18B - M118" NRM,Blu "        VP18D - M90 " NRM};
static const float vref[19]={5000,5000,2500,2000,2000,2000,2000,3700,3000,3000,3000,3000,3300,3300,2500,1000,1800,1800,1800};
static const char lvnotes[19][15]={"From VP12_0","From VP12_0","From VP5_5_IN","From VM27","From VM27","From VM27","From VM27","From VP5_5_IN","From VP37","From VP37","From VP37","From VP37","From VP37","From VP37","From VP5_5_IN","From VP25","From VP25","From VP25","From VP25"};
static const float blref3[6]={-7400,-5650,-4500,-7400,-5650,-7400};
static const float blref5[6]={-7400,-5100,-6150,-7400,-4250,-7400};
static const char hvissues[4][15]={"V mismatch","Wrong I offset","I mismatch","Unstable HV"};

//Global flags
static int verb=0;

int offcurve(FzSC *sock,int blk,int fee) {
	FILE *fout;
	int dac,mes,c,ret=0,N;
	char query[100];
	uint8_t reply[MLENG];
	
	fout=fopen("output.txt","w");
	if(fout==NULL) return 0;
	
	for(dac=0;dac<1024;dac+=10) {
		for(c=0;c<6;c++) {
			sprintf(query,"%s,%d,%d",lFPGA[c/3],c%3+1,dac);
			if((ret=sock->Send(blk,fee,0x89,query,reply,verb))) goto err;
		}
		fprintf(fout,"%4d",dac);
		printf("%4d",dac);
		for(c=0;c<6;c++) {
			sprintf(query,"%s,0x%d",lFPGA[c/3],reg[c%3]);
			if((ret=sock->Send(blk,fee,0x85,query,reply,verb))) goto err;
			N=sscanf((char *)reply,"0|%d",&mes);
			if(N==1) mes=(mes<32768)?mes:(mes-65536);
			else mes=-10000;
			fprintf(fout," % 6d",mes);
			printf(" % 6d",mes);
		}
		fprintf(fout,"\n");
		printf("\n");
	}
	err:
	fclose(fout);
	return ret;
}

//Fails:
//	   1 -> Overheat		=> check cooling
//	   2 -> SN not set		=> set SN
//	   4 -> Obsolete fw		=> reload new firmware
//	   8 -> Wrong voltages	=> check components
//	  16 -> Broken pre-amp	=> check components
//	  32 -> Bad offsets		=> regulate offsets, then check components
//	  64 -> Uncalibrated HV	=> calibrate HV
//	 128 -> HV not tested	=> test HV
//	 256 -> V mismatch		=> calibrate ADC only
//	 512 -> I not 0			=> calibrate ADC only
//	1024 -> I mismatch		=> check components
//	2048 -> Unstable HV		=> check components

int report(FEEData *data,int hvstat) {
	int c,status,failmask=0;
	float ref;
	
	printf("\n\n");
	printf(BLD "                         **************************************************\n");
	printf("                         *                  TEST  REPORT                  *\n");
	printf("                         **************************************************\n\n" NRM);
	printf(BLD "      Parameter      Res.         Value         Reference  Note\n" NRM);
	printf("----------------------------------------------------------------------------------------------------\n");
	
	//Card type and serial number
	printf(BLD "Card Type and SN     " NRM);
	if((data->v4<0)||(data->sn<0)) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
	else {
		if(data->sn==65535) {
			failmask|=2;
			printf(RED "Fail" NRM "                 %4s            No SN data\n",data->v4?"v4/5":"  v3");
		}
		else printf(GRN "Pass" NRM "             %4s-%03d\n",data->v4?"v4/5":"  v3",data->sn);
	}
	
	//PIC and FPGA version
	printf(BLD "\nFirmware:\n" NRM);
	printf("         PIC Version ");
	if(data->vPIC[0]=='\0') printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
	else {
		if(strcmp(data->vPIC,data->v4?LASTVPICV4:LASTVPICV3)) {
			failmask|=4;
			printf(YEL"Warn" NRM " %20s %10s Obsolete firmware\n",data->vPIC,data->v4?LASTVPICV4:LASTVPICV3);
		}
		printf(GRN "Pass" NRM " %20s %10s\n",data->vPIC,data->v4?LASTVPICV4:LASTVPICV3);
	}
	for(c=0;c<2;c++) {
		printf("      FPGA %s Version ",lFPGA[c]);
		if(data->vFPGA[c][0]=='\0') printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
		else {
			if(strcmp(data->vFPGA[c],LASTVFPGA)) {
				failmask|=4;
				printf(YEL"Warn" NRM " %20s %10s Obsolete firmware\n",data->vFPGA[c],LASTVFPGA);
			}
			else printf(GRN "Pass" NRM " %20s %10s\n",data->vFPGA[c],LASTVFPGA);
		}
	}
	
	//Temperatures
	status=0;
	for(c=0;c<6;c++) {
		if(data->temp[c]<0) {status=-1; break;}
		if(data->temp[c]>=70) status|=2;
		else if(data->temp[c]>=50) status|=1;
	}
	printf(BLD "\nTemperatures (in °C) " NRM);
	if(status<0) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
	else {
		if(status>=1) printf(YEL"Warn" NRM "    ");
		else printf(GRN "Pass" NRM "    ");
		for(c=0;c<6;c++) {
			if(c>0) printf(",");
			printf("%2d",data->temp[c]);
		}
		if(status>1) {
			failmask|=1;
			printf("     <50-70 Overheat\n");
		}
		else if(status==1) printf("     <50-70 Warm\n");
		else printf("     <50-70\n");
	}
	
	//Low voltages
	printf(BLD "\nLow voltages (in mV, measured by " Mag "M46" BLD ", " Cya "M1" BLD " and " Blu "M2" BLD "):\n" NRM);
	for(c=0;c<19;c++) {
		if(data->lv[c]==-2) continue;
		printf("%s ",lvlabel[c]);
		if(data->lv[c]<0) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
		else {
			if(fabs(vref[c]-(double)(data->lv[c]))/vref[c]>=0.02) {
				failmask|=8;
				printf(RED "Fail" NRM);
			}
			else printf(GRN "Pass" NRM);
			printf(" %20d %10.0f %s\n",data->lv[c],vref[c],lvnotes[c]);
		}
	}
	
	// Go/NoGo
	printf(BLD "\nPre-amplifiers       " NRM);
	if(data->gomask<0) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
	else {
		if(data->gomask==63) printf(GRN "Pass" NRM "         ");
		else {
			failmask|=16;
			printf(RED "Fail" NRM "         ");
		}
		for(c=0;c<6;c++) printf(" %1d",((data->gomask)&(1<<c))>>c);
		printf("       1=OK ");
		if(data->gomask<63) {
			printf("Broken:");
			for(c=0;c<6;c++) {
				if(((data->gomask)&(1<<c))==0) printf(" %s-%s",lChan[c%3],lFPGA[c/3]);
			}
		}
		printf("\n");
	}
	
	//Baseline offsets
	printf(BLD "\nBaseline offset (in ADC units):\n" NRM);
	for(c=0;c<12;c++) {
		printf("               %s-%s ",lADC[c%6],lFPGA[c/6]);
		if(data->bl[c]<-8500) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
		else {
			ref=(data->v4)?blref5[c%6]:blref3[c%6];
			if(fabs((ref-(double)(data->bl[c]))/ref)>=0.05) {
				failmask|=32;
				printf(RED "Fail" NRM);
			}
			else printf(GRN "Pass" NRM);
			printf(" % 20d % 10.0f\n",data->bl[c],ref);
		}
	}
	
	//HV calibration status
	printf(BLD "\nHigh Voltage:\n" NRM);
	printf("  Calibration status ");
	if(data->hvmask<0) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
	else {
		if(data->hvmask==15) printf(GRN "Pass" NRM "             ");
		else {
			failmask|=64;
			printf(RED "Fail" NRM "             ");
		}
		for(c=0;c<4;c++) printf(" %1d",((data->hvmask)&(1<<c))>>c);
		printf("    1=Calib ");
		if(data->hvmask<15) {
			printf("Uncalibrated:");
			for(c=0;c<4;c++) {
				if(((data->hvmask)&(1<<c))==0) printf(" %s-%s",lChan[c%2],lFPGA[c/2]);
			}
		}
		printf("\n");
	}
	printf("    HV quality check ");
	if(hvstat<0) {
		printf(BLU " ?? " NRM "                                 Not tested\n");
		failmask|=128;
	}
	else {
		if(hvstat) {
			failmask|=hvstat;
			printf(RED "Fail" NRM "                                ");
			for(c=8;c<12;c++) {
				if(hvstat&(1<<c)) printf(" %s",hvissues[c-8]);
			}
		}
		else printf(GRN "Pass" NRM);
		printf("\n");
	}
	printf("----------------------------------------------------------------------------------------------------\n\n");
	return failmask;
}

int main(int argc, char *argv[]) {
	int c,tmp;
	//Default values
	bool serial=true;
	char device[SLENG]="/dev/ttyUSB0";
	char hname[SLENG]="regboard0";
	int blk=0,fee=0,hv=0;
	
	//Decode options
	while((c=getopt(argc,argv,"hvud:e:b:f:H"))!=-1) {
		switch(c) {
			case 'v':
				verb=1;
				break;
			case 'u':
				serial=false;
				break;
			case 'd':
				if(optarg!=NULL && strlen(optarg)<SLENG) strcpy(device,optarg);
				break;
			case 'e':
				if(optarg!=NULL && strlen(optarg)<SLENG) strcpy(hname,optarg);
				break;
			case 'b':
				if(optarg!=NULL) tmp=atoi(optarg);
				if(tmp>=0 && tmp<3584) blk=tmp;
				break;
			case 'f':
				if(optarg!=NULL) tmp=atoi(optarg);
				if(tmp>=0 && tmp<8) fee=tmp;
				break;
			case 'H': hv=1; break;
			case 'h': default:
				if(c!='h') printf("\033[1;33mfz-test \033[1;39m: Unrecognized option\n");
				printf("\n**********  HELP **********\n");
				printf("    -h        this help\n");
				printf("    -v        enable verbose output\n");
				printf("    -u        use UDP protocol (via RB) [by default direct RS232 is used]\n");
				printf("    -d <dev>  specify the device where FEE is connected [default: /dev/ttyUSB0]\n");
				printf("    -e <dev>  specify the hostname of the RB [default: regboard0]\n");
				printf("    -b <blk>  specify the block ID [0-3584, default: 0]\n");
				printf("    -f <fee>  specify the FEE ID   [0-   7, default: 0]\n");
				printf("    -H        test HV outputs (WARNING: don't touch the card and be sure that no detector is connected!)\n");
				printf("\n");
		}
	}
	//Open device
	FzSC sock(serial,serial?device:hname);
	if(!(sock.SockOK())) return 0;
	
	int ret=0,N,failmask;
	int itmp[8];
	float ftmp[3];
	char query[100];
	uint8_t reply[MLENG];
	FEEData data;
	
	//Get serial number
	if((ret=sock.Send(blk,fee,0xA5,"Q",reply,verb))) goto err;
	N=sscanf((char *)reply,"0|%d",&data.sn);
	if(N!=1) data.sn=-1;
	
	//Get PIC firmware version
	if((ret=sock.Send(blk,fee,0x8B,"",reply,verb))) goto err;
	N=sscanf((char *)reply,"0|%d,%d,%d,V%*d",itmp,itmp+1,itmp+2);
	if(N!=3) strcpy(data.vPIC,"");
	else sprintf(data.vPIC,"%02d/%02d/%04d",itmp[0],itmp[1],itmp[2]);
	
	//Get FPGA firmware version
	for(c=0;c<2;c++) {
		if((ret=sock.Send(blk,fee,0x8C,lFPGA[c],reply,verb))) goto err;
		N=sscanf((char *)reply,"0|tel=%*c,day=%d,month=%d,year=%d,variant=%*d",itmp,itmp+1,itmp+2);
		if(N!=3) strcpy(data.vFPGA[c],"");
		else sprintf(data.vFPGA[c],"%02d/%02d/%04d",itmp[0],itmp[1],itmp[2]);
	}
	
	//Get temperatures
	if((ret=sock.Send(blk,fee,0x83,"",reply,verb))) goto err;
	N=sscanf((char *)reply,"0|%d,%d,%d,%d,%d,%d",data.temp,data.temp+1,data.temp+2,data.temp+3,data.temp+4,data.temp+5);
	if(N<6) {
		for(c=0;c<6;c++) data.temp[c]=-1;
	}
	
	//Get Low Voltages
	data.v4=-1;
	for(c=0;c<19;c++) data.lv[c]=-1;
	if((ret=sock.Send(blk,fee,0x9B,"",reply,verb))) goto err;
	for(c=0;reply[c];c++) if(reply[c]==0x2c) reply[c]=0x2e;
	N=sscanf((char *)reply,"0|v:%f %f %f",ftmp,ftmp+1,ftmp+2);
	if(N==3) for(c=0;c<3;c++) data.lv[c]=(int)(ftmp[c]*1000+0.5);
	if((ret=sock.Send(blk,fee,0x9C,"",reply,verb))) goto err;
	N=sscanf((char *)reply,"0|%d,%d,%d,%d,%d,%d,%d,%d",itmp,itmp+1,itmp+2,itmp+3,itmp+4,itmp+5,itmp+6,itmp+7);
	if(N==8) {
		data.v4=1;
		data.lv[4] =itmp[0];
		data.lv[9] =itmp[1];
		data.lv[6] =itmp[2];
		data.lv[11]=itmp[3];
		data.lv[3] =itmp[4];
		data.lv[8] =itmp[5];
		data.lv[5] =itmp[6];
		data.lv[10]=itmp[7];
	}
	if(N==4) {
		data.v4=0;
		data.lv[9] =itmp[0];
		data.lv[11]=itmp[1];
		data.lv[8] =itmp[2];
		data.lv[10]=itmp[3];
		data.lv[3]=data.lv[4]=data.lv[5]=data.lv[6]=-2;
	}
	if((ret=sock.Send(blk,fee,0x9D,"",reply,verb))) goto err;
	N=sscanf((char *)reply,"0|%d,%d,%d,%d,%d,%d,%d,%d",itmp,itmp+1,itmp+2,itmp+3,itmp+4,itmp+5,itmp+6,itmp+7);
	if(N==8) {
		data.lv[15]=itmp[0];
		data.lv[16]=itmp[1];
		data.lv[14]=itmp[2];
		data.lv[13]=itmp[3];
		data.lv[18]=itmp[4];
		data.lv[17]=itmp[5];
		data.lv[12]=itmp[6];
		data.lv[7] =itmp[7];
	}
	
	//Test pre-amp
	data.gomask=0;
	for(c=0;c<6;c++) {
		sprintf(query,"%s,%d",lFPGA[c/3],(c%3)+1);
		if((ret=sock.Send(blk,fee,0x98,query,reply,verb))) goto err;
		ret=sscanf((char *)reply,"0|%d",&N);
		if(ret!=1) {
			data.gomask=-1;
			break;
		}
		else if(N) data.gomask|=(1<<c);
	}
	
	//Test baseline offset
	for(c=0;c<12;c++) {
		sprintf(query,"%s,0x%d",lFPGA[c/6],1036+100*(c%6));
		if((ret=sock.Send(blk,fee,0x85,query,reply,verb))) goto err;
		N=sscanf((char *)reply,"0|%d",data.bl+c);
		if(N==1) data.bl[c]=(data.bl[c]<32768)?(data.bl[c]):(data.bl[c]-65536);
		else data.bl[c]=-10000;
	}
	
	//HV calibration status
	if((ret=sock.Send(blk,fee,0x94,"",reply,verb))) goto err;
	ret=strlen((char *)reply);
	data.hvmask=-1;
	if((ret>=3)&&(reply[2]>=0x30)&&(reply[2]<=0x39)) {
		N=reply[2]-0x30;
		if((N<=4)&&(ret==3+3*N)) {
			data.hvmask=0;
			for(c=0;c<N;c++) {
				if(reply[4+3*c]==0x41 && reply[5+3*c]==0x31) data.hvmask|=1;
				else if(reply[4+3*c]==0x41 && reply[5+3*c]==0x32) data.hvmask|=2;
				else if(reply[4+3*c]==0x42 && reply[5+3*c]==0x31) data.hvmask|=4;
				else if(reply[4+3*c]==0x42 && reply[5+3*c]==0x32) data.hvmask|=8;
				else {
					data.hvmask=-1; break;
				}
			}
		}
	}
	// 	if(hv) {
	// 		if(data.hvmask>0) ret=hvtest(fd,blk,fee,data.hvmask);
	// 		else ret=-1;
	// 	}
	// 	else ret=-1;
	
	//failmask=report(&data,ret);
	failmask=report(&data,-1);
	
	if(failmask&1) {
		printf("Card temperature is high. Please check cooling and restart the test...\n\n");
		return 0;
	}
	if(failmask) {
		printf("Some issues have been detected. What do you want to do?\n");
		printf("    1) Guided resolution\n");
		printf("    2) Manual resolution (operation menu)\n");
		printf("    0) Quit\n\n");
		printf("> "); scanf("%d",&ret); getchar();
		if((ret<0)||(ret>2)) ret=0;
	}
	else {
		printf("The FEE is fully functional! What do you want to do?\n");
		printf("    1) Operation menu\n");
		printf("    0) Quit\n\n");
		printf("> "); scanf("%d",&ret); getchar();
		if(ret==1) ret=2;
		else ret=0;
	}
	printf("\n");
	if(ret==0) return 0;
	if(ret==1) {
		if(failmask&4) {
			printf("Card firmwares are obsolete. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, reload new fw and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N!=1) N=0;
			if(N==0) return 0;
		}
		if(failmask&2) {
			printf("Card SN is not set. What do you want to do?\n");
			printf("    pos) Write new SN and press enter\n");
			printf("    neg) Ignore and go on\n");
			printf("     0 ) Quit the procedure\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N==0) return 0;
			if(N>0) {
				if(N>=65535) {
					printf(YEL"  main  " NRM ": invalid value (0<SN<65535); SN not set\n");
				}
				else {
					sprintf(query,"%d",N);
					if((ret=sock.Send(blk,fee,0xA5,query,reply,verb))) goto err;
					if(strcmp((char *)reply,"0|")) printf(YEL"  main  " NRM ": something went wrong, SN not set\n");
					else data.sn=N;
				}
			}
		}
		if(failmask&8) {
			printf("Low voltages are out of range. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N!=1) N=0;
			if(N==0) return 0;
		}
		if(failmask&16) {
			printf("Some pre-amps are broken. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N!=1) N=0;
			if(N==0) return 0;
		}
		if(failmask&32) {
			printf("Some offsets are out of range. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    2) Try to auto-calibrate\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
			//gestire N==2
		}
		if(failmask&64) {
			printf("Some HV channels are not calibrated. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    2) Perform HV calibration procedure\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
			//gestire N==2
		}
	}
	else {
		do {
			printf("\nOperation menu\n");
			printf("    1) Set new serial number\n");
			printf("    2) Offset auto-calibration\n");
			printf("    3) Plot offset calibration curve\n");
			printf("    4) HV calibration\n");
			printf("    5) Import EEPROM data\n");
			printf("    6) Export EEPROM data\n");
			printf("    0) Quit\n\n");
			printf("> "); scanf("%d",&tmp); getchar();
			if((tmp>6)||(tmp<0)) tmp=0;
			switch(tmp) {
				case 1:
					printf("New SN> "); scanf("%d",&N); getchar();
					if((N<=0)||(N>=65535)) {
						printf(YEL"  main  " NRM ": invalid value (0<SN<65535); SN not set\n");
					}
					else {
						sprintf(query,"%d",N);
						if((ret=sock.Send(blk,fee,0xA5,query,reply,verb))) goto err;
						if(strcmp((char *)reply,"0|")) printf(YEL"  main  " NRM ": something went wrong, SN not set\n");
						else data.sn=N;
					}
					break;
				case 3:
					if((ret=offcurve(&sock,blk,fee))) goto err;
			}
		}
		while(tmp);
	}
	return 0;
	
	err:
	switch(ret) {
		case -1: printf("\033[1;31m  main  \033[0;39m: many timeouts occurred, check FEE geo number or RS232 connection\n"); break;
		case -2: case -3: case -4: case -5:
			printf("\033[1;31m  main  \033[0;39m: FEE is connected but many errors occurred, check RS232 connection\n"); break;
		case -10: printf("\033[1;31m  main  \033[0;39m: device is not ready (check the software)\n"); break;
		default: printf("\033[1;31m  main  \033[0;39m: unhandled error...\n");
	}
	return -1;
}

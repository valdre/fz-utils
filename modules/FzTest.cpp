#include "FzTest.h"

FzTest::FzTest(FzSC *sck, const int Blk, const int Fee, const bool verbose) {
	sock=sck; ksock=nullptr;
	blk=Blk; fee=Fee;
	fVerb=verbose;
	
	Init();
}

FzTest::~FzTest() {
	
}

void FzTest::KeithleySetup(FzSC *sck) {
	ksock=sck;
	if(sck==nullptr) return;
	
	char reply[MLENG];
	//test
	ksock->KSend("*IDN?",reply,true);
	
	return;
}

void FzTest::Init() {
	int c;
	
	fTested=false;
	v4=-1; sn=-1;
	for(c=0;c<6;c++) temp[c]=-1;
	strcpy(vPIC,"");
	for(c=0;c<2;c++) strcpy(vFPGA[c],"");
	for(c=0;c<19;c++) lv[c]=-1;
	gomask=0;
	for(c=0;c<12;c++) {bl[c]=-10000; blvar[c]=-10000; dcreact[c]=1000;}
	hvmask=-1;
	for(c=0;c<5;c++) hvstat[c]=-1;
	failmask=0;
	return;
}

int FzTest::FastTest(bool hv) {
	int c,N,ret;
	char query[SLENG];
	uint8_t reply[MLENG];
	int itmp[8];
	float ftmp[3];
	
	if(!fVerb) printf(BLD "FastTest" NRM " initializing parameters\n");
	Init();
	if(!fVerb) printf(UP GRN "FastTest" NRM " initializing parameters\n");
	
	//Get serial number
	if(!fVerb) printf(BLD "FastTest" NRM " getting serial number\n");
	if((ret=sock->Send(blk,fee,0xA5,"Q",reply,fVerb))) return ret;
	N=sscanf((char *)reply,"0|%d",&sn);
	if(N!=1) return -20;
	if(!fVerb) printf(UP GRN "FastTest" NRM " getting serial number\n");
	
	//Get PIC firmware version
	if(!fVerb) printf(BLD "FastTest" NRM " getting firmware versions\n");
	if((ret=sock->Send(blk,fee,0x8B,"",reply,fVerb))) return ret;
	N=sscanf((char *)reply,"0|%d,%d,%d,V%*d",itmp,itmp+1,itmp+2);
	if(N!=3) return -20;
	sprintf(vPIC,"%02d/%02d/%04d",itmp[0],itmp[1],itmp[2]);
	//Get FPGA firmware version
	for(c=0;c<2;c++) {
		if((ret=sock->Send(blk,fee,0x8C,lFPGA[c],reply,fVerb))) return ret;
		N=sscanf((char *)reply,"0|tel=%*c,day=%d,month=%d,year=%d,variant=%*d",itmp,itmp+1,itmp+2);
		if(N!=3) return -20;
		else sprintf(vFPGA[c],"%02d/%02d/%04d",itmp[0],itmp[1],itmp[2]);
	}
	if(!fVerb) printf(UP GRN "FastTest" NRM " getting firmware versions\n");
	
	//Get temperatures
	if(!fVerb) printf(BLD "FastTest" NRM " getting temperatures\n");
	if((ret=sock->Send(blk,fee,0x83,"",reply,fVerb))) return ret;
	N=sscanf((char *)reply,"0|%d,%d,%d,%d,%d,%d",temp,temp+1,temp+2,temp+3,temp+4,temp+5);
	if(N<6) return -20;
	if(!fVerb) printf(UP GRN "FastTest" NRM " getting temperatures\n");
	
	//Get Low Voltages
	if(!fVerb) printf(BLD "FastTest" NRM " getting low voltages\n");
	if((ret=sock->Send(blk,fee,0x9B,"",reply,fVerb))) return ret;
	for(c=0;reply[c];c++) if(reply[c]==0x2c) reply[c]=0x2e; // ',' -> '.' (sscanf doesn't accept ',' as decimal separator)
	N=sscanf((char *)reply,"0|v:%f %f %f",ftmp,ftmp+1,ftmp+2);
	if(N!=3) return -20;
	for(c=0;c<3;c++) lv[c]=(int)(ftmp[c]*1000+0.5);
	if((ret=sock->Send(blk,fee,0x9C,"",reply,fVerb))) return ret;
	N=sscanf((char *)reply,"0|%d,%d,%d,%d,%d,%d,%d,%d",itmp,itmp+1,itmp+2,itmp+3,itmp+4,itmp+5,itmp+6,itmp+7);
	if(N==8) {
		v4=1;
		lv[4] =itmp[0]; lv[9] =itmp[1]; lv[6] =itmp[2]; lv[11]=itmp[3];
		lv[3] =itmp[4]; lv[8] =itmp[5]; lv[5] =itmp[6]; lv[10]=itmp[7];
	}
	else if(N==4) {
		v4=0;
		lv[9] =itmp[0]; lv[11]=itmp[1]; lv[8] =itmp[2]; lv[10]=itmp[3];
		lv[3]=lv[4]=lv[5]=lv[6]=-2;
	}
	else return -20;
	if((ret=sock->Send(blk,fee,0x9D,"",reply,fVerb))) return ret;
	N=sscanf((char *)reply,"0|%d,%d,%d,%d,%d,%d,%d,%d",itmp,itmp+1,itmp+2,itmp+3,itmp+4,itmp+5,itmp+6,itmp+7);
	if(N!=8) return -20;
	lv[15]=itmp[0]; lv[16]=itmp[1]; lv[14]=itmp[2]; lv[13]=itmp[3];
	lv[18]=itmp[4]; lv[17]=itmp[5]; lv[12]=itmp[6]; lv[7] =itmp[7];
	if(!fVerb) printf(UP GRN "FastTest" NRM " getting low voltages\n");
	
	//Test pre-amp
	if(!fVerb) printf(BLD "FastTest" NRM " testing pre-amplifiers (Go/NOGo)\n");
	for(c=0;c<6;c++) {
		sprintf(query,"%s,%d",lFPGA[c/3],(c%3)+1);
		if((ret=sock->Send(blk,fee,0x98,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&N);
		if(ret!=1) return -20;
		if(N) gomask|=(1<<c);
	}
	if(!fVerb) printf(UP GRN "FastTest" NRM " testing pre-amplifiers (Go/NOGo)\n");
	
	//Test baseline offset
	if(!fVerb) printf(BLD "FastTest" NRM " testing baseline offsets\n");
	for(c=0;c<12;c++) {
		if((ret=OffCheck(c))<0) return ret;
	}
	if(!fVerb) printf(UP GRN "FastTest" NRM " testing baseline offsets\n");
	
	//HV calibration status
	if(!fVerb) printf(BLD "FastTest" NRM " checking HV calibration status\n");
	if((ret=sock->Send(blk,fee,0x94,"",reply,fVerb))) return ret;
	ret=strlen((char *)reply);
	if((ret>=3)&&(reply[2]>=0x30)&&(reply[2]<=0x39)) {
		N=reply[2]-0x30;
		if((N<=4)&&(ret==3+3*N)) {
			hvmask=0;
			for(c=0;c<N;c++) {
				if(reply[4+3*c]==0x41 && reply[5+3*c]==0x31) hvmask|=1;
				else if(reply[4+3*c]==0x41 && reply[5+3*c]==0x32) hvmask|=2;
				else if(reply[4+3*c]==0x42 && reply[5+3*c]==0x31) hvmask|=4;
				else if(reply[4+3*c]==0x42 && reply[5+3*c]==0x32) hvmask|=8;
				else {
					hvmask=-1; break;
				}
			}
		}
	}
	if(!fVerb) printf(UP GRN "FastTest" NRM " checking HV calibration status\n");
	
	//HV test (no calibration)
	if(hv) {
		if(!fVerb) printf(BLD "FastTest" NRM " testing HV quality\n");
		if((ret=HVtest())<0) return ret;
	}
	
	fTested=true;
	return 0;
}

void FzTest::Report() {
	int c,status;
	float ref;
	
	if(!fTested) {
		printf(YEL "Report  " NRM "fast test was not performed\n");
		return;
	}
	failmask=0;
	
	printf("\n\n");
	printf(BLD "                         **************************************************\n");
	printf("                         *                  TEST  REPORT                  *\n");
	printf("                         **************************************************\n\n" NRM);
	printf(BLD "      Parameter      Res.         Value         Reference  Note\n" NRM);
	printf("----------------------------------------------------------------------------------------------------\n");
	
	//Card type and serial number
	printf(BLD "Card Type and SN     " NRM);
	if((v4<0)||(sn<0)) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
	else {
		if(sn==65535) {
			failmask|=2;
			printf(RED "Fail" NRM "                 %4s            No SN data\n",v4?"v4/5":"  v3");
		}
		else printf(GRN "Pass" NRM "             %4s-%03d\n",v4?"v4/5":"  v3",sn);
	}
	
	//PIC and FPGA version
	printf(BLD "\nFirmware:\n" NRM);
	printf("         PIC Version ");
	if(vPIC[0]=='\0') printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
	else {
		if(strcmp(vPIC,v4?LASTVPICV4:LASTVPICV3)) {
			failmask|=4;
			printf(YEL"Warn" NRM " %20s %10s Obsolete firmware\n",vPIC,v4?LASTVPICV4:LASTVPICV3);
		}
		printf(GRN "Pass" NRM " %20s %10s\n",vPIC,v4?LASTVPICV4:LASTVPICV3);
	}
	for(c=0;c<2;c++) {
		printf("      FPGA %s Version ",lFPGA[c]);
		if(vFPGA[c][0]=='\0') printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
		else {
			if(strcmp(vFPGA[c],LASTVFPGA)) {
				failmask|=4;
				printf(YEL"Warn" NRM " %20s %10s Obsolete firmware\n",vFPGA[c],LASTVFPGA);
			}
			else printf(GRN "Pass" NRM " %20s %10s\n",vFPGA[c],LASTVFPGA);
		}
	}
	
	//Temperatures
	status=0;
	for(c=0;c<6;c++) {
		if(temp[c]<0) {status=-1; break;}
		if(temp[c]>=70) status|=2;
		else if(temp[c]>=50) status|=1;
	}
	printf(BLD "\nTemperatures (in °C) " NRM);
	if(status<0) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
	else {
		if(status>=1) printf(YEL"Warn" NRM "    ");
		else printf(GRN "Pass" NRM "    ");
		for(c=0;c<6;c++) {
			if(c>0) printf(",");
			printf("%2d",temp[c]);
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
		if(lv[c]==-2) continue;
		printf("%s ",lvlabel[c]);
		if(lv[c]<0) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
		else {
			if(fabs(vref[c]-(double)(lv[c]))/vref[c]>=0.02) {
				failmask|=8;
				printf(RED "Fail" NRM);
			}
			else printf(GRN "Pass" NRM);
			printf(" %20d %10.0f %s\n",lv[c],vref[c],lvnotes[c]);
		}
	}
	
	// Go/NoGo
	printf(BLD "\nPre-amplifiers       " NRM);
	if(gomask<0) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
	else {
		if(gomask==63) printf(GRN "Pass" NRM "         ");
		else {
			failmask|=16;
			printf(RED "Fail" NRM "         ");
		}
		for(c=0;c<6;c++) printf(" %1d",((gomask)&(1<<c))>>c);
		printf("       1=OK ");
		if(gomask<63) {
			printf("Broken:");
			for(c=0;c<6;c++) {
				if(((gomask)&(1<<c))==0) printf(" %s-%s",lChan[c%3],lFPGA[c/3]);
			}
		}
		printf("\n");
	}
	
	//Baseline offsets
	printf(BLD "\nBaseline offsets (in ADC units):\n" NRM);
	for(c=0;c<12;c++) {
		printf("               %s-%s ",lADC[c%6],lFPGA[c/6]);
		if(bl[c]<-8500) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
		else {
			ref=(v4)?blref5[c%6]:blref3[c%6];
			if((fabs((ref-(double)(bl[c]))/ref)>=bltoll[c%6])||(blvar[c]>=blvtol[c%6])||(dcreact[c]<1000)) {
				failmask|=32;
				printf(RED "Fail" NRM);
			}
			else printf(GRN "Pass" NRM);
			printf(" % 20d % 10.0f",bl[c],ref);
			if(blvar[c]>=blvtol[c%6]) {
				printf(" Unstable DC level");
				if(!dcreact[c]) printf(" -");
			}
			if(dcreact[c]<1000) printf(" Non responsive");
			printf("\n");
		}
	}
	
	//HV calibration status
	printf(BLD "\nHigh Voltage:\n" NRM);
	printf("  Calibration status ");
	if(hvmask<0) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
	else {
		if(hvmask==15) printf(GRN "Pass" NRM "             ");
		else {
			failmask|=64;
			printf(RED "Fail" NRM "             ");
		}
		for(c=0;c<4;c++) printf(" %1d",((hvmask)&(1<<c))>>c);
		printf("    1=Calib ");
		if(hvmask<15) {
			printf("Uncalibrated:");
			for(c=0;c<4;c++) {
				if(((hvmask)&(1<<c))==0) printf(" %s-%s",lChan[c%2],lFPGA[c/2]);
			}
		}
		printf("\n");
	}
	for(c=0;c<5;c++) {
		if(c==2) continue;
		printf(" %3s-%1s quality check ",lChan[c%3],lFPGA[c/3]);
		if(hvstat[c]<0) {
			printf(BLU " ?? " NRM "                                 Not tested\n");
			failmask|=128;
		}
		else {
			if(hvstat[c]) {
				failmask|=hvstat[c];
				printf(RED "Fail" NRM "                              ");
				for(int i=8;i<12;i++) {
					if(hvstat[c]&(1<<i)) printf("   %s",hvissues[i-8]);
				}
			}
			else printf(GRN "Pass" NRM);
			printf("\n");
		}
	}
	printf("----------------------------------------------------------------------------------------------------\n\n");
	return;
}

int FzTest::Guided() {
	int ret,N;
	
	if(!fTested) {
		printf(YEL "Guided  " NRM "fast test was not performed\n");
		return 0;
	}
	
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
				if((ret=SetSN(N))<0) return ret;
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
			printf("    2) Try to auto-calibrate DC levels\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
			if(N==2) {
				if((ret=OffCal())<0) return ret;
			}
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
		if(failmask&256) {
			printf("Measured voltage is inconsistent with applied voltage. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    2) Perform HV calibration procedure (ADC only)\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
			//gestire N==2
		}
		if(failmask&512) {
			printf("Measured current is not 0 without load. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    2) Perform HV calibration procedure (ADC only)\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
			//gestire N==2
		}
		if(failmask&1024) {
			printf("Measured current is inconsistent with Ohm's law. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
		}
		if(failmask&2048) {
			printf("HV is unstable and maximum voltage cannot be reached. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
		}
	}
	else Manual();
	return 0;
}

int FzTest::Manual() {
	int tmp,ret,N;
	
	if(failmask&1) {
		printf("Card temperature is high. Please check cooling and restart the test...\n\n");
		return 0;
	}
	do {
		printf("\nOperation menu\n");
		printf("    1) Set new serial number\n");
		printf("    2) Offset auto-calibration\n");
		printf("    3) Plot offset calibration curve\n");
		printf("    4) HV calibration\n");
		printf("    5) Import EEPROM data\n");
		printf("    6) Export EEPROM data\n");
		printf("    8) Repeat the fast test\n");
		printf("    9) Repeat the fast test with HV check\n");
		printf("    0) Quit\n\n");
		printf("> "); scanf("%d",&tmp); getchar();
		if((tmp>9)||(tmp<0)) tmp=0;
		switch(tmp) {
			case 1:
				printf("New SN> "); scanf("%d",&N); getchar();
				if((ret=SetSN(N))<0) return ret;
				break;
			case 2:
				if((ret=OffCal())<0) return ret;
				break;
			case 3:
				if((ret=OffCurve())) return ret;
				break;
			case 8: case 9:
				if((ret=FastTest((tmp==8)?false:true))<0) return ret;
				Report();
		}
	}
	while(tmp);
	return 0;
}

int FzTest::SetSN(const int SN) {
	int ret;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	if(SN>=65535) {
		printf(YEL "SetSN   " NRM " invalid value (0<SN<65535); SN not set\n");
	}
	else {
		sprintf(query,"%d",SN);
		if((ret=sock->Send(blk,fee,0xA5,query,reply,fVerb))) return ret;
		if(strcmp((char *)reply,"0|")) printf(YEL "SetSN   " NRM " something went wrong, SN not set\n");
		else sn=SN;
	}
	return 0;
}

//ch goes from 0 to 11 (QH1,I1,QL1,Q2,I2,Q3 x2)
//c  goes from 0 to 5  (QH1,Q2,Q3 x2)

int FzTest::BLmeas(const int ch,const int tries,int *Bl,int *Blvar) {
	int ret=0,N=0,tmp,min=9999,max=-9999;
	double av=0;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	sprintf(query,"%s,0x%d",lFPGA[ch/6],1036+100*(ch%6));
	for(int i=0;i<tries;i++) {
		usleep(1000);
		if((ret=sock->Send(blk,fee,0x85,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&tmp);
		if(ret==1) {
			tmp=(tmp<32768)?tmp:(tmp-65536);
			av+=(double)tmp;
			N++;
			if(tmp<min) min=tmp;
			if(tmp>max) max=tmp;
		}
	}
	if(N==tries) {
		*Bl=(int)(0.5+av/((double)N));
		if(Blvar!=nullptr) *Blvar=max-min;
		return 0;
	}
	return -20;
}

int FzTest::OffCheck(const int ch) {
	int c,ret=0;
	int old,min,max;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	if((ret=BLmeas(ch,10,bl+ch,blvar+ch))<0) return ret;
	if(ch==1 || ch==2 || ch==4 || ch==7 || ch==8 || ch==10) return 0;
	
	c=ch2c[ch];
	//Store present DAC value
	sprintf(query,"%s,%s",lFPGA[c/3],regDAC[c%3]);
	if((ret=sock->Send(blk,fee,0x85,query,reply,fVerb))) return ret;
	ret=sscanf((char *)reply,"0|%d",&old);
	if(ret!=1) return -20;
	//Set DAC to 200 and test BL
	sprintf(query,"%s,%d,%d",lFPGA[c/3],(c%3)+1,200);
	if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
	usleep(1000);
	if((ret=BLmeas(ch,5,&max,nullptr))<0) return ret;
	//Set DAC to 400 and test BL
	sprintf(query,"%s,%d,%d",lFPGA[c/3],(c%3)+1,400);
	if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
	usleep(1000);
	if((ret=BLmeas(ch,5,&min,nullptr))<0) return ret;
	//Set DAC to previous value
	sprintf(query,"%s,%d,%d",lFPGA[c/3],(c%3)+1,old);
	if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
	dcreact[ch]=max-min;
	return 0;
}

int FzTest::OffCal() {
	int target,c,i,ch,ret,ierr,dac1,dac2,bl1,bl2,dac,Bl,tmp;
	char ctmp,query[SLENG];
	uint8_t reply[MLENG];
	int finaldac[6];
	
	if(!fTested) printf(YEL "OffCal  " NRM " fast test was not performed. Performing offset check only...\n");
	
	printf("Target baseline [-7400]>");
	for(c=0;c<SLENG-1;c++) {
		ctmp=getchar();
		if(ctmp=='\n') break;
		query[c]=ctmp;
	}
	query[c]='\0';
	if(c==0) target=-7400;
	else {
		target=atoi(query);
		if(target<-8192 || target>8191) {
			printf(YEL "OffCal  " NRM " invalid target. Set to -7400.\n");
			target=-7400;
		}
	}
	for(c=0;c<6;c++) {
		ch=c2ch[c];
		if(!fTested) {
			if((ret=OffCheck(ch))<0) return ret;
		}
		//Reading original DAC value
		sprintf(query,"%s,%s",lFPGA[c/3],regDAC[c%3]);
		if((ret=sock->Send(blk,fee,0x85,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&dac);
		if(ret!=1) return -20;
		
		finaldac[c]=dac;
		if(bl[ch]>=8100 || bl[ch]<=-8100) {
			printf(YEL "OffCal  " NRM " %s-%s: saturating baseline\n",lADC[ch%6],lFPGA[ch/6]);
			continue;
		}
		if(blvar[ch]>blvtol[ch%6]) {
			printf(YEL "OffCal  " NRM " %s-%s: unstable DC level\n",lADC[ch%6],lFPGA[ch/6]);
			continue;
		}
		if(dcreact[ch]<1000) {
			printf(YEL "OffCal  " NRM " %s-%s: DC level is not responsive\n",lADC[ch%6],lFPGA[ch/6]);
			continue;
		}
		//Usually DAC should be around 700 but I prefer to start from lower values to avoid BL underflow
		dac1=200; dac2=400;
		//Try DAC=200 and check bl
		sprintf(query,"%s,%d,%d",lFPGA[c/3],(c%3)+1,dac1);
		if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
		usleep(1000);
		if((ret=BLmeas(ch,5,&bl1,nullptr))<0) return ret;
		//Try DAC=400 and check bl
		sprintf(query,"%s,%d,%d",lFPGA[c/3],(c%3)+1,dac2);
		if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
		usleep(1000);
		if((ret=BLmeas(ch,5,&bl2,nullptr))<0) return ret;
		ierr=0; Bl=10000;
		for(i=0;;i++) {
			if(bl1<=bl2) {
				if(dac1<=dac2) ierr=1;
				else {
					dac=dac1;  tmp=bl1;
					dac1=dac2; bl1=bl2;
					dac2=dac;  bl2=tmp;
				}
			}
			dac=dac1+(int)(0.5+((double)(target-bl1))*((double)(dac2-dac1))/((double)(bl2-bl1)));
			if((abs(dac2-dac1)<10) || (abs(Bl-target)<20)) break;
			if((i>=5) || ierr) break;
			
			sprintf(query,"%s,%d,%d",lFPGA[c/3],(c%3)+1,dac);
			if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
			usleep(1000);
			if((ret=BLmeas(ch,5,&Bl,nullptr))<0) return ret;
			
			if(target<bl2) {bl1=Bl; dac1=dac;}
			else if(target<bl1) {
				if(Bl>=target) {bl2=Bl; dac2=dac;}
				else {bl1=Bl; dac1=dac;}
			}
			else {bl2=Bl; dac2=dac;}
		}
		if((i>=5) || ierr) {
			printf(YEL "OffCal  " NRM " %s-%s: DC level reaction is anomalous (i=%d bl1=%d bl2=%d dac1=%d dac2=%d)\n",lADC[ch%6],lFPGA[ch/6],i,bl1,bl2,dac1,dac2);
			continue;
		}
		finaldac[c]=dac;
	}
	
	for(c=0;c<6;c++) {
		ch=c2ch[c];
		sprintf(query,"%s,%d,%d",lFPGA[c/3],(c%3)+1,finaldac[c]);
		if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
		usleep(1000);
		if((ret=BLmeas(ch,5,&Bl,nullptr))<0) return ret;
		printf(GRN "OffCal  " NRM " %s-%s: DAC set to %4d. BL -> % 5d\n",lADC[ch%6],lFPGA[ch/6],finaldac[c],(Bl<32768)?Bl:(Bl-65536));
	}
	//Writing DAC values to PIC EEPROM
	if((ret=sock->Send(blk,fee,0x8E,"",reply,fVerb))) return ret;
	return 0;
}

int FzTest::OffCurve() {
	FILE *fout;
	int dac,mes,c,ret;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	fout=fopen("output.txt","w");
	if(fout==NULL) return 0;
	
	if(!fVerb) {
		printf(BLD " DAC  QH1-A  Q2-A  Q3-A QH1-B  Q2-B  Q3-B\n" NRM);
		printf("------------------------------------------\n");
	}
	for(dac=0;dac<1024;dac+=10) {
		for(c=0;c<6;c++) {
			sprintf(query,"%s,%d,%d",lFPGA[c/3],c%3+1,dac);
			if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) goto err;
		}
		fprintf(fout,"%4d",dac);
		if(!fVerb) printf(BLD "%4d" NRM,dac);
		for(c=0;c<6;c++) {
			if((ret=BLmeas(c2ch[c],3,&mes,nullptr))<0) goto err;
			fprintf(fout," % 6d",mes);
			if(!fVerb) printf(" % 6d",mes);
		}
		fprintf(fout,"\n");
		if(!fVerb) printf("\n");
	}
	err:
	fclose(fout);
	return ret;
}

int FzTest::WaitHVready(int c) {
	int i,ret;
	char query[SLENG],Vy[SLENG];
	uint8_t reply[MLENG];
	
	//60s timeout
	for(i=0;i<60;i++) {
		sprintf(query,"%s,%d",lFPGA[c/3],c%3+1);
		if((ret=sock->Send(blk,fee,0x88,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%*d,%s",Vy);
		if(ret!=1) return -20;
		if(Vy[0]!='N' && Vy[1]!='C') break;
		sleep(1);
	}
	if(i>=60) return -20;
	return 0;
}

int FzTest::IVmeas(int c,int *V,int *I,bool wait) {
	int ret,Nch=0,Nstab=0,Vtmp,Itmp,old=-1,dir=0,oldir=0;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	if(!fVerb) printf("\n");
	sleep(3);
	//60s timeout for current measurement
	for(int i=0;i<30;i++) {
		sleep(2);
		sprintf(query,"%s,%d",lFPGA[c/3],c%3+1);
		if((ret=sock->Send(blk,fee,0x87,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&Itmp);
		if(ret!=1) return -20;
		if(!wait) break;
		if(!fVerb) printf(UP BLD "IVmeas  " Mag " %s-%s" NRM " waiting for current stabilization: " BLD "%5d\n" NRM,lChan[c%3],lFPGA[c/3],Itmp);
		if(old>=0) {
			dir=((Itmp==old)?dir:((Itmp>old)?1:-1));
			if(oldir!=0 && oldir!=dir) Nch++;
			if(abs(Itmp-old)<=5) Nstab++;
			else Nstab=0;
			//exit after 3 changes of direction or 3 stable readings in a row
			if(Nch==3 || Nstab==2) break;
		}
		oldir=dir;
		old=Itmp;
	}
	
	//voltage measurement
	sprintf(query,"%s,%d",lFPGA[c/3],c%3+1);
	if((ret=sock->Send(blk,fee,0x88,query,reply,fVerb))) return ret;
	ret=sscanf((char *)reply,"0|%d,%*d",&Vtmp);
	if(ret!=1) return -20;
	
	if(!fVerb) printf(UP GRN "IVmeas  " Mag " %s-%s" NRM " V = %3d V    I =%5d nA                     \n",lChan[c%3],lFPGA[c/3],-Vtmp,Itmp);
	if(V!=nullptr) *V=-Vtmp;
	if(I!=nullptr) *I=Itmp;
	
	return 0;
}

int FzTest::ApplyHV(int c,int V) {
	int i,ret,tmp;
	char query[SLENG],Vy[SLENG];
	uint8_t reply[MLENG];
	
	if((ret=WaitHVready(c))<0) return ret;
	sprintf(query,"%s,%d,%d,%d",lFPGA[c/3],c%3+1,V,HVRAMP);
	if((ret=sock->Send(blk,fee,0x86,query,reply,fVerb))) return ret;
	
 	if(!fVerb) printf("\n");
	for(i=0;i<50;i++) {
		sleep(1);
		sprintf(query,"%s,%d",lFPGA[c/3],c%3+1);
		if((ret=sock->Send(blk,fee,0x88,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d,%s",&tmp,Vy);
		if(ret!=2) return -20;
		tmp=-tmp;
		if(!fVerb) printf(UP BLD "ApplyHV " Mag " %s-%s" NRM " ramping [%3d/%3d]\n",lChan[c%3],lFPGA[c/3],tmp,V);
		if(Vy[0]!='N' && Vy[1]!='C') break;
	}
	if(i==50) return -20;
	if(!fVerb) printf(UP GRN "ApplyHV " Mag " %s-%s" NRM " reached target voltage: %d V\n",lChan[c%3],lFPGA[c/3],V);
	return 0;
}

int FzTest::HVtest() {
	int c,ret,max,V,I;
	char ctmp,query[SLENG];
	double R;
	uint8_t reply[MLENG];
	const int bit[5]={1,2,2,4,8};
	const int maxv3[5]={200,350,200,200,350};
	const int maxv4[5]={300,400,300,300,400};
	
	for(c=0;c<5;c++) {
		if(c==2) continue; //Skip CsI channels
		hvstat[c]=0;
		if((hvmask&(bit[c]))==0) {
			printf(YEL "HVtest  " Mag " %s-%s" NRM " not calibrated\n",lChan[c%3],lFPGA[c/3]);
			continue;
		}
		printf(MAG "HVtest  " Mag " %s-%s" NRM " press enter when ready to apply HV...",lChan[c%3],lFPGA[c/3]);
		for(;getchar()!='\n';);
		//Set max voltage
		max=v4?maxv4[c]:maxv3[c];
		sprintf(query,"%s,%d,%d",lFPGA[c/3],c%3+1,max);
		if((ret=sock->Send(blk,fee,0x92,query,reply,fVerb))) goto err;
		//Apply 20V to start (if HV is unstable it is unsafe to go further)
		if((ret=ApplyHV(c,20))<0) goto err;
		//Check current and voltage
		if((ret=IVmeas(c,&V,&I,true))<0) goto err;
		if(I>100) hvstat[c]|=512;
		if(abs(V-20)>1) {
			if((ret=ApplyHV(c,0))<0) goto err;
			hvstat[c]|=256;
			continue;
		}
		
		//If ok, apply max voltage
		if((ret=ApplyHV(c,max))<0) goto err;
		//Check current and voltage
		if((ret=IVmeas(c,&V,&I,true))<0) goto err;
		if((ret=ApplyHV(c,0))<0) goto err;
		if(I>100) hvstat[c]|=512;
		if(abs(V-max)>1) {
			hvstat[c]|=2048;
			continue;
		}
		
		//Test current
		printf(MAG "HVtest  " Mag " %s-%s" NRM " plug load in the molex and write the value in MOhm [100]>",lChan[c%3],lFPGA[c/3]);
		for(ret=0;ret<SLENG-1;ret++) {
			ctmp=getchar();
			if(ctmp=='\n') break;
			query[ret]=ctmp;
		}
		query[ret]='\0';
		if(ret==0) R=100.;
		else {
			R=atof(query);
			if(R<0 || R>200) {
				printf(YEL "OffCal  " NRM " invalid target. Skipping current check.\n");
				continue;
			}
		}
		if((ret=ApplyHV(c,(int)(R+10.5)))<0) goto err;
		//Check current
		if((ret=IVmeas(c,nullptr,&I,true))<0) goto err;
		if(abs(I-1000)>100) hvstat[c]|=1024;
		if((ret=ApplyHV(c,0))<0) goto err;
	}
	return 0;
	err:
	//forcing return to 0V to all channels
	for(c=0;c<5;c++) {
		if(c==2) continue; //Skip CsI channels
		ApplyHV(c,0);
	}
	return ret;
}

#include "FzTest.h"

FzTest::FzTest(FzSC *sck, const int Blk, const int Fee, const bool verbose) {
	sock=sck; ksock=nullptr;
	blk=Blk; fee=Fee;
	fVerb=verbose;
	
	Init();
}

FzTest::~FzTest() {
	if(ksock!=nullptr) delete ksock;
}

//Keithley functions
void FzTest::KeithleySetup(FzSC *sck) {
	int ret;
	char reply[MLENG];
	
	ksock=sck;
	if(sck==nullptr) return;
	
	//Reset Keithley
	if((ret=ksock->KSend("*RST",nullptr,false))<0) goto err;
	usleep(500000);
	
	//Check if Keithley answers correctly
	if((ret=ksock->KSend("*IDN?",reply,false))<0) goto err;
	if(strncmp(reply,"KEITHLEY INSTRUMENTS INC.",25)) goto err;
	
	//Settings for HV measuring
	if((ret=ksock->KSend("volt:dc:rang max",nullptr,false))<0) goto err;
	usleep(1000);
	if((ret=ksock->KSend(":Sens:Func 'volt:dc'",nullptr,false))<0) goto err;
	return;
	
	err:
	delete ksock;
	ksock=nullptr;
	return;
}

//get voltage from Keithley 2000 (wait a stable voltage, then average on 5 samples)
int FzTest::GetVoltage(double *V, double *Vvar, const bool wait) {
	int i,ret;
	char reply[MLENG];
	double tmp,old=-1,max=0,min=999;
	int dir=0,oldir=0,Nch=0,Nstab=0;
	
	//5s timeout for voltage measurement
	for(i=0;wait && i<25;i++) {
		usleep(200000);
		if((ret=ksock->KSend(":read?",reply,false))<0) return ret;
		if(sscanf(reply,"%lg",&tmp)<1) return -20;
		tmp=fabs(tmp);
		
		if(old>=0) {
			dir=((tmp==old)?dir:((tmp>old)?1:-1));
			if(oldir!=0 && oldir!=dir) Nch++;
			if(fabs(tmp-old)<=0.1) Nstab++;
			else Nstab=0;
			//exit after 3 changes of direction or 3 stable readings (Nstab==2) in a row
			if(Nch==3 || Nstab==2) break;
		}
		oldir=dir;
		old=tmp;
	}
	
	//Precise voltage measurement
	old=0; //used as average
	for(i=0;i<5;i++) {
		usleep(200000);
		if((ret=ksock->KSend(":read?",reply,false))<0) return ret;
		if(sscanf(reply,"%lg",&tmp)<1) return -20;
		tmp=fabs(tmp);
		old+=tmp;
		if(tmp<min) min=tmp;
		if(tmp>max) max=tmp;
	}
	if(V!=nullptr) *V=old/5.;
	if(Vvar!=nullptr) *Vvar=max-min;
	return 0;
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
	for(c=0;c<5;c++) {V20[c]=-1; V20var[c]=-1; Vfull[c]=-1; Vfullvar[c]=-1; Ifull[c]=-1; I1000[c]=-1;}
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
	bool hvt=true;
	
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
				if(dcreact[c]<1000) printf(" -");
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
		if(V20[c]<0 || V20var[c]<0) {hvt=false; continue;}
		
		printf(" %3s-%1s        (20 V) ",lChan[c%3],lFPGA[c/3]);
		if(abs(V20[c]-20)>1 || V20var[c]>5) printf(RED "Fail" NRM);
		else printf(GRN "Pass" NRM);
		printf(" % 20d         20",V20[c]);
		if(abs(V20[c]-20)>1) {
			printf(" V mismatch");
			failmask|=256;
			if(V20var[c]>5) printf(" -");
		}
		if(V20var[c]>5) {
			printf(" Unstable HV");
			failmask|=2048;
		}
		printf("\n");
		
		int max=v4?maxhv4[c]:maxhv3[c];
		if(Vfull[c]<0 || Vfullvar[c]<0) continue;
		printf(" %3s-%1s        (Vmax) ",lChan[c%3],lFPGA[c/3]);
		if(abs(Vfull[c]-max)>1 || Vfullvar[c]>5) printf(RED "Fail" NRM);
		else printf(GRN "Pass" NRM);
		printf(" % 20d        %3d",Vfull[c],max);
		if(abs(Vfull[c]-max)>1) {
			printf(" V mismatch");
			failmask|=256;
			if(Vfullvar[c]>5) printf(" -");
		}
		if(Vfullvar[c]>5) {
			printf(" Unstable HV");
			failmask|=2048;
		}
		printf("\n");
		
		if(Ifull[c]<0) continue;
		printf(" %3s-%1s (Ioff @ Vmax) ",lChan[c%3],lFPGA[c/3]);
		if(Ifull[c]>100) printf(RED "Fail" NRM);
		else printf(GRN "Pass" NRM);
		printf(" % 20d       <100",Ifull[c]);
		if(Ifull[c]>100) {
			printf(" Bad I offset");
			failmask|=512;
		}
		printf("\n");
		
		if(I1000[c]<0) continue;
		printf(" %3s-%1s (Iref=1000nA) ",lChan[c%3],lFPGA[c/3]);
		if(abs(I1000[c]-1000)>100) printf(RED "Fail" NRM);
		else printf(GRN "Pass" NRM);
		printf(" % 20d       1000",I1000[c]);
		if(abs(I1000[c]-1000)>100) {
			printf(" Bad I reading");
			failmask|=1024;
		}
		printf("\n");
		
	}
	if(!hvt) failmask|=128;
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
			if(N==2) {
				if((ret=HVcalib())<0) return ret;
				return 0;
			}
		}
		if(failmask&256) {
			printf("Measured voltage is inconsistent with applied voltage. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    2) Perform HV calibration procedure (ADC only)\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
			if(N==2) {
				if((ret=HVcalib())<0) return ret;
				return 0;
			}
		}
		if(failmask&512) {
			printf("Measured current is not 0 without load. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    2) Perform HV calibration procedure (ADC only)\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
			if(N==2) {
				if((ret=HVcalib())<0) return ret;
				return 0;
			}
		}
		if(failmask&1024) {
			printf("Measured current is inconsistent with Ohm's law. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>1) N=0;
			if(N==0) return 0;
		}
		if(failmask&2048) {
			printf("HV is unstable and/or maximum voltage cannot be reached. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>1) N=0;
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
			case 4:
				if((ret=HVcalib())<0) return ret;
				tmp=0;
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
		usleep(10000);
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

int FzTest::WaitHVready(const int c) {
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

int FzTest::IVmeas(const int c,int *V,int *Vvar,int *I,bool wait) {
	int ret,Nch=0,Nstab=0,Vtmp,Vmax,Vmin,Itmp,old=-1,dir=0,oldir=0;
	char query[SLENG];
	double av;
	uint8_t reply[MLENG];
	
	if(I==nullptr) wait=false;
	if(!fVerb) printf(BLD "IVmeas  " Mag " %s-%s" NRM " waiting for current stabilization\n",lChan[c%3],lFPGA[c/3]);
	sleep(2);
	//60s timeout for current measurement
	for(int i=0;i<20;i++) {
		sleep(3);
		sprintf(query,"%s,%d",lFPGA[c/3],c%3+1);
		if((ret=sock->Send(blk,fee,0x87,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&Itmp);
		if(ret!=1) return -20;
		if(!wait) break;
		if(!fVerb) printf(UP BLD "IVmeas  " Mag " %s-%s" NRM " waiting for current stabilization: " BLD "%5d" NRM " nA\n",lChan[c%3],lFPGA[c/3],Itmp);
		if(old>=0) {
			dir=((Itmp==old)?dir:((Itmp>old)?1:-1));
			if(oldir!=0 && oldir!=dir) Nch++;
			if(abs(Itmp-old)<=5) Nstab++;
			else Nstab=0;
			//exit after 3 changes of direction or 3 stable readings (Nstab==2) in a row
			if(Nch==3 || Nstab==2) break;
		}
		oldir=dir;
		old=Itmp;
	}
	if(I!=nullptr) *I=Itmp;
	if(V==nullptr) {
		if(Vvar!=nullptr) *Vvar=-1;
		return 0;
	}
	
	//voltage measurement (3 meas at 0.5s interval)
	Vmax=0; Vmin=500; Nch=0; av=0;
	sprintf(query,"%s,%d",lFPGA[c/3],c%3+1);
	for(int i=0;i<3;i++) {
		usleep(500000);
		if((ret=sock->Send(blk,fee,0x88,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d,%*d",&Vtmp);
		if(ret==1) {
			av-=(double)Vtmp;
			Nch++;
			if(-Vtmp<Vmin) Vmin=-Vtmp;
			if(-Vtmp>Vmax) Vmax=-Vtmp;
		}
	}
	if(Nch==3) {
		Vtmp=(int)(0.5+av/((double)Nch));
		if(V!=nullptr) *V=Vtmp;
		if(Vvar!=nullptr) *Vvar=Vmax-Vmin;
		if(!fVerb) printf(UP GRN "IVmeas  " Mag " %s-%s" NRM "                         " BLD "%3d" NRM " V      " BLD "%5d" NRM " nA\n",lChan[c%3],lFPGA[c/3],Vtmp,Itmp);
		return 0;
	}
	return -20;
}

int FzTest::ApplyHV(const int c,const int V) {
	int i,ret,tmp;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	if((ret=WaitHVready(c))<0) return ret;
	sprintf(query,"%s,%d,%d,%d",lFPGA[c/3],c%3+1,V,HVRAMP);
	if((ret=sock->Send(blk,fee,0x86,query,reply,fVerb))) return ret;
	
	if(!fVerb) printf(BLD "ApplyHV " Mag " %s-%s" NRM " ramping...\n",lChan[c%3],lFPGA[c/3]);
	for(i=0;i<50;i++) {
		sleep(1);
		sprintf(query,"%s,%d",lFPGA[c/3],c%3+1);
		if((ret=sock->Send(blk,fee,0xA0,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&tmp);
		if(ret!=1) return -20;
		if(tmp==1) break;
	}
	if(i==50) return -20;
	if(!fVerb) printf(UP GRN "ApplyHV " Mag " %s-%s" NRM " reached target voltage: %3d V\n",lChan[c%3],lFPGA[c/3],V);
	return 0;
}

int FzTest::HVtest() {
	int c,ret,max,V,Vvar,I;
	double R;
	char query[SLENG];
	uint8_t reply[MLENG];
	const int bit[5]={1,2,2,4,8};
	
	for(c=0;c<5;c++) {
		if(c==2) continue; //Skip CsI channels
		if((hvmask&(bit[c]))==0) {
			printf(YEL "HVtest  " Mag " %s-%s" NRM " not calibrated\n",lChan[c%3],lFPGA[c/3]);
			continue;
		}
		printf("Ready to test" Mag " %s-%s " NRM "HV channel. What do you want to do?\n",lChan[c%3],lFPGA[c/3]);
		printf("    1) Go on (HV will be applied)!\n");
		printf("    2) Skip this channel\n");
		printf("    0) Stop the HV test\n\n");
		printf("> "); scanf("%d",&ret); getchar();
		if(ret<0||ret>2) ret=0;
		if(ret==2) continue;
		if(ret==0) break;
		
		//Set max voltage
		max=v4?maxhv4[c]:maxhv3[c];
		sprintf(query,"%s,%d,%d",lFPGA[c/3],c%3+1,max);
		if((ret=sock->Send(blk,fee,0x92,query,reply,fVerb))) goto err;
		//Apply 20V to start (if HV is unstable it is unsafe to go further)
		if((ret=ApplyHV(c,20))<0) goto err;
		//Check voltage stability
		if((ret=IVmeas(c,&V,&Vvar,nullptr,false))<0) goto err;
		V20[c]=V; V20var[c]=Vvar;
		if(abs(V-20)>2 || Vvar>5) {
			if((ret=ApplyHV(c,0))<0) goto err;
			continue;
		}
		
		//If ok, apply max voltage
		if((ret=ApplyHV(c,max))<0) goto err;
		//Check current and voltage
		if((ret=IVmeas(c,&V,&Vvar,&I,true))<0) goto err;
		if((ret=ApplyHV(c,0))<0) goto err;
		Vfull[c]=V; Vfullvar[c]=Vvar; Ifull[c]=I;
		if(abs(V-max)>20 || Vvar>5) continue;
		
		//Test current
		printf("Ready to check" Mag " %s-%s " NRM "I measurement. What do you want to do?\n",lChan[c%3],lFPGA[c/3]);
		printf("  pos) Plug the load and type its res. in MOhm\n");
		printf("  neg) Skip the I measurement\n\n");
		printf("> "); scanf("%lf",&R); getchar();
		if(R<0) continue;
		if(R>200) {
			printf(YEL "OffCal  " NRM " invalid value. Skipping current check.\n");
			continue;
		}
		if((ret=ApplyHV(c,(int)(R+10.5)))<0) goto err;
		//Check current
		if((ret=IVmeas(c,nullptr,nullptr,&I,true))<0) goto err;
		if((ret=ApplyHV(c,0))<0) goto err;
		I1000[c]=I;
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

int FzTest::HVcalib() {
	int c,ret,max;
	char query[SLENG];
	uint8_t reply[MLENG];
	const int bit[5]={1,2,2,4,8};
	bool dac=false;
	
	for(c=0;c<5;c++) {
		if(c==2) continue; //Skip CsI channels
		if(hvmask&(bit[c])) printf("\n %s-%s" NRM " is already calibrated. What do you want to do?\n",lChan[c%3],lFPGA[c/3]);
		else printf("Ready to calibrate" Mag " %s-%s " NRM "HV channel. What do you want to do?\n",lChan[c%3],lFPGA[c/3]);
		printf("    1) Go on (HV will be applied, Keithley probe must be soldered before going on)!\n");
		if(hvmask&(bit[c])) printf("    2) Calibrate ADC only (HV will be applied, but Keithley is not needed)\n");
		printf("    3) Skip this channel\n");
		printf("    0) Quit the HV calibration procedure\n\n");
		printf("> "); scanf("%d",&ret); getchar();
		if(ret<0||ret>3) ret=0;
		if(ret==3) continue;
		if(ret==0) break;
		if(ret==2 && (hvmask&(bit[c]))==0) {
			printf(YEL "HVtest  " NRM "Unable to calibate ADC only (DAC must be calibrated first!)\n");
			ret=1;
		}
		if(ret==1 && ksock==nullptr) {
			printf(RED "HVtest  " NRM " Keithley 2000 is not connected or configured\n");
			break;
		}
		if(ret==1) dac=true;
		
		//Set max voltage
		max=v4?maxhv4[c]:maxhv3[c];
		sprintf(query,"%s,%d,%d",lFPGA[c/3],c%3+1,max);
		if((ret=sock->Send(blk,fee,0x92,query,reply,fVerb))) return ret;
		
		//Enable HV (v4/5 only)
		if(v4) {
			sprintf(query,"%s,1",lFPGA[c/3]);
			if((ret=sock->Send(blk,fee,0xA6,query,reply,fVerb))) return ret;
		}
		
		//Calibrate!
		if((ret=HVcalChan(c,max,dac))<0) return ret;
	}
	printf(GRN "HVcalib " NRM " calibration terminated. Please restart the card before applying HV!\n");
	return 0;
}

int FzTest::HVcalChan(const int c,const int max,const bool dac) {
	int N,i,ret,Vtarg,Vvar,maxvar,dau,oldau=-1,dacadr,isamadr;
	double VK,VKvar,Vadc[50],Iadc[50],lastV[2]={0,0},lastD[2]={0,0},bakV,bakD;
	int V2D=((c==0)||(c==3))?DACVSI1:DACVSI2;
	const int cdisc[5]={0,2,2,1,3};
	const int call[5] ={0,1,1,2,3};
	
	switch(c) {
		case 0:  dacadr=82;         isamadr=376; break;
		case 1:  dacadr=v4?142:122; isamadr=436; break;
		case 3:  dacadr=v4?222:192; isamadr=516; break;
		case 4:  dacadr=v4?282:232; isamadr=576; break;
		default: dacadr=-1;         isamadr=-1;
	}
	
	//Setting channel as NOT calibrated
	if((ret=WriteCell(78+cdisc[c],1))<0) return ret;
	if((ret=WriteCell(50+call[c],0))<0) return ret;
	
	//Before starting: force 0V to DAC output, wait 2s and check Keithley
	if((ret=SetDAC(c,0))<0) return ret;
	sleep(2);
	if((ret=GetVoltage(&VK,nullptr,false)<0)) return ret;
	if(VK>=5) {
		printf(YEL "HVcalib " NRM " measured voltage is not zero (%f). Skipping" Mag " %s-%s\n" NRM,VK,lChan[c%3],lFPGA[c/3]);
		goto bad;
	}
	
	//ADC calibration point at 0V
	if((ret=IVADC(c,Vadc,&Vvar,Iadc))<0) goto err;
	maxvar=(int)(Vadc[0]/200); //0.5%
	if(maxvar<50) maxvar=50;
	if(Vvar>=maxvar) {
		printf(YEL "HVcalib " NRM " unstable HV: Vtarg=0, ADC=%5.0f, DADC=%5d\n",Vadc[0],Vvar);
		goto bad;
	}
	
	//Calibration points every 10V
	for(Vtarg=10,N=1;Vtarg<=max;Vtarg+=10,N++) {
		printf("\n\n" CYA "HVcalib " BLD "***** NEW TARGET VOLTAGE => %3d V *****\n",Vtarg);
		if(dac) {
			//DAC CALIBRATION!
			bakV=lastV[1]; bakD=lastD[1];
			for(i=0;i<20;i++) {
				//First point: guess
				if(fabs(lastV[1]-lastV[0])<0.001) {
					dau=V2D*Vtarg;
					if(i==0) printf(CYA "HVcalib " NRM " guessing first DAC value...\n");
				}
				//Then: linear extrapolation
				else {
					dau=(int)(0.5+lastD[0]+(((double)Vtarg)-lastV[0])*(lastD[1]-lastD[0])/(lastV[1]-lastV[0]));
					if(i==0) printf(CYA "HVcalib " NRM " extrapolating from (%3.0f V: %5.0f), (%3.0f V: %5.0f)\n",lastV[0],lastD[0],lastV[1],lastD[1]);
				}
				
				if((ret=SetDAC(c,dau,oldau))<0) goto err;
				oldau=dau;
				if((ret=GetVoltage(&VK,&VKvar,true)<0)) goto err;
				if(VKvar>=1) {
					printf(YEL "HVcalib " NRM " unstable HV: Vtarg=%d, VK=%.3f, DVK=%.3f\n",Vtarg,VK,VKvar);
					goto bad;
				}
				if(fabs(VK-(double)Vtarg)>5) {
					printf(YEL "HVcalib " NRM " unexpected HV meas.: Vtarg=%d, VK=%.3f, DVK=%.3f\n",Vtarg,VK,VKvar);
					goto bad;
				}
				printf(CYA "HVcalib " Mag " %s-%s " NRM "       DAC set to %5d => VK = " BLD "%7.3f\n" NRM,lChan[c%3],lFPGA[c/3],dau,VK);
				
				lastV[0]=lastV[1]; lastD[0]=lastD[1];
				lastV[1]=VK;       lastD[1]=(double)dau;
				
				if(fabs(VK-(double)Vtarg)<0.01) break;
				if(fabs(VK-lastV[0])<0.005) break;
				if(abs(dau-(int)(lastD[0]))<5) break;
			}
			if(i>=20 || fabs(VK-(double)Vtarg)>=0.1) {
				printf(YEL "HVcalib " NRM " unable to reach target voltage!\n");
				goto bad;
			}
			lastV[0]=bakV; lastD[0]=bakD;
			
			//Write the DAC value in the correspondent PIC EEPROM cell
			for(i=0;i<2;i++) if((ret=WriteCell(dacadr+(N-1)*2+i,SELBYTE(dau,i)))<0) goto err;
		}
		else if((ret=ApplyHV(c,Vtarg))<0) goto err;
		
		//ADC calibration
		if((ret=IVADC(c,Vadc+N,&Vvar,Iadc+N))<0) goto err;
		maxvar=(int)(Vadc[N]/200); //0.5%
		if(maxvar<50) maxvar=50;
		if(Vvar>=maxvar) {
			printf(YEL "HVcalib " NRM " unstable HV: Vtarg=%d, ADC=%5.0f, DADC=%5d\n",Vtarg,Vadc[N],Vvar);
			goto bad;
		}
		if(v4) {
			dau=(int)(0.5+Iadc[N]);
			//Store I samples only in v4/5 cards
			for(i=0;i<2;i++) if((ret=WriteCell(isamadr+(N-1)*2+i,SELBYTE(dau,i)))<0) goto err;
		}
	}
	//Back to 0V
	if((ret=SetDAC(c,0))<0) return ret;
	
	//Update discrete calibration status EEPROM cell
	if((ret=WriteCell(78+cdisc[c],0))<0) return ret;
	
	//Computing linear fit coefficients and check limits
	double ii[50],p0,p1;
	long int cAV,cBV,cBI;
	unsigned long int cAI;
	int sgI,sgV;
	
	for(i=0;i<50;i++) ii[i]=(double)i;
	if((ret=LinReg(N,Vadc,ii,&p1,&p0))<0) return 0;
	if(v4) {
		cAV = (long int)(p1*1.e7);
		cBV = labs((long int)(p0*1.e7));
		sgV = (p0>=0) ? 1 : 0;
		if(cBV>=(1<<24)) {
			printf(YEL "HVcalib " NRM " V coeff B is outside boundaries\n");
			return 0;
		}
	}
	else {
		if((c%3)==0) {
			cAV = (long int)(-p1*1.e8);
			cBV = (long int)(p0*1.e8);
		}
		else {
			cAV = (long int)(-p1*1.e7);
			cBV = (long int)(p0*1.e7);
		}
		sgV=1;
		if((cBV>=(1L<<31))||(cBV<-(1L<<31))) {
			printf(YEL "HVcalib " NRM " V coeff B is outside boundaries\n");
			return 0;
		}
	}
	if((cAV>=(1L<<15))||(cAV<-(1L<<15))) {
		printf(YEL "HVcalib " NRM " V coeff A is outside boundaries\n");
		return 0;
	}
	if((ret=LinReg(N,ii,Iadc,&p1,&p0))<0) return 0;
	cAI = (unsigned long int)(p1*1.e3);
	cBI = labs((long int)p0);
	sgI = (p0>=0) ? 1 : 0;
	if(cAI>=(1L<<32)) {
		printf(YEL "HVcalib " NRM " I coeff A is outside boundaries\n");
		return 0;
	}
	if(cBI>=(1L<<32)) {
		printf(YEL "HVcalib " NRM " I coeff B is outside boundaries\n");
		return 0;
	}
	printf(GRN "HVcalib " NRM " V coeff: A =% 12ld, B =% 12ld\n",cAV,sgV?cBV:-cBV);
	printf(GRN "HVcalib " NRM " I coeff: A =%12lu, B =% 12ld\n",cAI,sgI?cBI:-cBI);
	
	for(i=0;i<2;i++) if((ret=WriteCell(54+call[c]*6+i,SELBYTE(cAV,i)))<0) return ret;
	for(i=0;i<4;i++) {
		if(v4&&i==3) {
			if((ret=WriteCell(59+call[c]*6,sgV))<0) return ret;
			break;
		}
		if((ret=WriteCell(56+call[c]*6+i,SELBYTE(cBV,i)))<0) return ret;
	}
	for(i=0;i<4;i++) if((ret=WriteCell(13+call[c]*9+i,SELBYTE(cAI,i)))<0) return ret;
	for(i=0;i<4;i++) if((ret=WriteCell(17+call[c]*9+i,SELBYTE(cBI,i)))<0) return ret;
	if((ret=WriteCell(21+call[c]*9,sgI))<0) return ret;
	
	//Mark the channel as calibrated and exit!
	if((ret=WriteCell(50+call[c],0x38))<0) return ret;
	return 0;
	
	err: //ERROR: exit from everything
	//forcing return to 0V
	SetDAC(c,0);
	return ret;
	
	bad: //BAD BEHAVIOUR: print report but don't exit
	//forcing return to 0V
	SetDAC(c,0);
	return 0;
}

int FzTest::LinReg(const int n,const double *x,const double *y,double *p1,double *p0) {
	long double sumx=0,sumx2=0,sumxy=0,sumy=0,sumy2=0,denom;
	
	for(int i=0;i<n;i++) {
		sumx  += (long double) x[i];
		sumx2 += (long double) x[i] * (long double) x[i];
		sumxy += (long double) x[i] * (long double) y[i];
		sumy  += (long double) y[i];
		sumy2 += (long double) y[i] * (long double) y[i];
	}
	
	denom = (long double)n * sumx2 - sumx * sumx;
	if(denom == 0) {
		printf(RED "LinReg  " NRM " singular matrix! Linear regression failed!\n");
		*p1 = 0;
		*p0 = 0;
		return -1;
	}
	*p1 = (double)((n * sumxy  -  sumx * sumy) / denom);
	*p0 = (double)((sumy * sumx2  -  sumx * sumxy) / denom);
	return 0;
}

int FzTest::SetDAC(const int c, const int vset, int vprev) {
	int N=0,ret,inc;
	char query[SLENG];
	uint8_t reply[MLENG];
	int V2D=((c==0)||(c==3))?DACVSI1:DACVSI2;
	
	//dac range: 0-65535
	if((vset<0)||(vset>65535)) return -20;
	if((vprev<0)||(vprev>65535)) {
		//previous value was unknown, trying to add 1 and reading new value
		sprintf(query,"%s,%d,+,1",lFPGA[c/3],(c%3)+1);
		if((ret=sock->Send(blk,fee,0x8D,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&vprev);
		if(ret!=1) return -20;
		//Qui c'era uno usleep(10000)... serviva?
	}
	
	if(!fVerb) printf(BLD "SetDAC  " Mag " %s-%s" NRM " ramping...\n",lChan[c%3],lFPGA[c/3]);
	for(;vset!=vprev;) {
		inc=abs(vset-vprev);
		//increment in 0.050s for a 20 V/s ramp
		if(inc>(V2D/2)) inc=V2D/2;
		
		if(vset>vprev) sprintf(query,"%s,%d,+,%d",lFPGA[c/3],(c%3)+1,inc);
		else sprintf(query,"%s,%d,-,%d",lFPGA[c/3],(c%3)+1,inc);
		if((ret=sock->Send(blk,fee,0x8D,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&vprev);
		if(ret!=1) return -20;
		
		//cycle every 0.050s supposing extra 40ms for SC (Keithley is slow!)
		usleep(10000);
		//next call is useful to keep under human control the HV (Keithley display is updated), but it is very slow!
		if(ksock) {
			if((ret=ksock->KSend(":read?",(char *)reply,false))<0) return ret;
		}
		if(++N >= 2000) break;
	}
	if(N>=2000) {
		printf(RED "SetDAC  " NRM " timeout. Vprev=%d, Vset=%d\n" NRM,vprev,vset);
		return -20;
	}
	if(!fVerb) printf(UP GRN "SetDAC  " Mag " %s-%s" NRM " reached DAC level " BLD "%5d\n" NRM,lChan[c%3],lFPGA[c/3],vset);
	return 0;
}

int FzTest::IVADC(const int c,double *V,int *Vvar,double *I) {
	int ret,Nch=0,Nstab=0,tmp,Vmax,Vmin,old=-1,dir=0,oldir=0;
	char query[SLENG];
	uint8_t reply[MLENG];
	double av;
	const int Ich[5]={0,1,1,2,3};
	const int Vch[5]={4,5,5,6,7};
	
	if(!fVerb) printf(BLD "IVADC   " Mag " %s-%s" NRM " waiting for current stabilization\n",lChan[c%3],lFPGA[c/3]);
	sleep(1);
	//30s timeout for current stabilization
	sprintf(query,"%d",Ich[c]);
	for(int i=0;i<30;i++) {
		sleep(1);
		if((ret=sock->Send(blk,fee,0x8F,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&tmp);
		if(ret!=1) return -20;
		if(!fVerb) printf(UP BLD "IVADC   " Mag " %s-%s" NRM " waiting for current stabilization: " BLD "%5d\n" NRM,lChan[c%3],lFPGA[c/3],tmp);
		if(old>=0) {
			dir=((tmp==old)?dir:((tmp>old)?1:-1));
			if(oldir!=0 && oldir!=dir) Nch++;
			if(abs(tmp-old)<=50) Nstab++;
			else Nstab=0;
			//exit after 4 changes of direction or 4 stable readings (Nstab==3) in a row
			if(Nch==4 || Nstab==3) break;
		}
		oldir=dir;
		old=tmp;
	}
	
	//Current measurement: 10 samples (ADC isn't already averaged onboard, differently from calibrated I measurement!)
	if(!fVerb) printf(UP BLD "IVADC   " Mag " %s-%s" NRM " current stabilized. Measuring current...      \n",lChan[c%3],lFPGA[c/3]);
	av=0;
	sprintf(query,"%d",Ich[c]);
	for(int i=0;i<10;i++) {
		usleep(50000);
		if((ret=sock->Send(blk,fee,0x8F,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&tmp);
		if(ret!=1) return -20;
		av+=(double)tmp;
	}
	old=(int)(0.5+av/10.);
	if(I!=nullptr) *I=av/10.;
	if(V==nullptr) {
		if(Vvar!=nullptr) *Vvar=-1;
		return 0;
	}
	
	//Voltage measurement: 10 samples
	if(!fVerb) printf(UP BLD "IVADC   " Mag " %s-%s" NRM " current stabilized. Measuring voltage...      \n",lChan[c%3],lFPGA[c/3]);
	Vmax=0; Vmin=65535; av=0;
	sprintf(query,"%d",Vch[c]);
	for(int i=0;i<10;i++) {
		usleep(50000);
		if((ret=sock->Send(blk,fee,0x8F,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&tmp);
		if(ret!=1) return -20;
		av+=(double)tmp;
		if(tmp<Vmin) Vmin=tmp;
		if(tmp>Vmax) Vmax=tmp;
	}
	tmp=(int)(0.5+av/10.);
	if(V!=nullptr) *V=av/10.;
	if(Vvar!=nullptr) *Vvar=Vmax-Vmin;
	if(!fVerb) printf(UP GRN "IVADC   " Mag " %s-%s" NRM "   ADC readings -> " BLD "%5d" NRM " (V)     " BLD "%5d" NRM " (I)\n",lChan[c%3],lFPGA[c/3],tmp,old);
	return 0;
}

int FzTest::ReadCell(const int add) {
	int ret,cont;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	if(add<0 || add>1023) {
		printf(RED "ReadCell" NRM " invalid address (%d)\n",add);
		return -20;
	}
	sprintf(query,"%d",add);
	if((ret=sock->Send(blk,fee,0x90,query,reply,fVerb))<0) return ret;
	if(sscanf((char *)reply,"content : %d",&cont)<1) {
		printf(RED "ReadCell" NRM " bad reply (%s)\n", reply);
		return -20;
	}
	return cont;
}

int FzTest::WriteCell(const int add,const int cont) {
	int ret;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	if(add<0 || add>1023) {
		printf(RED "WriteCel" NRM " invalid address (%d)\n",add);
		return -20;
	}
	if(cont<0 || cont>255) {
		printf(RED "WriteCel" NRM " invalid content (%d)\n",cont);
		return -20;
	}
	sprintf(query,"%d,%d",add,cont);
	if((ret=sock->Send(blk,fee,0x95,query,reply,fVerb))<0) return ret;
	
	return 0;
}

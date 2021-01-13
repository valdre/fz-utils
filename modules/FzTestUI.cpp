/*******************************************************************************
*                                                                              *
*                         Simone Valdre' - 22/12/2020                          *
*                  distributed under GPL-3.0-or-later licence                  *
*                                                                              *
*******************************************************************************/

#include "FzTest.h"

//Public: fast test routine
int FzTest::FastTest(bool hv) {
	int c,N,ret;
	char query[SLENG];
	uint8_t reply[MLENG];
	int itmp[3];
	
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
	
	//Get LV values and HV calibration status
	if(!fVerb) printf(BLD "FastTest" NRM " getting low voltages, card version and HV calibration status\n");
	if((ret=LVHVtest())<0) return ret;
	if(!fVerb) printf(UP GRN "FastTest" NRM " getting low voltages, card version and HV calibration status\n");
	
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
	
	fTested=true;
	
	//HV test (no calibration)
	if(hv) {
		if(!fVerb) printf(BLD "FastTest" NRM " testing HV quality\n");
		if((ret=HVtest())<0) return ret;
	}
	return 0;
}

//Public: result report
void FzTest::Report() {
	int c,status,cnt;
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
		if(status>=1) printf(YEL "Warn" NRM "    ");
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
			if(fabs(vref[c]-(double)(lv[c]))/vref[c]>=0.05) {
				failmask|=8;
				printf(RED "Fail" NRM);
			}
			else printf(GRN "Pass" NRM);
			printf(" %20d %10.0f %s\n",lv[c],vref[c],lvnotes[c]);
		}
	}
	
	// ADC status
	for(c=0;c<6;c++) {
		if((((gomask)&(1<<c))>>c)==0 && blvar[c2ch[c]]==0 && dcreact[c]==0) {
			adcmask|=(1<<c);
			failmask|=4096;
		}
	}
	
	// Go/NoGo
	printf(BLD "\nPre-amplifiers       " NRM);
	if(gomask<0) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
	else {
		if(gomask==63) printf(GRN "Pass" NRM "         ");
		else {
			if((gomask|adcmask)==63) printf(YEL "Warn" NRM "         ");
			else {
				failmask|=16;
				printf(RED "Fail" NRM "         ");
			}
		}
		cnt=0;
		for(c=0;c<6;c++) {
			if(adcmask&(1<<c)) printf(" ?");
			else {
				printf(" %1d",((gomask)&(1<<c))>>c);
				if((gomask&(1<<c))==0) cnt++;
			}
		}
		printf("       1=OK ");
		if(cnt) {
			printf("Broken:");
			for(c=0;c<6;c++) {
				if(((gomask|adcmask)&(1<<c))==0) printf(" %s-%s",lChan[c%3],lFPGA[c/3]);
			}
		}
		else if(adcmask) printf("Status unknown");
		printf("\n");
	}
	
	//Baseline offsets
	printf(BLD "\nBaseline offsets (in ADC units):\n" NRM);
	for(c=0;c<12;c++) {
		printf("               %s-%s ",lADC[c%6],lFPGA[c/6]);
		if(bl[c]<-8500) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
		else {
			ref=(v4)?blref5[c%6]:blref3[c%6];
			if((fabs((ref-(double)(bl[c]))/ref)>=bltoll[c%6])||(blvar[c]>=blvtol[c%6])) {
				if((adcmask&(1<<(ch2c[c])))==0 || c==1 || c==2 || c==4 || c==7 || c==8 || c==10) failmask|=32;
				printf(RED "Fail" NRM);
			}
			else printf(GRN "Pass" NRM);
			printf(" % 20d % 10.0f",bl[c],ref);
			if((adcmask&(1<<(ch2c[c]))) && (c==0 || c==3 || c==5 || c==6 || c==9 || c==11)) printf(" Broken ADC");
			else {
				if(blvar[c]>=blvtol[c%6]) {
					printf(" Unstable");
					if(fabs((ref-(double)(bl[c]))/ref)>=bltoll[c%6]) printf(" -");
				}
				if(fabs((ref-(double)(bl[c]))/ref)>=bltoll[c%6]) printf(" Bad level");
			}
			printf("\n");
		}
	}
	
	//Baseline DC level
	printf(BLD "\nBaseline DC level (in DAC units):\n" NRM);
	for(c=0;c<6;c++) {
		printf("               %s-%s ",lChan[c%3],lFPGA[c/3]);
		if(dacoff[c]<0) printf(BLU " ?? " NRM "                                 Invalid reply from PIC\n");
		else {
			if((dacoff[c]<500) || (dacoff[c]>900) || (dcreact[c]<1000)) {
				if((adcmask&(1<<c))==0) failmask|=32;
				printf(RED "Fail" NRM);
			}
			else printf(GRN "Pass" NRM);
			printf(" % 20d % 10d",dacoff[c],700);
			if(adcmask&(1<<c)) printf(" Broken ADC");
			else {
				if(dcreact[c]<1000) {
					printf(" Non responsive");
					if((dacoff[c]<500) || (dacoff[c]>900)) printf(" -");
				}
				if((dacoff[c]<500) || (dacoff[c]>900)) printf(" Bad level");
			}
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
	for(c=0;c<4;c++) {
		if(V20[c]<0 || V20var[c]<0) {hvt=false; continue;}
		
		printf(" %3s-%1s        (20 V) ",lChan[c%2],lFPGA[c/2]);
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
		
		int max=v4?maxhv4[c%2]:maxhv3[c%2];
		if(Vfull[c]<0 || Vfullvar[c]<0) continue;
		printf(" %3s-%1s        (Vmax) ",lChan[c%2],lFPGA[c/2]);
		if(abs(Vfull[c]-max)>5 || Vfullvar[c]>5) printf(RED "Fail" NRM);
		else printf(GRN "Pass" NRM);
		printf(" % 20d        %3d",Vfull[c],max);
		if(abs(Vfull[c]-max)>5) {
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
		printf(" %3s-%1s (Ioff @ Vmax) ",lChan[c%2],lFPGA[c/2]);
		if(Ifull[c]>100) printf(RED "Fail" NRM);
		else printf(GRN "Pass" NRM);
		printf(" % 20d       <100",Ifull[c]);
		if(Ifull[c]>100) {
			printf(" Bad I offset");
			failmask|=512;
		}
		printf("\n");
		
		if(I1000[c]<0) continue;
		printf(" %3s-%1s (Iref=1000nA) ",lChan[c%2],lFPGA[c/2]);
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

//Public: guided resolution
int FzTest::Guided() {
	int ret,N;
	
	if(!fTested) {
		printf(YEL "Guided  " NRM "fast test was not performed\n");
		return 0;
	}
	
	if(failmask&1) {
		printf("\nCard temperature is high. Please check cooling and restart the test...\n\n");
		return 0;
	}
	if(failmask) {
		printf("\nSome issues have been detected. What do you want to do?\n");
		printf("    1) Guided resolution\n");
		printf("    2) Manual resolution (operation menu)\n");
		printf("    0) Quit\n");
		printf("> "); scanf("%d",&ret); getchar();
		if((ret<0)||(ret>2)) ret=0;
	}
	else {
		printf("\nThe FEE is fully functional! What do you want to do?\n");
		printf("    1) Operation menu\n");
		printf("    0) Quit\n");
		printf("> "); scanf("%d",&ret); getchar();
		if(ret==1) ret=2;
		else ret=0;
	}
	printf("\n");
	if(ret==0) return 0;
	if(ret==1) {
		if(failmask&4) {
			printf("\nCard firmwares are obsolete. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, reload new fw and restart the test\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N!=1) N=0;
			if(N==0) return 0;
		}
		if(failmask&2) {
			printf("\nCard SN is not set. What do you want to do?\n");
			printf("    pos) Write new SN and press enter\n");
			printf("    neg) Ignore and go on\n");
			printf("     0 ) Quit the procedure\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N==0) return 0;
			if(N>0) {
				if((ret=SetSN(N))<0) return ret;
			}
		}
		if(failmask&8) {
			printf("\nLow voltages are out of range. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N!=1) N=0;
			if(N==0) return 0;
		}
		if(failmask&16) {
			printf("\nSome pre-amps are broken. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N!=1) N=0;
			if(N==0) return 0;
		}
		if(failmask&32) {
			printf("\nSome offsets are out of range. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    2) Try to auto-calibrate DC levels\n");
			printf("    0) Quit the procedure, check components and restart the test\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
			if(N==2) {
				if((ret=OffCal())<0) return ret;
			}
		}
		if(failmask&4096) {
			printf("\nOne or more ADCs are broken (");
			N=0; ret=0;
			for(int c=0;c<6;c++) if(adcmask&(1<<c)) N++;
			for(int c=0;c<6;c++) {
				if(adcmask&(1<<c)) {
					printf(BLD "%s" NRM,lADCcomp[c]);
					if(++ret < N) printf(", ");
				}
			}
			printf("). What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N!=1) N=0;
			if(N==0) return 0;
		}
		if(failmask&64) {
			printf("\nSome HV channels are not calibrated. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    2) Perform HV calibration procedure\n");
			printf("    0) Quit the procedure, check components and restart the test\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
			if(N==2) {
				if((ret=HVcalib())<0) return ret;
				return 0;
			}
		}
		if(failmask&128) {
			printf("\nHV was not tested. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure and restart the test with \"-H\" option\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>1) N=0;
			if(N==0) return 0;
		}
		if(failmask&256) {
			printf("\nMeasured voltage is inconsistent with applied voltage. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    2) Perform HV calibration procedure (ADC only)\n");
			printf("    0) Quit the procedure, check components and restart the test\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
			if(N==2) {
				if((ret=HVcalib())<0) return ret;
				return 0;
			}
		}
		if(failmask&512) {
			printf("\nMeasured current is not 0 without load. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    2) Perform HV calibration procedure (ADC only)\n");
			printf("    0) Quit the procedure, check components and restart the test\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>2) N=0;
			if(N==0) return 0;
			if(N==2) {
				if((ret=HVcalib())<0) return ret;
				return 0;
			}
		}
		if(failmask&1024) {
			printf("\nMeasured current is inconsistent with Ohm's law. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>1) N=0;
			if(N==0) return 0;
		}
		if(failmask&2048) {
			printf("\nHV is unstable and/or maximum voltage cannot be reached. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n");
			printf("> "); scanf("%d",&N); getchar();
			if(N<0||N>1) N=0;
			if(N==0) return 0;
		}
	}
	else Manual();
	return 0;
}

//Public: manual resolution
int FzTest::Manual() {
	int tmp,ret,N;
	
	if(failmask&1) {
		printf("\nCard temperature is high. Please check cooling and restart the test...\n\n");
		return 0;
	}
	do {
		printf("\nOperation menu\n");
		printf("    1) Set new serial number\n");
		printf("    2) Offset auto-calibration\n");
		printf("    3) Plot offset calibration curve\n");
		printf("    4) Offset manual test\n");
		printf("    5) HV calibration\n");
		if(fTested) printf("    8) Repeat the fast test\n");
		else printf("    8) Perform the fast test\n");
		if(fTested) printf("    9) Repeat the fast test with HV check\n");
		else printf("    9) Perform the fast test with HV check\n");
		printf("    0) Quit\n");
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
				if((ret=OffManual())) return ret;
				break;
			case 5:
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

//Offset calibration routine
int FzTest::OffCal() {
	int target,c,i,ch,ret,ierr,dac1,dac2,bl1,bl2,dac,Bl,tmp;
	char ctmp,query[SLENG];
	uint8_t reply[MLENG];
	int finaldac[6];
	
	printf("\nTarget baseline [-7400]>");
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
		//Reading original DAC value
		sprintf(query,"%s,%d",lFPGA[c/3],(c%3)+4);
		if((ret=sock->Send(blk,fee,0x85,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&dac);
		if(ret!=1) return -20;
		finaldac[c]=dac;
		
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
		//Always check stability!
		if((ret=OffCheck(ch))<0) return ret;
		if(bl[ch]>=8100 || bl[ch]<=-8100) {
			printf(YEL "OffCal  " NRM " %s-%s: saturating baseline\n",lADC[ch%6],lFPGA[ch/6]);
			continue;
		}
		if(blvar[ch]>5*blvtol[ch%6]) {
			printf(YEL "OffCal  " NRM " %s-%s: unstable DC level\n",lADC[ch%6],lFPGA[ch/6]);
			continue;
		}
		if(dcreact[c]<1000) {
			printf(YEL "OffCal  " NRM " %s-%s: DC level is not responsive\n",lADC[ch%6],lFPGA[ch/6]);
			continue;
		}
		//Fast search procedure
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
	
	//Testing new BL values
	for(ch=0;ch<12;ch++) {
		if((ret=OffCheck(ch))<0) return ret;
	}
	return 0;
}

//Offset vs applyed DC level plot routine
int FzTest::OffCurve() {
	FILE *fout;
	int dac,mes,c,ret;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	fout=fopen("output.txt","w");
	if(fout==NULL) return 0;
	
	if(!fVerb) {
		printf(BLD " DAC   QH1-A   Q2-A   Q3-A  QH1-B   Q2-B   Q3-B\n" NRM);
		printf("------------------------------------------------\n");
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

//Offset manual test
int FzTest::OffManual() {
	int ch,c,ret;
	int old,dac,base;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	printf("\nWhich channel do you want to test?\n");
	printf("    0) QH1-A\n");
	printf("    1)  Q2-A\n");
	printf("    2)  Q3-A\n");
	printf("    3) QH1-B\n");
	printf("    4)  Q2-B\n");
	printf("    5)  Q3-B\n");
	printf("[default=quit]> ");
	c=getchar();
	if(c!='\n') {
		for(;getchar()!='\n';);
	}
	if(c<0x30 || c>0x35) return 0;
	c-=0x30; ch=c2ch[c];
	
	if((ret=BLmeas(ch,3,nullptr,nullptr))<0) return ret;
	if(ch==1 || ch==2 || ch==4 || ch==7 || ch==8 || ch==10) return 0;
	
	//Store present DAC value
	sprintf(query,"%s,%d",lFPGA[c/3],(c%3)+4);
	if((ret=sock->Send(blk,fee,0x85,query,reply,fVerb))) return ret;
	ret=sscanf((char *)reply,"0|%d",&old);
	if(ret!=1) return -20;
	
	for(;;) {
		printf("DAC value [neg to stop] > "); scanf("%d",&dac); getchar();
		if(dac<0 || dac>1023) break;
		sprintf(query,"%s,%d,%d",lFPGA[c/3],(c%3)+1,dac);
		if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
		usleep(1000);
		if((ret=BLmeas(ch,10,&base,nullptr))<0) return ret;
		if(!fVerb) printf(UP);
		printf("DAC = " BLD "%4d" NRM " => BL =% 6d          \n",dac,base);
	}
	
	//Set DAC to previous value
	sprintf(query,"%s,%d,%d",lFPGA[c/3],(c%3)+1,old);
	if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
	return 0;
}

//HV functions
//HV extended test
int FzTest::HVtest() {
	int c,ret,max[4]={0,0,0,0},V[4],testmask=0;
	double R;
	char query[SLENG];
	uint8_t reply[MLENG];
	bool sim=false;
	
	if(!fTested) printf(YEL "HVtest  " NRM " fast test was not performed. Performing HV check only...\n");
	if((ret=LVHVtest())<0) return ret;
	
	for(c=0;c<4;c++) {
		if((hvmask&(1<<c))==0) {
			printf(" Channel" Mag " %s-%s" NRM " is not calibrated, skipping...\n",lChan[c%2],lFPGA[c/2]);
		}
		else {
			printf(" Channel" Mag " %s-%s" NRM " is calibrated, test it [Y/n]? ",lChan[c%2],lFPGA[c/2]);
			
			ret=getchar();
			if(ret!='\n') {
				for(;getchar()!='\n';);
			}
			if(ret=='\n' || ret=='y' || ret=='Y') testmask|=(1<<c);
		}
	}
	if(testmask==0) return 0;
	printf("\nReady to test, HV will be applyed. Press enter to continue..."); getchar();
	
	//Set max voltage
	for(c=0;c<4;c++) {
		if((testmask&(1<<c))==0) continue;
		max[c]=v4?maxhv4[c%2]:maxhv3[c%2];
		sprintf(query,"%s,%d,%d",lFPGA[c/2],c%2+1,max[c]);
		if((ret=sock->Send(blk,fee,0x92,query,reply,fVerb))) goto err;
	}
	
	//Apply 20V to start (if HV is unstable it is unsafe to go further)
	for(c=0;c<4;c++) V[c]=20;
	if((ret=ApplyManyHV(testmask,V))<0) goto err;
	
	//Check voltage stability and exclude unstable channels
	if((ret=ManyIVmeas(testmask,V20,V20var,nullptr,false))<0) goto err;
	for(c=0;c<4;c++) {
		if(abs(V20[c]-20)>2 || V20var[c]>5) {
			if((ret=ApplyHV(c,0))<0) goto err;
			testmask&=(~(1<<c));
		}
	}
	
	//If ok, apply max voltage...
	if((ret=ApplyManyHV(testmask,max))<0) goto err;
	//... and check current and voltage
	if((ret=ManyIVmeas(testmask,Vfull,Vfullvar,Ifull,true))<0) goto err;
	//... while returning to 0V.
	for(c=0;c<4;c++) V[c]=0;
	if((ret=ApplyManyHV(testmask,V))<0) goto err;
	
	//Exclude again unstable channels
	for(c=0;c<4;c++) {
		if((testmask&(1<<c))==0) continue;
		if(abs(Vfull[c]-max[c])>20 || Vfullvar[c]>5) {
			testmask&=(~(1<<c));
		}
	}
	
	//Test current
	printf("\nTest all channels simultaneously with 100 MOhm loads (HV will be applyed) [Y/n]> ");
	ret=getchar();
	if(ret!='\n') {
		for(;getchar()!='\n';);
	}
	if(ret=='\n' || ret=='y' || ret=='Y') sim=true;
	
	if(sim) {
		//Apply 110V
		for(c=0;c<4;c++) V[c]=110;
		if((ret=ApplyManyHV(testmask,V))<0) goto err;
		//Check current
		if((ret=ManyIVmeas(testmask,nullptr,nullptr,I1000,true))<0) goto err;
		//Return to 0V
		for(c=0;c<4;c++) V[c]=0;
		if((ret=ApplyManyHV(testmask,V))<0) goto err;
	}
	else {
		for(c=0;c<4;c++) {
			if((testmask&(1<<c))==0) continue;
			printf("\nReady to check" Mag " %s-%s " NRM "I measurement. What do you want to do?\n",lChan[c%2],lFPGA[c/2]);
			printf("  pos) Plug the load and type its res. in MOhm\n");
			printf("  neg) Skip the I measurement\n");
			printf("> "); scanf("%lf",&R); getchar();
			if(R<0) continue;
			if(R>200) {
				printf(YEL "OffCal  " NRM " invalid value. Skipping current check.\n");
				continue;
			}
			if((ret=ApplyHV(c,(int)(R+10.5)))<0) goto err;
			//Check current
			if((ret=IVmeas(c,nullptr,nullptr,I1000+c,true))<0) goto err;
			if((ret=ApplyHV(c,0))<0) goto err;
		}
	}
	
	return 0;
	err:
	//forcing return to 0V to all channels
	for(c=0;c<4;c++) ApplyHV(c,0);
	return ret;
}

//HV calibration!
int FzTest::HVcalib() {
	int c,ret,max;
	char query[SLENG];
	uint8_t reply[MLENG];
	bool dac=false;
	
	if(!fTested) printf(YEL "HVcalib " NRM " fast test was not performed. Performing HV check only...\n");
	if((ret=LVHVtest())<0) return ret;
	
	for(c=0;c<4;c++) {
		if(hvmask&(1<<c)) printf("\n %s-%s" NRM " is already calibrated. What do you want to do?\n",lChan[c%2],lFPGA[c/2]);
		else printf("\nReady to calibrate" Mag " %s-%s " NRM "HV channel. What do you want to do?\n",lChan[c%2],lFPGA[c/2]);
		printf("    1) Go on (HV will be applied, Keithley probe must be soldered before going on)!\n");
		if(hvmask&(1<<c)) printf("    2) Calibrate ADC only (HV will be applied, but Keithley is not needed)\n");
		printf("    3) Skip this channel\n");
		printf("    0) Quit the HV calibration procedure\n\n");
		if(hvmask&(1<<c)) printf("[default=3]> ");
		else printf("[default=1]> ");
		
		ret=getchar();
		if(ret=='\n') {
			if(hvmask&(1<<c)) ret=0x33;
			else ret=0x31;
		}
		else {
			for(;getchar()!='\n';);
		}
		ret-=0x30;
		if(ret<0||ret>3) ret=0;
		if(ret==3) continue;
		if(ret==0) break;
		if(ret==2 && (hvmask&(1<<c))==0) {
			printf(YEL "HVcalib " NRM "Unable to calibate ADC only (DAC must be calibrated first!)\n");
			ret=1;
		}
		if(ret==1 && ksock==nullptr) {
			printf(RED "HVcalib " NRM " Keithley 2000 is not connected or configured\n");
			break;
		}
		if(ret==1) dac=true;
		
		//Set max voltage
		max=v4?maxhv4[c%2]:maxhv3[c%2];
		sprintf(query,"%s,%d,%d",lFPGA[c/2],c%2+1,max);
		if((ret=sock->Send(blk,fee,0x92,query,reply,fVerb))) return ret;
		
		//Enable HV (v4/5 only)
		if(v4) {
			sprintf(query,"%s,1",lFPGA[c/2]);
			if((ret=sock->Send(blk,fee,0xA6,query,reply,fVerb))) return ret;
		}
		
		//Calibrate!
		if((ret=HVcalChan(c,max,dac))<0) return ret;
		fCalib=true;
	}
	max=0;
	for(c=0;c<4;c++) {
		if(tcal[c]>=0) max+=tcal[c];
	}
	printf(GRN "HVcalib " NRM " overall calibration terminated in %2dm%02ds. " BLD "Please restart the card before applying HV!\n" NRM,max/60,max%60);
	return 0;
}

//Update the card DB on disk
void FzTest::UpdateDB() {
	int c,ch,ret,N,tmp1,tmp2;
	char chr,dirname[SLENG],filename[2*SLENG+2],row[MLENG],label[SLENG],data[MLENG],lcmp[SLENG];
	struct stat st;
	uint8_t reply[MLENG];
	
	printf("\nTest ended. Do you want to update the FEE database [Y/n]? ");
	chr=getchar();
	if(chr!='\n') {
		for(;getchar()!='\n';);
	}
	if(chr!='\n' && chr!='y' && chr!='Y') return;
	
	N=-1;
	if(sn<=0) { //Try to retrieve the serial number
		if((ret=sock->Send(blk,fee,0xA5,"Q",reply,fVerb))==0) {
			ret=sscanf((char *)reply,"0|%d",&N);
			if(ret!=1) N=-1;
		}
		if(N>0) sn=N;
	}
	if(sn<=0 || sn>=65535) {
		printf("\nSN is not defined! Type a valid SN (1-65534)>");
		scanf("%d",&N); getchar();
		if(N<=0 || N>=65535) return;
		SetSN(N);
	}
	
	sprintf(dirname,"testdb/%05d",sn);
	if(stat(dirname,&st)<0) {
		if(mkdir(dirname,0755)<0) {
			perror(RED "UpdateDB" NRM);
			return;
		}
	}
	
	FzTestRef ref;
	FILE *f,*flog;
	time_t now=time(NULL);
	struct tm *nowtm=localtime(&now);
	char stime[SLENG],dumpfile[SLENG];
	strftime(stime,sizeof(stime),"%F %T",nowtm);
	strftime(dumpfile,sizeof(dumpfile),"dump%Y%m%d%H%M%S.txt",nowtm);
	
	sprintf(filename,"%s/log.txt",dirname);
	if((flog=fopen(filename,"a"))==NULL) {
		perror(RED "UpdateDB" NRM);
		return;
	}
	fprintf(flog,"[%s] New test performed\n",stime);
	
	//EEPROM dump
	sprintf(filename,"%s/%s",dirname,dumpfile);
	FullRead(filename);
	
	//GENERAL INFORMATION
	sprintf(filename,"%s/general.txt",dirname);
	f=fopen(filename,"r");
	if(f!=NULL) {
		//previous file exists
		N=0; ref.hvmask=0;
		for(;fgets(row,MLENG,f);) {
			ret=sscanf(row," %[^:]: %[^\n]",label,data);
			//ret==1 is ok, it means "untested", so I can continue because ref is initialized with "untested" values
			if(ret!=2) continue;
			if(strcmp(label,"FEE version")==0) {
				if(strcmp(data,"v4/5")==0) ref.v4=1;
				else if(strcmp(data,"v3")==0) ref.v4=0;
			}
			else if(strcmp(label,"FEE serial number")==0) {
				ref.sn=atoi(data);
				if(ref.sn<=0 || ref.sn>=65535) ref.sn=-1;
			}
			else if(strcmp(label,"PIC FW version")==0) {
				if(strlen(data)<=10) strcpy(ref.vPIC,data);
			}
			else if(strcmp(label,"FPGA A FW version")==0) {
				if(strlen(data)<=10) strcpy(ref.vFPGA[0],data);
			}
			else if(strcmp(label,"FPGA B FW version")==0) {
				if(strlen(data)<=10) strcpy(ref.vFPGA[1],data);
			}
			else if(strcmp(label,"Low voltages")==0) {
				ret=sscanf(data," %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",ref.lv,ref.lv+1,ref.lv+2,ref.lv+3,ref.lv+4,ref.lv+5,ref.lv+6,ref.lv+7,ref.lv+8,ref.lv+9,ref.lv+10,ref.lv+11,ref.lv+12,ref.lv+13,ref.lv+14,ref.lv+15,ref.lv+16,ref.lv+17,ref.lv+18);
				if(ret!=19) {
					for(c=0;c<19;c++) ref.lv[c]=-1;
				}
			}
			else {
				for(c=0;c<4;c++) {
					sprintf(lcmp,"%s-%s HV calibrated",lChan[c%2],lFPGA[c/2]);
					if(strcmp(label,lcmp)==0) {
						if(strcmp(data,"yes")==0) {
							N++;
							ref.hvmask|=(1<<c);
						}
						else if(strcmp(data,"no")==0) N++;
						break;
					}
				}
			}
		}
		fclose(f);
		if(N==0) ref.hvmask=-1;
	}
	if((f=fopen(filename,"w"))==NULL) {
		perror(RED "UpdateDB" NRM);
		fclose(flog);
		return;
	}
	if(ref.v4>=0 && v4<0) v4=ref.v4;
	if(ref.v4!=v4) fprintf(flog,"[%s]  general.txt          FEE version DB entry was updated\n",stime);
	fprintf(f,"         FEE version: %10s\n",(v4<0)?"":((v4>0)?"v4/5":"v3"));
	
	if(ref.sn>=0 && sn<0) sn=ref.sn;
	if(ref.sn!=sn) fprintf(flog,"[%s]  general.txt    FEE serial number DB entry was updated\n",stime);
	if(sn>=0) fprintf(f,"   FEE serial number: % 10d\n",sn);
	else fprintf(f,"   FEE serial number:           \n");
	
	if(strlen(ref.vPIC)>0 && strlen(vPIC)==0) strcpy(vPIC,ref.vPIC);
	if(strcmp(ref.vPIC,vPIC)!=0) fprintf(flog,"[%s]  general.txt       PIC FW version DB entry was updated\n",stime);
	fprintf(f,"      PIC FW version: %10s\n",vPIC);
	
	for(c=0;c<2;c++) {
		if(strlen(ref.vFPGA[c])>0 && strlen(vFPGA[c])==0) strcpy(vFPGA[c],ref.vFPGA[c]);
		if(strcmp(ref.vFPGA[c],vFPGA[c])!=0) fprintf(flog,"[%s]  general.txt    FPGA %s FW version DB entry was updated\n",stime,lFPGA[c]);
		fprintf(f,"   FPGA %s FW version: %10s\n",lFPGA[c],vFPGA[c]);
	}
	
	for(c=0;c<19;c++) {
		if((ref.lv[c]>=0 || ref.lv[c]==-2) && lv[c]==-1) lv[c]=ref.lv[c];
	}
	//Always update lv
	fprintf(flog,"[%s]  general.txt         Low voltages DB entry was updated\n",stime);
	fprintf(f,"        Low voltages:");
	for(c=0;c<19;c++) fprintf(f," % 4d",lv[c]);
	fprintf(f,"\n");
	
	if(ref.hvmask>=0 && hvmask<0) hvmask=ref.hvmask;
	if(ref.hvmask!=hvmask) fprintf(flog,"[%s]  general.txt     HV calib. status DB entry was updated\n",stime);
	for(c=0;c<4;c++) {
		if(hvmask>=0) fprintf(f," %s-%s HV calibrated: %10s\n",lChan[c%2],lFPGA[c/2],(hvmask&(1<<c))?"yes":"no");
		else fprintf(f," %s-%s HV calibrated:           \n",lChan[c%2],lFPGA[c/2]);
	}
	fclose(f);
	
	//ANALOG CHAIN INFORMATION
	sprintf(filename,"%s/analog.txt",dirname);
	f=fopen(filename,"r");
	if(f!=NULL) {
		//previous file exists
		for(;fgets(row,MLENG,f);) {
			ret=sscanf(row," %[^:]: %[^\n]",label,data);
			//ret==1 is ok, it means "untested", so I can continue because ref is initialized with "untested" values
			if(ret!=2) continue;
			
			for(ch=0;ch<12;ch++) {
				c=ch2c[ch];
				sprintf(lcmp,"%s-%s offset",lADCs[ch%6],lFPGA[ch/6]);
				if(strcmp(lcmp,label)==0) {
					N=sscanf(data," %d %d %d %d",ref.bl+ch,ref.blvar+ch,&tmp1,&tmp2);
					if((N==4)&&(ch==0 || ch==3 || ch==5 || ch==6 || ch==9 || ch==11)) {
						ref.dacoff[c]=tmp1; ref.dcreact[c]=tmp2;
					}
					else if(N!=2) {
						ref.bl[ch]=-10000; ref.blvar[ch]=-10000;
					}
					break;
				}
			}
		}
		fclose(f);
	}
	if((f=fopen(filename,"w"))==NULL) {
		perror(RED "UpdateDB" NRM);
		fclose(flog);
		return;
	}
	
	fprintf(f,"# First value -> measured base line level in ADC units\n");
	fprintf(f,"#Second value -> maximum variation of base line level\n");
	fprintf(f,"# Third value -> currently set DAC value (QH1, Q2 and Q3 only)\n");
	fprintf(f,"#Fourth value -> base line shift with a 200 unit variation of the DAC output (QH1, Q2 and Q3 only)\n\n");
	for(ch=0;ch<12;ch++) {
		sprintf(lcmp,"        %s-%s offset",lADC[ch%6],lFPGA[ch/6]);
		if(ref.bl[ch]>-9000 && bl[ch]<-9000) {bl[ch]=ref.bl[ch]; blvar[ch]=ref.blvar[ch];}
		else fprintf(flog,"[%s]   analog.txt %s DB entry was updated (offsets)\n",stime,lcmp);
		fprintf(f,"%s: %5d %5d",lcmp,bl[ch],blvar[ch]);
		if(ch==1 || ch==2 || ch==4 || ch==7 || ch==8 || ch==10) {
			fprintf(f,"\n");
			continue;
		}
		c=ch2c[ch];
		if(ref.dacoff[c]>=0 && dacoff[c]<0) {dacoff[c]=ref.dacoff[c]; dcreact[c]=ref.dcreact[c];}
		else fprintf(flog,"[%s]   analog.txt %s DB entry was updated (DAC)\n",stime,lcmp);
		fprintf(f," %5d %5d\n",dacoff[c],dcreact[c]);
	}
	fclose(f);
	
	//HV TEST OUTPUT
	sprintf(filename,"%s/hvtest.txt",dirname);
	f=fopen(filename,"r");
	if(f!=NULL) {
		//previous file exists
		for(;fgets(row,MLENG,f);) {
			ret=sscanf(row," %[^:]: %[^\n]",label,data);
			//ret==1 is ok, it means "untested", so I can continue because ref is initialized with "untested" values
			if(ret!=2) continue;
			
			for(c=0;c<4;c++) {
				sprintf(lcmp,"%s-%s HV test",lChan[c%2],lFPGA[c/2]);
				if(strcmp(lcmp,label)==0) {
					N=sscanf(data," %d %d %d %d %d %d",ref.V20+c,ref.V20var+c,ref.Vfull+c,ref.Vfullvar+c,ref.Ifull+c,ref.I1000+c);
					if(N!=6) {
						ref.V20[c]=-1; ref.V20var[c]=-1; ref.Vfull[c]=-1; ref.Vfullvar[c]=-1; ref.Ifull[c]=-1; ref.I1000[c]=-1;
					}
					break;
				}
			}
		}
		fclose(f);
	}
	if((f=fopen(filename,"w"))==NULL) {
		perror(RED "UpdateDB" NRM);
		fclose(flog);
		return;
	}
	
	fprintf(f,"# First value -> measured HV at 20V calibrated in V\n");
	fprintf(f,"#Second value -> maximum variation of HV level at 20V\n");
	fprintf(f,"# Third value -> measured maximum HV calibrated in V\n");
	fprintf(f,"#Fourth value -> maximum variation of maximum HV level\n");
	fprintf(f,"# Fifth value -> measured current at maximum voltage without load (nA)\n");
	fprintf(f,"# Sixth value -> measured current with load (1000 nA expected)\n\n");
	for(c=0;c<4;c++) {
		sprintf(lcmp,"       %s-%s HV test",lChan[c%2],lFPGA[c/2]);
		if(ref.V20[c]>=0 && V20[c]<0) {V20[c]=ref.V20[c]; V20var[c]=ref.V20var[c];}
		else if(abs(V20[c]-ref.V20[c])>1 || abs(V20var[c]-ref.V20var[c])>1) fprintf(flog,"[%s]   hvtest.txt %s DB entry was updated (V20)\n",stime,lcmp);
		fprintf(f,"%s: %3d %3d",lcmp,V20[c],V20var[c]);
		
		if(ref.Vfull[c]>=0 && Vfull[c]<0) {Vfull[c]=ref.Vfull[c]; Vfullvar[c]=ref.Vfullvar[c]; Ifull[c]=ref.Ifull[c];}
		else if(abs(Vfull[c]-ref.Vfull[c])>1 || abs(Vfullvar[c]-ref.Vfullvar[c])>1 || abs(Ifull[c]-ref.Ifull[c])>50) fprintf(flog,"[%s]   hvtest.txt %s DB entry was updated (Vfull)\n",stime,lcmp);
		fprintf(f," %3d %3d %4d",Vfull[c],Vfullvar[c],Ifull[c]);
		
		if(ref.I1000[c]>=0 && I1000[c]<0) I1000[c]=ref.I1000[c];
		else if(abs(I1000[c]-ref.I1000[c])>50) fprintf(flog,"[%s]   hvtest.txt %s DB entry was updated (I1000)\n",stime,lcmp);
		fprintf(f," %4d\n",I1000[c]);
	}
	fclose(f);
	
	//HV CALIBRATION DATA
	if(!fCalib) goto fine;
	sprintf(filename,"%s/hvcalib.txt",dirname);
	f=fopen(filename,"r");
	if(f!=NULL) {
		//previous file exists
		for(;fgets(row,MLENG,f);) {
			ret=sscanf(row," %[^:]: %[^\n]",label,data);
			//ret==1 is ok, it means "untested", so I can continue because ref is initialized with "untested" values
			if(ret!=2) continue;
			tmp1=0;
			for(c=0;c<4;c++) {
				sprintf(lcmp,"%s-%s ADC p0        ",lChan[c%2],lFPGA[c/2]);
				if(strcmp(label,lcmp)==0) {
					N=sscanf(data," %lg %lg",ref.Vp0+c,ref.Ip0+c);
					if(N!=2) {
						ref.Vp0[c]=-99; ref.Ip0[c]=-99;
					}
					tmp1=1;
					break;
				}
				sprintf(lcmp,"%s-%s ADC p1        ",lChan[c%2],lFPGA[c/2]);
				if(strcmp(label,lcmp)==0) {
					N=sscanf(data," %lg %lg",ref.Vp1+c,ref.Ip1+c);
					if(N!=2) {
						ref.Vp1[c]=-99; ref.Ip1[c]=-99;
					}
					tmp1=1;
					break;
				}
				int max=(v4?maxhv4[c%2]:maxhv3[c%2])/10;
				for(ch=0;ch<=max;ch++) {
					sprintf(lcmp,"%s-%s          %3d V",lChan[c%2],lFPGA[c/2],ch*10);
					if(strcmp(label,lcmp)==0) {
						N=sscanf(data," %lg %lg %lg %lg",ref.Vkei[c]+ch,ref.Vdac[c]+ch,ref.Vadc[c]+ch,ref.Iadc[c]+ch);
						if(N!=4) {
							ref.Vkei[c][ch]=-1; ref.Vdac[c][ch]=-1; ref.Vadc[c][ch]=-1; ref.Iadc[c][ch]=-1;
						}
						tmp1=1;
						break;
					}
				}
				if(tmp1) break;
			}
			//if(tmp1) continue;
		}
		fclose(f);
	}
	if((f=fopen(filename,"w"))==NULL) {
		perror(RED "UpdateDB" NRM);
		fclose(flog);
		return;
	}
	
	fprintf(f,"# First value -> HV measured by Keithley (in V) \n");
	fprintf(f,"#Second value -> corresponding DAC value\n");
	fprintf(f,"# Third value -> voltage ADC reading (or fit parameter)\n");
	fprintf(f,"#Fourth value -> current ADC reading (or fit parameter)\n");
	fprintf(flog,"[%s]  hvcalib.txt HV calibration table was updated\n\n",stime);
	for(c=0;c<4;c++) {
		sprintf(lcmp,"%s-%s ADC p0        ",lChan[c%2],lFPGA[c/2]);
		if(ref.Vp0[c]>-98 && Vp0[c]<=-98) Vp0[c]=ref.Vp0[c];
		if(ref.Ip0[c]>-98 && Ip0[c]<=-98) Ip0[c]=ref.Ip0[c];
		fprintf(f,"%s:                   %8.3f %8.3f\n",lcmp,Vp0[c],Ip0[c]);
		
		sprintf(lcmp,"%s-%s ADC p1        ",lChan[c%2],lFPGA[c/2]);
		if(ref.Vp1[c]>-99 && Vp1[c]<=-99) Vp1[c]=ref.Vp1[c];
		if(ref.Ip1[c]>-99 && Ip1[c]<=-99) Ip1[c]=ref.Ip1[c];
		fprintf(f,"%s:                   %8.3f %8.3f\n",lcmp,Vp1[c],Ip1[c]);
		
		int max=(v4?maxhv4[c%2]:maxhv3[c%2])/10;
		for(ch=0;ch<=max;ch++) {
			sprintf(lcmp,"%s-%s          %3d V",lChan[c%2],lFPGA[c/2],ch*10);
			if(ref.Vkei[c][ch]>=0 && Vkei[c][ch]<0) Vkei[c][ch]=ref.Vkei[c][ch];
			if(ref.Vdac[c][ch]>=0 && Vdac[c][ch]<0) Vdac[c][ch]=ref.Vdac[c][ch];
			if(ref.Vadc[c][ch]>=0 && Vadc[c][ch]<0) Vadc[c][ch]=ref.Vadc[c][ch];
			if(ref.Iadc[c][ch]>=0 && Iadc[c][ch]<0) Iadc[c][ch]=ref.Iadc[c][ch];
			fprintf(f,"%s: %7.3f %5.0f    %5.0f    %5.0f\n",lcmp,Vkei[c][ch],Vdac[c][ch],Vadc[c][ch],Iadc[c][ch]);
		}
	}
	fclose(f);
	
	fine:
	fclose(flog);
	return;
}

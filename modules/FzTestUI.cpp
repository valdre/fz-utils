/*******************************************************************************
*                                                                              *
*                         Simone Valdre' - 17/12/2021                          *
*                  distributed under GPL-3.0-or-later licence                  *
*                                                                              *
*******************************************************************************/

#include "FzTest.h"

//Fast test routine
int FzTest::FastTest(bool man) {
	int c, N, Nfail, ret;
	float ref;
	double ComSc=0, ExtSc=0;
	bool fFailOff[6], fEx=true;
	for(int j = 0; j < 6; j++) fFailOff[j] = false;
	
	Init();
	printf("\n\n");
	printf(BLD "                         **************************************************\n");
	printf("                         *                  TEST  REPORT                  *\n");
	printf("                         **************************************************\n\n" NRM);
	printf(BLD "      Parameter      Res.         Value         Reference  Note\n" NRM);
	printf("----------------------------------------------------------------------------------------------------\n");
	
	//General test (SN, FW, temperatures, LV, card version, HV cal. status)
	if((ret=TestGeneral())<0) return ret;
	
	//Card type and serial number
	printf(BLD "Card Type and SN     " NRM);
	if(sn==65535) {
		failmask|=FAIL_SN;
		printf(YEL "Warn" NRM "                 %4s            No SN data\n",v4?"v4/5":"  v3");
		fEx = false;
	}
	else printf(GRN "Pass" NRM "             %4s-%03d\n",v4?"v4/5":"  v3",sn);
	
	//PIC and FPGA version
	printf(BLD "\nFirmware:\n" NRM);
	printf("         PIC Version ");
	if(strcmp(vPIC,v4?LASTVPICV4:LASTVPICV3)) {
		failmask|=FAIL_FW;
		printf(YEL "Warn" NRM " %20s %10s Obsolete firmware\n",vPIC,v4?LASTVPICV4:LASTVPICV3);
	}
	else {
		printf(GRN "Pass" NRM " %20s %10s\n",vPIC,v4?LASTVPICV4:LASTVPICV3);
		ExtSc += 4;
	}
	for(c=0;c<2;c++) {
		printf("      FPGA %s Version ",lFPGA[c]);
		if(strcmp(vFPGA[c],LASTVFPGA)) {
			failmask|=FAIL_FW;
			printf(YEL"Warn" NRM " %20s %10s Obsolete firmware\n",vFPGA[c],LASTVFPGA);
		}
		else {
			printf(GRN "Pass" NRM " %20s %10s\n",vFPGA[c],LASTVFPGA);
			ExtSc += 3;
		}
	}
	
	//Temperatures
	ret=0;
	for(c=0;c<6;c++) {
		if(temp[c]>=70) ret|=2;
		else if(temp[c]>=50) ret|=1;
	}
	printf(BLD "\nTemperatures (in °C) " NRM);
	if(ret>=1) printf(YEL "Warn" NRM "    ");
	else printf(GRN "Pass" NRM "    ");
	for(c=0;c<6;c++) {
		if(c>0) printf(",");
		printf("%2d",temp[c]);
	}
	if(ret>1) {
		failmask|=FAIL_TEMP;
		printf("     <50-70 Overheat\n");
	}
	else if(ret==1) printf("     <50-70 Warm\n");
	else printf("     <50-70\n");
	
	//Low voltages
	printf(BLD "\nLow voltages (in mV, measured by " Mag "M46" BLD ", " Cya "M1" BLD " and " Blu "M2" BLD "):\n" NRM);
	ComSc -= 9;
	for(c=0;c<19;c++) {
		if(lv[c]==-2) {
			ComSc += 1;
			continue;
		}
		printf("%s ",lvlabel[c]);
		if(fabs(vref[c]-(double)(lv[c]))/vref[c]>=0.05) {
			failmask|=FAIL_LV;
			printf(RED "Fail" NRM);
			fEx = false;
		}
		else {
			printf(GRN "Pass" NRM);
			ComSc += 1;
		}
		printf(" %20d %10.0f %s\n", lv[c], vref[c], lvnotes[c]);
	}
	
	//Analog chain test (Pre-Amp, DC offsets and ADCs)
	if((ret=TestAnalog())<0) return ret;
	
	// ADC and Pre-amp Go/NoGo test
	printf(BLD "\nADC working bits and pre-amplifiers Go / NOGo test:\n" NRM);
	ComSc -= 4;
	for(c=0; c<6; c++) {
		printf("               %s-%s ",lChan[c%3],lFPGA[c/3]);
		if(adcmask&(1<<c)) {
			failmask|=FAIL_ADC;
			printf(RED "Fail" NRM);
			fEx = false;
		}
		else if((gomask&(1<<c))==0) {
			failmask|=FAIL_PREAMP;
			printf(RED "Fail" NRM);
			fEx = false;
		}
		else {
			printf(GRN "Pass" NRM);
			ComSc += 4;
		}
		for(int i=13; i>=0; i--) {
			if(i%2) printf(" ");
			printf("%d", (adcbits[c]>>i)&1);
		}
		printf("           ");
		if(adcmask&(1<<c)) {
			if(adcbits[c] == 16383) printf(" ADC S/H failure");
			else printf(" Some ADC bits hanged");
		}
		else if((gomask&(1<<c))==0) printf(" Pre-amp failure");
		printf("\n");
	}
	
	//Baseline DC level
	printf(BLD "\nBaseline DC level (in DAC units):\n" NRM);
	ComSc -= 2;
	for(c=0;c<6;c++) {
		if(adcmask&(1<<c)) {
			ComSc += 2;
			continue; //Skip line if ADC is broken
		}
		printf("               %s-%s ",lChan[c%3],lFPGA[c/3]);
		Nfail=0;
		if(abs(dacoff[c]-dcref) > dcvar) {
			failmask |= FAIL_DC;
			Nfail++;
		}
		if(abs(dcreact[c]-reacref) > reacvar) {
			fFailOff[c] = true;
			failmask |= FAIL_OFFSET;
			Nfail++;
		}
		if(Nfail == 0) {
			printf(GRN "Pass" NRM);
			ComSc += 2;
		}
		else {
			printf(RED "Fail" NRM);
			fEx = false;
		}
		
		printf(" % 20d % 10d", dacoff[c], dcref);
		if(abs(dcreact[c]-reacref) > reacvar) {
			printf(" Bad reaction");
			if(--Nfail) printf(" -");
		}
		if(abs(dacoff[c]-dcref) > dcvar) printf(" Bad level");
		printf("\n");
	}
	
	//Baseline offsets
	printf(BLD "\nBaseline offsets (in ADC units):\n" NRM);
	ComSc -= 2; ExtSc += 16;
	for(c=0;c<12;c++) {
		if(adcmask&(1<<ch2c[c]) && is_charge[c%6]) {
			ComSc += 1;
			continue; //Skip line if ADC is broken
		}
		if(fFailOff[ch2c[c]] && is_charge[c%6]) {
			ComSc += 1;
			continue; //Skip line if a failure on DC level was already shown
		}
		
		printf("               %s-%s ", lADC[c%6], lFPGA[c/6]);
		ref = v4 ? blref5[c%6] : blref3[c%6];
		Nfail=0;
		if(fabs(ref-(double)(bl[c])) >= bltoll[c%6]) {
			failmask |= FAIL_OFFSET;
			Nfail++;
		}
		else ExtSc += 1 - fabs(ref-(double)(bl[c])) / bltoll[c%6];
		
		if(blvar[c] >= blvtol[c%6]) {
			failmask |= FAIL_OFFSET;
			Nfail++;
		}
		else ExtSc += 1 - blvar[c] / blvtol[c%6];
		
		if(Nfail) {
			printf(RED "Fail" NRM);
			fEx = false;
		}
		else {
			printf(GRN "Pass" NRM);
			ComSc += 1;
		}
		
		printf(" % 20d % 10.0f Var = %5.1lf ", bl[c], ref, blvar[c]);
		if(blvar[c]>=blvtol[c%6]) {
			printf(" Unstable");
			if(--Nfail) printf(" -");
		}
		if(fabs(ref-(double)(bl[c]))>=bltoll[c%6]) {
			printf(" Bad level");
			if(--Nfail) printf(" -");
		}
		printf("\n");
	}
	
	//HV calibration
	printf(BLD "\nHV calibration status:\n" NRM);
	for(c=0;c<4;c++) {
		printf("               %s-%s ",lChan[c%2],lFPGA[c/2]);
		if((hvmask&(1<<c))==0) {
			failmask|=FAIL_HVCALIB;
			printf(YEL "Uncal." NRM);
		}
		else printf(GRN "Pass" NRM);
		printf("\n");
	}
	
	//Final score
	Score = ComSc;
	if(fEx) Score += ExtSc;
	printf(BLD "\nFEE overall quality score: ");
	if(Score < 60) printf(RED);
	else if(Score < 85) printf(YEL);
	else printf(GRN);
	printf("%3.0f\n" NRM, Score);
	
	//If FastTest was launched in manual mode exit now.
	if(man) return 0;
	
	//Else propose solutions:
	if(failmask&FAIL_TEMP) {
		printf("\nCard temperature is high. Please check cooling and restart the test...\n\n");
		return 0;
	}
	if(failmask) {
		printf("\nSome issues have been detected. In this case other test results may be hidden\nbecause they are unreliable until the issues were solved.\nRemember to launch again the test after solving all the issues.\nWhat do you want to do?\n");
		printf("    1) Guided resolution\n");
		printf("    2) Manual resolution (operation menu)\n");
		printf("    0) Quit\n");
		printf("[default=1]> ");
		ret=getchar();
		if(ret=='\n') ret=0x31;
		else {
			for(;getchar()!='\n';);
		}
		ret-=0x30;
		if((ret<0)||(ret>2)) ret=0;
	}
	else {
		printf("\nThe FEE seems fully functional (but HV was not tested)! What do you want to do?\n");
		printf("    1) Test HV\n");
		printf("    2) Operation menu\n");
		printf("    0) Quit\n");
		printf("[default=1]> ");
		ret=getchar();
		if(ret=='\n') ret=0x31;
		else {
			for(;getchar()!='\n';);
		}
		ret-=0x30;
		if((ret<0)||(ret>2)) ret=0;
		if(ret==1) {
			if((ret=HVTest())<0) return ret;
			ret=0;
		}
	}
	printf("\n");
	if(ret==0) return 0;
	if(ret==1) {
		if(failmask&FAIL_FW) {
			printf("\nCard firmwares are obsolete. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, reload new fw and restart the test\n");
			printf("[default=1]> ");
			ret=getchar();
			if(ret=='\n') ret=0x31;
			else {
				for(;getchar()!='\n';);
			}
			ret-=0x30;
			if(ret!=1) return 0;
		}
		if(failmask&FAIL_SN) {
			printf("\nCard SN is not set. What do you want to do?\n");
			printf("    pos) Write new SN and press enter\n");
			printf("    neg) Ignore and go on\n");
			printf("     0 ) Quit the procedure\n");
			printf("> "); scanf("%d",&ret); getchar();
			if(ret==0) return 0;
			if(ret>0) {
				if((ret=SetSN(ret))<0) return ret;
			}
		}
		if(failmask&FAIL_LV) {
			printf("\nLow voltages are out of range. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n");
			printf("[default=1]> ");
			ret=getchar();
			if(ret=='\n') ret=0x31;
			else {
				for(;getchar()!='\n';);
			}
			ret-=0x30;
			if(ret!=1) return 0;
		}
		if(failmask&FAIL_PREAMP) {
			printf("\nSome pre-amps are broken. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    0) Quit the procedure, check components and restart the test\n");
			printf("[default=1]> ");
			ret=getchar();
			if(ret=='\n') ret=0x31;
			else {
				for(;getchar()!='\n';);
			}
			ret-=0x30;
			if(ret!=1) return 0;
		}
		if(failmask&FAIL_DC) {
			printf("\nSome offsets are not regulated. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    2) Auto-calibrate DC levels\n");
			printf("    0) Quit the procedure\n");
			printf("[default=2]> ");
			ret=getchar();
			if(ret=='\n') {
				ret=0x32;
			}
			else {
				for(;getchar()!='\n';);
			}
			ret-=0x30;
			if((ret<=0)||(ret>2)) return 0;
			if(ret==2) {
				if((ret=OffCal())<0) return ret;
			}
		}
		if(failmask&FAIL_OFFSET) {
			printf("\nSome offsets are unstable or out of range. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			printf("    2) Run a DAC-offset curve test\n");
			printf("    0) Quit the procedure, check components and restart the test\n");
			printf("[default=1]> ");
			ret=getchar();
			if(ret=='\n') ret=0x31;
			else {
				for(;getchar()!='\n';);
			}
			ret-=0x30;
			if((ret<=0)||(ret>2)) return 0;
			if(ret==2) {
				if((ret=OffCurve())<0) return ret;
			}
		}
		if(failmask&FAIL_ADC) {
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
			printf("[default=1]> ");
			ret=getchar();
			if(ret=='\n') ret=0x31;
			else {
				for(;getchar()!='\n';);
			}
			ret-=0x30;
			if(ret!=1) return 0;
		}
		if(failmask&FAIL_HVCALIB) {
			printf("\nSome HV channels are not calibrated. What do you want to do?\n");
			printf("    1) Ignore and go on\n");
			if(ksock!=nullptr) printf("    2) Perform HV calibration procedure (multimeter probes must be soldered)\n");
			printf("    0) Quit the procedure, solder probes and launch with option \"-k\"\n");
			printf("[default=1]> ");
			ret=getchar();
			if(ret=='\n') ret=0x31;
			else {
				for(;getchar()!='\n';);
			}
			ret-=0x30;
			if((ret<=0)||(ret>2)) return 0;
			if(ret==2) {
				if(ksock==nullptr) return 0;
				if((ret=HVCalib())<0) return ret;
				return 0;
			}
		}
	}
	else Manual();
	return 0;
}

//HV extended test
int FzTest::HVTest() {
	int c,ret,max[4]={0,0,0,0},V[4],testmask=0;
	double R, ComSc = 0, ExtSc = 0;
	char query[SLENG];
	uint8_t reply[MLENG];
	bool sim=false, fEx=true;
	
	if(!tGeneral) {
		printf(YEL "HVTest  " NRM " fast test was not performed. Performing general check only...\n");
		if((ret=TestGeneral())<0) return ret;
	}
	
	for(c=0;c<4;c++) {
		tHV[c]=false;
		if((hvmask&(1<<c))==0) {
			printf(" Channel" Mag " %s-%s" NRM " is not calibrated, skipping...\n",lChan[c%2],lFPGA[c/2]);
			fEx = false;
		}
		else {
			ComSc += 5;
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
		tHV[c]=true;
		max[c]=v4?maxhv4[c%2]:maxhv3[c%2];
		sprintf(query,"%s,%d,%d",lFPGA[c/2],c%2+1,max[c]);
		if((ret=sock->Send(blk,fee,0x92,query,reply,fVerb))) goto err;
	}
	
	//Apply 20V to start (if HV is unstable it is unsafe to go further)
	for(c=0;c<4;c++) V[c]=20;
	if((ret=ApplyManyHV(testmask,V))<0) goto err;
	
	//Check voltage stability and exclude unstable channels
	ComSc -= 3; ExtSc += 3;
	if((ret=ManyIVmeas(testmask,V20,V20var,nullptr,false))<0) goto err;
	for(c=0;c<4;c++) {
		if((testmask&(1<<c))==0) continue;
		if(abs(V20[c]-20)>2 || V20var[c]>2) {
			if((ret=ApplyHV(c,0))<0) goto err;
			testmask&=(~(1<<c));
			fEx = false;
		}
		else {
			ComSc += 2;
			ExtSc += 1. - ((double)(V20[c] - 20)) / 2.;
			ExtSc += 2. - V20var[c];
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
	ComSc -= 3; ExtSc += 3;
	for(c=0;c<4;c++) {
		if((testmask&(1<<c))==0) continue;
		if(abs(Vfull[c]-max[c])>20 || Vfullvar[c]>2) {
			testmask&=(~(1<<c));
			fEx = false;
		}
		else {
			ComSc += 2;
			ExtSc += 1. - ((double)(Vfull[c] - max[c])) / 20.;
			ExtSc += 2. - Vfullvar[c];
			
			if(Ifull[c] <= 100) {
				ComSc += 5;
				ExtSc += 5. - ((double)(Ifull[c])) / 20.;
			}
			else fEx = false;
		}
	}
	
	//Test current
	printf("\nTest all channels with 100 MOhm loads (HV will be applyed), s = skip all [Y/n/s]> ");
	ret=getchar();
	if(ret!='\n') {
		for(;getchar()!='\n';);
	}
	if(ret=='\n' || ret=='y' || ret=='Y') sim=true;
	if(ret=='s' || ret=='S') goto skip;
	
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
				printf(YEL "HVTest  " NRM " invalid value. Skipping current check.\n");
				continue;
			}
			if((ret=ApplyHV(c,(int)(R+10.5)))<0) goto err;
			//Check current
			if((ret=IVmeas(c,nullptr,nullptr,I1000+c,true))<0) goto err;
			if((ret=ApplyHV(c,0))<0) goto err;
		}
	}
	
	skip:
	printf("\n\n");
	printf(BLD "                         **************************************************\n");
	printf("                         *                 HV TEST REPORT                 *\n");
	printf("                         **************************************************\n\n" NRM);
	printf(BLD "      Parameter      Res.         Value         Reference  Note\n" NRM);
	printf("----------------------------------------------------------------------------------------------------\n");
	for(c=0;c<4;c++) {
		if(tHV[c]==false) continue;
		
		printf(" %3s-%1s        (20 V) ",lChan[c%2],lFPGA[c/2]);
		if(abs(V20[c]-20)>1 || V20var[c]>2) printf(RED "Fail" NRM);
		else printf(GRN "Pass" NRM);
		printf(" % 20d         20",V20[c]);
		if(abs(V20[c]-20)>1) {
			printf(" V mismatch");
			failmask|=FAIL_HVADC;
			if(V20var[c]>2) printf(" -");
		}
		if(V20var[c]>2) {
			printf(" Unstable HV");
			failmask|=FAIL_HVHARD;
		}
		printf("\n");
		
		int max=v4?maxhv4[c%2]:maxhv3[c%2];
		if(Vfull[c]<0 || Vfullvar[c]<0) continue;
		printf(" %3s-%1s        (Vmax) ",lChan[c%2],lFPGA[c/2]);
		if(abs(Vfull[c]-max)>5 || Vfullvar[c]>2) printf(RED "Fail" NRM);
		else printf(GRN "Pass" NRM);
		printf(" % 20d        %3d",Vfull[c],max);
		if(abs(Vfull[c]-max)>5) {
			printf(" V mismatch");
			failmask|=FAIL_HVADC;
			if(Vfullvar[c]>2) printf(" -");
		}
		if(Vfullvar[c]>2) {
			printf(" Unstable HV");
			failmask|=FAIL_HVHARD;
		}
		printf("\n");
		
		if(Ifull[c]<0) continue;
		printf(" %3s-%1s (Ioff @ Vmax) ",lChan[c%2],lFPGA[c/2]);
		if(Ifull[c]>100) printf(RED "Fail" NRM);
		else printf(GRN "Pass" NRM);
		printf(" % 20d       <100",Ifull[c]);
		if(Ifull[c]>100) {
			printf(" Bad I offset");
			failmask|=FAIL_HVADC;
		}
		printf("\n");
		
		if(I1000[c]<0) continue;
		printf(" %3s-%1s (Iref=1000nA) ",lChan[c%2],lFPGA[c/2]);
		if(abs(I1000[c]-1000)>100) printf(RED "Fail" NRM);
		else printf(GRN "Pass" NRM);
		printf(" % 20d       1000",I1000[c]);
		if(abs(I1000[c]-1000)>100) {
			printf(" Bad I reading");
			failmask|=FAIL_HVHARD;
		}
		printf("\n");
		
	}
	printf("----------------------------------------------------------------------------------------------------\n\n");
	
	//Final score
	HVScore = ComSc;
	if(fEx) HVScore += ExtSc;
	printf(BLD "\nFEE HV quality score: ");
	if(HVScore < 60) printf(RED);
	else if(HVScore < 85) printf(YEL);
	else printf(GRN);
	printf("%3.0f\n" NRM, HVScore);
	
	if(failmask&FAIL_HVADC) {
		printf("\nMeasured voltage and/or current are inconsistent with expected values. What do you want to do?\n");
		printf("    1) Ignore and go on\n");
		printf("    2) Perform HV calibration procedure (ADC only)\n");
		printf("    0) Quit the procedure, check components and restart the test\n");
		printf("[default=1]> ");
		ret=getchar();
		if(ret=='\n') ret=0x31;
		else {
			for(;getchar()!='\n';);
		}
		ret-=0x30;
		if((ret<0)||(ret>2)) ret=0;
		if(ret==0) return 0;
		if(ret==2) {
			if((ret=HVCalib())<0) return ret;
			return 0;
		}
	}
	if(failmask&FAIL_HVHARD) {
		printf("\nBad HV behaviour probably due to hardware issues. What do you want to do?\n");
		printf("    1) Ignore and go on\n");
		printf("    0) Quit the procedure, check components and restart the test\n");
		printf("[default=1]> ");
		ret=getchar();
		if(ret=='\n') ret=0x31;
		else {
			for(;getchar()!='\n';);
		}
		ret-=0x30;
		if(ret!=1) ret=0;
		if(ret==0) return 0;
	}
	
	return 0;
	err:
	printf(RED "HVTest  " NRM " test failed! Applying 0 V to all channels...\n");
	//forcing return to 0V to all channels
	for(c=0;c<4;c++) ApplyManyHV(15,0);
	return ret;
}

//Public: manual resolution
int FzTest::Manual() {
	int tmp,ret,N;
	
	if(!fInit) Init();
	do {
		printf("\nOperation menu\n");
		printf("    1) Set new serial number\n");
		printf("    2) Offset auto-calibration\n");
		printf("    3) Plot offset calibration curve\n");
		printf("    4) Offset manual test\n");
		printf("    5) HV calibration\n");
		printf("    6) HV guided test\n");
		printf("    7) HV manual test\n");
		printf("    8) Send manual SC command\n");
		printf("    9) General fast test\n");
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
				if((ret=HVCalib())<0) return ret;
				tmp=0;
				break;
			case 6:
				if((ret=HVTest())<0) return ret;
				break;
			case 7:
				if((ret=HVManual())<0) return ret;
				break;
			case 8:
				if((ret=SCManual())<0) return ret;
				break;
			case 9:
				if((ret=FastTest(true))<0) return ret;
				break;
		}
	}
	while(tmp);
	return 0;
}

//Offset calibration routine
int FzTest::OffCal() {
	int target, c, i, ch, ret, ierr, dac1, dac2, bl1, bl2, dac, Bl, tmp;
	char ctmp, query[SLENG];
	uint8_t reply[MLENG];
	int finaldac[6];
	
	printf("\nTarget baseline [-7400]> ");
	for(c=0; c<SLENG-1; c++) {
		ctmp = getchar();
		if(ctmp == '\n') break;
		query[c] = ctmp;
	}
	query[c] = '\0';
	if(c==0) target = -7400;
	else {
		target = atoi(query);
		if(target<-8192 || target>8191) {
			printf(YEL "OffCal  " NRM " invalid target. Set to -7400.\n");
			target = -7400;
		}
	}
	for(c=0; c<6; c++) {
		ch = c2ch[c];
		printf(BLD "OffCal  " NRM " %s-%s: calibrating offset...\n" UP, lADC[ch%6], lFPGA[ch/6]);
		//Reading original DAC value
		sprintf(query, "%s,%d", lFPGA[c/3], (c%3)+4);
		if((ret = sock->Send(blk, fee, 0x85, query, reply, fVerb))) return ret;
		ret = sscanf((char *)reply, "0|%d", &dac);
		if(ret != 1) return -20;
		finaldac[c] = dac;
		
		//Usually DAC should be around 700 but I prefer to start from lower values to avoid BL underflow
		dac1 = 200; dac2 = 400;
		//Try DAC=200 and check bl
		sprintf(query, "%s,%d,%d", lFPGA[c/3], (c%3)+1, dac1);
		if((ret = sock->Send(blk, fee, 0x89, query, reply, fVerb))) return ret;
		usleep(80000);
		if((ret = BLmeas(ch, 20, &bl1, nullptr)) < 0) return ret;
		//Try DAC=400 and check bl
		sprintf(query, "%s,%d,%d", lFPGA[c/3], (c%3)+1, dac2);
		if((ret = sock->Send(blk, fee, 0x89, query, reply, fVerb))) return ret;
		usleep(80000);
		if((ret = BLmeas(ch, 20, bl+ch, blvar+ch))<0) return ret;
		bl2 = bl[ch];
		dcreact[c] = bl1 - bl2;
		
		//Always check stability!
		if(bl[ch]>=8100 || bl[ch]<=-8100) {
			printf(YEL "OffCal  " NRM " %s-%s: saturating baseline\n",lADC[ch%6],lFPGA[ch/6]);
			continue;
		}
		if(blvar[ch] > 5*blvtol[ch%6]) {
			printf(YEL "OffCal  " NRM " %s-%s: unstable DC level\n",lADC[ch%6],lFPGA[ch/6]);
			continue;
		}
		if(dcreact[c] < 1000) {
			printf(YEL "OffCal  " NRM " %s-%s: DC level is not responsive\n",lADC[ch%6],lFPGA[ch/6]);
			continue;
		}
		
		//Fast search procedure
		ierr=0; Bl=10000;
		for(i=0; ; i++) {
			if(bl1 <= bl2) {
				if(dac1 <= dac2) ierr=1;
				else {
					dac  = dac1;  tmp = bl1;
					dac1 = dac2;  bl1 = bl2;
					dac2 = dac;   bl2 = tmp;
				}
			}
			dac = dac1 + (int)(0.5 + ((double)(target-bl1)) * ((double)(dac2-dac1)) / ((double)(bl2-bl1)));
			if((dac<0) || (dac>1023)) {ierr=2; break;}
			if((abs(dac2 - dac1) < 10) || (abs(Bl - target) < 20)) break;
			if((i>=5) || ierr) break;
			
			sprintf(query, "%s,%d,%d", lFPGA[c/3], (c%3)+1, dac);
			if((ret=sock->Send(blk, fee, 0x89, query, reply, fVerb))) return ret;
			usleep(80000);
			if((ret = BLmeas(ch, 10, &Bl, nullptr)) < 0) return ret;
			
			if(target < bl2) {bl1=Bl; dac1=dac;}
			else if(target < bl1) {
				if(Bl >= target) {bl2 = Bl; dac2 = dac;}
				else {bl1 = Bl; dac1 = dac;}
			}
			else {bl2 = Bl; dac2 = dac;}
		}
		if((i>=5) || ierr) {
			printf(YEL "OffCal  " NRM " %s-%s: DC level reaction is anomalous (i=%d bl1=%d bl2=%d dac1=%d dac2=%d)\n", lADC[ch%6], lFPGA[ch/6], i, bl1, bl2, dac1, dac2);
			continue;
		}
		finaldac[c] = dac;
	}
	
	for(c=0; c<6; c++) {
		ch = c2ch[c];
		sprintf(query, "%s,%d,%d", lFPGA[c/3], (c%3)+1, finaldac[c]);
		if((ret = sock->Send(blk, fee, 0x89, query, reply, fVerb))) return ret;
		usleep(80000);
		if((ret = BLmeas(ch, 10, &Bl, nullptr)) < 0) return ret;
		printf(GRN "OffCal  " NRM " %s-%s: DAC set to %4d. BL -> % 5d\n", lADC[ch%6], lFPGA[ch/6], finaldac[c], (Bl<32768)?Bl:(Bl-65536));
	}
	//Writing DAC values to PIC EEPROM
	if((ret = sock->Send(blk, fee, 0x8E, "", reply, fVerb))) return ret;
	
	//Testing new BL values
	if((ret = TestAnalog())<0) return ret;
	return 0;
}

//Offset vs applyed DC level plot routine
int FzTest::OffCurve() {
	int dac,mes,c,ret,old[6];
	char query[SLENG];
	uint8_t reply[MLENG];
	
	//Store present DAC value
	for(c=0;c<6;c++) {
		sprintf(query,"%s,%d",lFPGA[c/3],(c%3)+4);
		if((ret=sock->Send(blk,fee,0x85,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",old+c);
		if(ret!=1) return -20;
	}
	
	if(!fVerb) {
		printf(BLD "\n DAC   QH1-A   Q2-A   Q3-A  QH1-B   Q2-B   Q3-B\n" NRM);
		printf("------------------------------------------------\n");
	}
	for(dac=0;dac<1024;dac+=10) {
		for(c=0;c<6;c++) {
			sprintf(query,"%s,%d,%d",lFPGA[c/3],c%3+1,dac);
			if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
		}
		if(!fVerb) printf(BLD "%4d" NRM,dac);
		usleep(20000);
		for(c=0;c<6;c++) {
			if((ret=BLmeas(c2ch[c],10,&mes,nullptr))<0) return ret;
			if(!fVerb) printf(" % 6d",mes);
			offmatrix[c][dac/10]=mes;
		}
		if(!fVerb) printf("\n");
	}
	tCurve=true;
	
	//Set DAC to previous value
	for(c=0;c<6;c++) {
		//Set DAC to previous value
		sprintf(query,"%s,%d,%d",lFPGA[c/3],(c%3)+1,old[c]);
		if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
	}
	return 0;
}

//Offset manual test
int FzTest::OffManual() {
	int ch,c,ret;
	int old,dac,base,basel;
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
	
// 	if((ret=BLmeas(ch,3,nullptr,nullptr))<0) return ret;
// 	if(ch==1 || ch==2 || ch==4 || ch==7 || ch==8 || ch==10) return 0;
	
	//Store present DAC value
	sprintf(query,"%s,%d",lFPGA[c/3],(c%3)+4);
	if((ret=sock->Send(blk,fee,0x85,query,reply,fVerb))) return ret;
	ret=sscanf((char *)reply,"0|%d",&old);
	if(ret!=1) return -20;
	
	for(;;) {
		printf("DAC value [<0 to stop, >1023 to store] > "); scanf("%d",&dac); getchar();
		if(dac<0) break;
		if(dac>1023) {
			//Writing DAC values to PIC EEPROM
			if((ret=sock->Send(blk,fee,0x8E,"",reply,fVerb))) return ret;
			break;
		}
		sprintf(query,"%s,%d,%d",lFPGA[c/3],(c%3)+1,dac);
		if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
		usleep(80000);
		if((ret=BLmeas(ch,1,&base,nullptr))<0) return ret;
		if(ch==0 || ch==6) {
			if((ret=BLmeas(ch+2,1,&basel,nullptr))<0) return ret;
		}
		if(!fVerb) printf(UP);
		printf("DAC = " BLD "%4d" NRM " => BL(H) = %5d (",dac,base);
		for(int i=13;i>=0;i--) {
			printf("%d",(base>>i)&1);
			if(i%2==0 && i!=0) printf(" ");
		}
		printf(")");
		if(ch==0 || ch==6) {
			printf(", BL(L) = %5d (",basel);
			for(int i=13;i>=0;i--) {
				printf("%d",(basel>>i)&1);
				if(i%2==0 && i!=0) printf(" ");
			}
			printf(")\n");
		}
		else printf("                    \n");
	}
	
	//Set DAC to previous value
	sprintf(query,"%s,%d,%d",lFPGA[c/3],(c%3)+1,old);
	if((ret=sock->Send(blk,fee,0x89,query,reply,fVerb))) return ret;
	return 0;
}

//HV functions
//HV calibration!
int FzTest::HVCalib() {
	int c,ret,max;
	char query[SLENG];
	uint8_t reply[MLENG];
	bool dac=false;
	
	if(!tGeneral) {
		printf(YEL "HVCalib " NRM " fast test was not performed. Performing general check only...\n");
		if((ret=TestGeneral())<0) return ret;
	}
	
	for(c=0;c<4;c++) {
		if(hvmask&(1<<c)) printf(Mag "\n%s-%s" NRM " is already calibrated. What do you want to do?\n",lChan[c%2],lFPGA[c/2]);
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
			printf(YEL "HVCalib " NRM "Unable to calibate ADC only (DAC must be calibrated first!)\n");
			ret=1;
		}
		if(ret==1 && ksock==nullptr) {
			printf(RED "HVCalib " NRM " Keithley 2000 is not connected or configured\n");
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
		if((ret=HVCalChan(c,max,dac))<0) return ret;
		tCalib=true;
	}
	max=0;
	for(c=0;c<4;c++) {
		if(tcal[c]>=0) max+=tcal[c];
	}
	printf(GRN "HVCalib " NRM " overall calibration terminated in %2dm%02ds. " BLD "Please restart the card before applying HV!\n" NRM,max/60,max%60);
	return 0;
}

int FzTest::HVManual() {
	int ret,c,max,dac[4],vinp,dinp,V,I;
	double Vd,Id;
	bool fast=false,cal;
	char input[SLENG],query[SLENG];
	uint8_t reply[MLENG];
	
	if(!tGeneral) {
		printf(YEL "HVManual" NRM " fast test was not performed. Performing general check only...\n");
		if((ret=TestGeneral())<0) return ret;
	}
	
	printf("\nFast mode? (current readings will be not reliable)> " NRM "[Y/n]> ");
	ret=getchar();
	if(ret!='\n') {
		for(;getchar()!='\n';);
	}
	if(ret=='\n' || ret=='y' || ret=='Y') fast=true;
	
	for(c=0;c<4;c++) {
		//Set max voltage
		max=v4?maxhv4[c%2]:maxhv3[c%2];
		sprintf(query,"%s,%d,%d",lFPGA[c/2],c%2+1,max);
		if((ret=sock->Send(blk,fee,0x92,query,reply,fVerb))) goto err;
		
		//Enable HV (v4/5 only)
		if(v4) {
			sprintf(query,"%s,1",lFPGA[c/2]);
			if((ret=sock->Send(blk,fee,0xA6,query,reply,fVerb))) return ret;
		}
		
		//Cheching present DAC value
		if((dac[c]=GetDAC(c))<0) return dac[c];
	}
	
	printf("\nManual test syntax: <channel>,<value>\n");
	printf("Positive value means DAC units, negative is expressed in Volts\n");
	printf("If the channel is not calibrated, an approximated conversion V => DAC will be performed.\n");
	printf("Current readings are not reliable if DAC is directly set\ninstead of typing V in calibrated channels.\n");
	printf("Empty or invalid queries will quit this funcion.\n\n");
	
	//Test loop
	for(;;) {
		printf("List and status of available HV channels:\n");
		for(c=0;c<4;c++) printf("    %d) %s-%s [%s] DAC = %5d\n",c,lChan[c%2],lFPGA[c/2],(hvmask&(1<<c))?"calib":"uncal",dac[c]);
		printf("> ");
		ret=0;
		c=getchar();
		if(c!='\n') {
			input[0]=(char)c;
			for(ret=1;((c=getchar())!='\n')&&(ret<SLENG-1);ret++) input[ret]=(char)c;
		}
		input[ret]='\0';
		if(ret==0) break;
		ret=sscanf(input,"%d,%d",&c,&vinp);
		if(ret!=2) break;
		if((c<0)||(c>3)) {
			printf(UP YEL "Invalid channel!                             \n" NRM);
			continue;
		}
		
		//Set max variable and check limits
		max=v4?maxhv4[c%2]:maxhv3[c%2];
		if((vinp<-max) || (vinp>65535)) {
			printf(UP YEL "Invalid value!\n" NRM);
			continue;
		}
		
		if(hvmask&(1<<c)) {
			cal=true;
			if(vinp<0) {
				dinp=-1;
				vinp=-vinp;
			}
			else {
				dinp=vinp;
				vinp=-1;
			}
		}
		else {
			cal=false;
			if(vinp<0) {
				dinp=-V2D[c%2]*vinp;
				vinp=-1;
			}
			else {
				dinp=vinp;
				vinp=-1;
			}
		}
		if(vinp>=0) {
			printf(UP BLD "Setting   V =   %3d V on channel %d      \n" NRM,vinp,c);
			if((ret=ApplyHV(c,vinp))<0) goto err;
			//Cheching present DAC value
			if((dac[c]=GetDAC(c))<0) return dac[c];
			//measuring calibrated and uncalibrated readings
			if((ret=IVmeas(c,&V,nullptr,&I,!fast))<0) goto err;
			if((ret=IVADC(c,&Vd,nullptr,&Id,false))<0) goto err;
		}
		else {
			if(dinp>V2D[c%2]*max) {
				printf(UP YEL "Very high DAC value!!! Are you sure? " NRM "[y/N]> ");
				ret=getchar();
				if(ret!='\n') {
					for(;getchar()!='\n';);
				}
				if(ret=='\n' || ret=='n' || ret=='N') continue;
			}
			printf(UP BLD "Setting DAC = %5d   on channel %d      \n" NRM,dinp,c);
			if((ret=SetDAC(c,dinp))<0) goto err;
			dac[c]=dinp;
			//measuring (calibrated and) uncalibrated readings
			if(cal) {
				if((ret=IVmeas(c,&V,nullptr,&I,!fast))<0) goto err;
				if((ret=IVADC(c,&Vd,nullptr,&Id,false))<0) goto err;
			}
			else {
				if((ret=IVADC(c,&Vd,nullptr,&Id,!fast))<0) goto err;
			}
		}
	}
	ret=0;
	err:
	//forcing return to 0V to all channels
	for(c=0;c<4;c++) SetDAC(c,0);
	return ret;
}

int FzTest::SCManual() {
	int i,Blk,Fee,Com,ret;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	printf(BLD "\nBLK ID" NRM "\n 0:3583 -> blocks\n     -1 -> Regional Board\n others -> quit\n[default=%d]> ",blk);
	for(i=0;(ret=getchar())!='\n';i++) query[i]=ret;
	query[i]='\0';
	if(i==0) Blk=blk;
	else Blk=atoi(query);
	if(Blk<-1 || Blk>3583) goto err;
	if(Blk<0) {
		Blk=3584;
		Fee=0;
	}
	else {
		printf(BLD "\nFEE ID" NRM "\n 0:7 -> FEEs\n  8 -> Power Supply\n  9 -> Block Card\n others -> quit\n[default=%d]> ",fee);
		for(i=0;(ret=getchar())!='\n';i++) query[i]=ret;
		query[i]='\0';
		if(i==0) Fee=fee;
		else Fee=atoi(query);
		if(Fee<0 || Fee>9) goto err;
	}
	printf(BLD "\nCOMMAND" NRM " (HEX)\n> ");
	for(i=0;(ret=getchar())!='\n';i++) query[i]=ret;
	query[i]='\0';
	if(i==0) goto err;
	if((ret=sscanf(query,"%X",&Com))!=1) goto err;
	if(Com<0 || Com>0xFF) goto err;
	
	printf(BLD "\nARGUMENT" NRM " (ASCII)\n[default=<empty>]> ");
	for(i=0;(ret=getchar())!='\n';i++) query[i]=ret;
	query[i]='\0';
	
	printf("\nTransaction output:\n");
	sock->Send(Blk,Fee,Com,query,reply,1);
	return 0;
	
	err:
	printf(RED "SCManual" NRM "Invalid choice\n");
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
		printf("\nSN is not defined! Type a valid SN (1-65534)> ");
		scanf("%d",&N); getchar();
		if(N<=0 || N>=65535) return;
		SetSN(N);
	}
	
	sprintf(dirname,"testdb");
	if(stat(dirname,&st)<0) {
		if(mkdir(dirname,0755)<0) {
			perror(RED "UpdateDB" NRM);
			return;
		}
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
	if(tGeneral == false) goto loganalog;
	sprintf(filename,"%s/general.txt",dirname);
	f=fopen(filename,"r");
	if(f!=NULL) {
		//previous file exists
		N=0; ref.hvmask=0;
		for(;fgets(row,MLENG,f);) {
			ret=sscanf(row," %[^:]: %[^\n]",label,data);
			//ret==1 is ok, it means "untested", so I can continue because ref is initialized with "untested" values
			if(ret!=2) continue;
			if(strcmp(label,"General score")==0) {
				ref.Score=atof(data);
				if(ref.Score<0 || ref.Score>100) ref.Score=-1;
			}
			else if(strcmp(label,"HV score")==0) {
				ref.HVScore=atof(data);
				if(ref.HVScore<0 || ref.HVScore>100) ref.HVScore=-1;
			}
			else if(strcmp(label,"FEE version")==0) {
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
	fprintf(f,"# !!! NEW DB format !!!\n");
	if(ref.Score>=0 && Score<0) Score=ref.Score;
	if(ref.Score!=Score) fprintf(flog,"[%s]  general.txt        General score DB entry was updated\n",stime);
	fprintf(f,"       General score:        %3.0f\n", Score);
	
	if(ref.HVScore>=0 && HVScore<0) HVScore=ref.HVScore;
	if(ref.HVScore!=HVScore) fprintf(flog,"[%s]  general.txt             HV score DB entry was updated\n",stime);
	fprintf(f,"            HV score:        %3.0f\n", HVScore);
	
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
	
	loganalog:
	//ANALOG CHAIN INFORMATION
	if(tAnalog == false) goto logcurve;
	sprintf(filename,"%s/analog.txt",dirname);
	f=fopen(filename,"r");
	if(f!=NULL) {
		//previous file exists
		for(;fgets(row,MLENG,f);) {
			ret=sscanf(row," %[^:]: %[^\n]",label,data);
			//ret==1 is ok, it means "untested", so I can continue because ref is initialized with "untested" values
			if(ret!=2) continue;
			
			for(ch=0;ch<12;ch++) {
				c = ch2c[ch];
				sprintf(lcmp, "%s-%s offset", lADCs[ch%6], lFPGA[ch/6]);
				if(strcmp(lcmp, label) == 0) {
					N = sscanf(data, " %d %lf %d %d", ref.bl+ch, ref.blvar+ch, &tmp1, &tmp2);
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
	
	fprintf(f,"# !!! NEW DB format !!!\n");
	fprintf(f,"# First value -> measured base line level in ADC units\n");
	fprintf(f,"#Second value -> st. dev. of base line level\n");
	fprintf(f,"# Third value -> currently set DAC value (QH1, Q2 and Q3 only)\n");
	fprintf(f,"#Fourth value -> base line shift with a 200 unit variation of the DAC output (QH1, Q2 and Q3 only)\n\n");
	for(ch=0;ch<12;ch++) {
		sprintf(lcmp,"        %s-%s offset",lADC[ch%6],lFPGA[ch/6]);
		if(ref.bl[ch]>-9000 && bl[ch]<-9000) {bl[ch] = ref.bl[ch]; blvar[ch] = ref.blvar[ch];}
		else fprintf(flog,"[%s]   analog.txt %s DB entry was updated (offsets)\n",stime,lcmp);
		fprintf(f, "%s: %5d %5.1f", lcmp, bl[ch], blvar[ch]);
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
	
	logcurve:
	//OFFSET CURVE (IF TESTED, OVERWRITE PREVIOUS CURVE)
	if(tCurve == false) goto logHV;
	sprintf(filename,"%s/offcurve.txt",dirname);
	if((f=fopen(filename,"w"))==NULL) {
		perror(RED "UpdateDB" NRM);
		fclose(flog);
		return;
	}
	
	fprintf(f,"#DAC  QH1-A   Q2-A   Q3-A  QH1-B   Q2-B   Q3-B\n");
	for(N=0;N<103;N++) {
		fprintf(f,"%4d",N*10);
		for(c=0;c<6;c++) {
			fprintf(f," % 6d",offmatrix[c][N]);
		}
		fprintf(f,"\n");
	}
	fprintf(flog,"[%s] offcurve.txt DB file was updated\n",stime);
	fclose(f);
	
	logHV:
	if((tHV[0] || tHV[1] || tHV[2] || tHV[3]) == false) goto logCalib;
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
					N = sscanf(data, " %d %lf %d %lf %d %d", ref.V20+c, ref.V20var+c, ref.Vfull+c, ref.Vfullvar+c, ref.Ifull+c, ref.I1000+c);
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
	
	fprintf(f,"# !!! NEW DB format !!!\n");
	fprintf(f,"# First value -> measured HV at 20V calibrated in V\n");
	fprintf(f,"#Second value -> st. dev. of HV level at 20V\n");
	fprintf(f,"# Third value -> measured maximum HV calibrated in V\n");
	fprintf(f,"#Fourth value -> st. dev. of maximum HV level\n");
	fprintf(f,"# Fifth value -> measured current at maximum voltage without load (nA)\n");
	fprintf(f,"# Sixth value -> measured current with load (1000 nA expected)\n\n");
	for(c=0;c<4;c++) {
		sprintf(lcmp,"       %s-%s HV test",lChan[c%2],lFPGA[c/2]);
		if(ref.V20[c]>=0 && V20[c]<0) {V20[c]=ref.V20[c]; V20var[c]=ref.V20var[c];}
		else if(abs(V20[c]-ref.V20[c])>1 || fabs(V20var[c]-ref.V20var[c])>0.5) fprintf(flog,"[%s]   hvtest.txt %s DB entry was updated (V20)\n",stime,lcmp);
		fprintf(f,"%s: %3d %4.1f",lcmp,V20[c],V20var[c]);
		
		if(ref.Vfull[c]>=0 && Vfull[c]<0) {Vfull[c]=ref.Vfull[c]; Vfullvar[c]=ref.Vfullvar[c]; Ifull[c]=ref.Ifull[c];}
		else if(abs(Vfull[c]-ref.Vfull[c])>1 || fabs(Vfullvar[c]-ref.Vfullvar[c])>0.5 || abs(Ifull[c]-ref.Ifull[c])>50) fprintf(flog,"[%s]   hvtest.txt %s DB entry was updated (Vfull)\n",stime,lcmp);
		fprintf(f," %3d %4.1f %4d",Vfull[c],Vfullvar[c],Ifull[c]);
		
		if(ref.I1000[c]>=0 && I1000[c]<0) I1000[c]=ref.I1000[c];
		else if(abs(I1000[c]-ref.I1000[c])>50) fprintf(flog,"[%s]   hvtest.txt %s DB entry was updated (I1000)\n",stime,lcmp);
		fprintf(f," %4d\n",I1000[c]);
	}
	fclose(f);
	
	logCalib:
	//HV CALIBRATION DATA
	if(!tCalib) goto fine;
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

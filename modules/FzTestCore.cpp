/*******************************************************************************
*                                                                              *
*                         Simone Valdre' - 17/12/2021                          *
*                  distributed under GPL-3.0-or-later licence                  *
*                                                                              *
*******************************************************************************/


#include "FzTest.h"

//Constructor and destructor
FzTest::FzTest(FzSC *sck, const int Blk, const int Fee, const bool verbose) {
	sock  = sck;
	ksock = nullptr;
	blk   = Blk;
	fee   = Fee;
	fVerb = verbose;
	fInit = false;
}

FzTest::~FzTest() {
	if(ksock!=nullptr) delete ksock;
}

//Constructor and destructor of reference data class
FzTestRef::FzTestRef() {
	int c;
	
	Score=-1; HVScore=-1;
	v4=-1; sn=-1;
	for(c=0;c<6;c++) temp[c]=-1;
	strcpy(vPIC,"");
	for(c=0;c<2;c++) strcpy(vFPGA[c],"");
	for(c=0;c<19;c++) lv[c]=-1;
	gomask=-1; adcmask=-1;
	for(c=0;c<12;c++) {bl[c]=-10000; blvar[c]=-10000;}
	for(c=0;c<6;c++) {dacoff[c]=-1; dcreact[c]=-1;}
	hvmask=-1;
	for(c=0;c<4;c++) {
		V20[c]=-1; V20var[c]=-1; Vfull[c]=-1; Vfullvar[c]=-1; Ifull[c]=-1; I1000[c]=-1;
		for(int i=0;i<41;i++) {Vkei[c][i]=-1; Vdac[c][i]=-1; Vadc[c][i]=-1; Iadc[c][i]=-1;}
		Vp0[c]=-99; Vp1[c]=-99; Ip0[c]=-99; Ip1[c]=-99;
	}
}

FzTestRef::~FzTestRef() {
}

void FzTest::Init() {
	int c;
	
	failmask=0;
	Score=-1; HVScore=-1;
	v4=-1; sn=-1;
	for(c=0;c<6;c++) temp[c]=-1;
	strcpy(vPIC,"");
	for(c=0;c<2;c++) strcpy(vFPGA[c],"");
	for(c=0;c<19;c++) lv[c]=-1;
	gomask=-1; adcmask=-1;
	for(c=0;c<12;c++) {bl[c]=-10000; blvar[c]=-10000;}
	for(c=0;c<6;c++) {
		dacoff[c]=-1; dcreact[c]=-1;
		adcbits[c]=0;
		for(int i=0;i<103;i++) offmatrix[c][i]=-1;
	}
	hvmask=-1;
	for(c=0;c<4;c++) {
		V20[c]=-1; V20var[c]=-1; Vfull[c]=-1; Vfullvar[c]=-1; Ifull[c]=-1; I1000[c]=-1;
		for(int i=0;i<41;i++) {Vkei[c][i]=-1; Vdac[c][i]=-1; Vadc[c][i]=-1; Iadc[c][i]=-1;}
		Vp0[c]=-99; Vp1[c]=-99; Ip0[c]=-99; Ip1[c]=-99;
		tcal[c]=-1;
	}
	
	fInit    = true;
	tGeneral = false;
	tAnalog  = false;
	tCurve   = false;
	for(c=0;c<4;c++) tHV[c] = false;
	tCalib   = false;
	return;
}

//Keithley functions
//Public: configure Keithley multimeter
void FzTest::KeithleySetup(FzSC *sck) {
	int ret;
	char reply[MLENG];
	
	ksock=sck;
	if(sck==nullptr) {
		printf(RED "Keithley" NRM " socket not set!");
		return;
	}
	
	if(!fVerb) printf(BLD "Keithley" NRM " configuring device...\n");
	
	//Reset Keithley
	if((ret=ksock->KSend("*RST",nullptr,false))<0) goto err;
	sleep(1); //Immediately after reset the Keithltey is not ready to accept any command
	
	//Check if Keithley answers correctly
	if((ret=ksock->KSend("*IDN?",reply,false))<0) goto err;
	if(strncmp(reply,"KEITHLEY INSTRUMENTS INC.",25)) {
		printf(RED "KSetup  " NRM "Keithley is not ready\n");
		goto err;
	}
	
	//Settings for HV measuring
	if((ret=ksock->KSend("volt:dc:rang max",nullptr,false))<0) goto err;
	usleep(1000);
	if((ret=ksock->KSend(":Sens:Func 'volt:dc'",nullptr,false))<0) goto err;
	return;
	
	if(!fVerb) printf(UP GRN "Keithley" NRM " device configured.     \n");
	
	err:
	if(!fVerb) printf(UP RED "Keithley" NRM " configuration failed.    \n");
	delete ksock;
	ksock=nullptr;
	return;
}

//Get voltage from Keithley 2000 (wait a stable voltage, then average on 5 samples)
int FzTest::GetVoltage(double *V, double *Vvar, const bool wait) {
	int i,ret;
	char reply[MLENG];
	double tmp,old=-1,max=0,min=999;
	int dir=0,oldir=0,Nch=0,Nstab=0;
	
	if(ksock==nullptr) return -20;
	//5s timeout for voltage measurement
	for(i=0;wait && i<25;i++) {
		usleep(200000);
		if((ret=ksock->KSend(":read?",reply,false))<0) return ret;
		if(sscanf(reply,"%lg",&tmp)<1) return -20;
		tmp=fabs(tmp);
		
		if(old>=0) {
			dir=((tmp==old)?dir:((tmp>old)?1:-1));
			if(oldir!=0 && oldir!=dir) Nch++;
			if(fabs(tmp-old)<=0.01) Nstab++;
			else Nstab=0;
			//exit after 4 changes of direction or 4 stable readings (Nstab==3) in a row
			if(Nch==4 || Nstab==3) break;
		}
		oldir=dir;
		old=tmp;
	}
	
	//Precise voltage measurement
	old=0; //used as average
	dir=0; //used as sign
	for(i=0;i<5;i++) {
		usleep(200000);
		if((ret=ksock->KSend(":read?",reply,false))<0) return ret;
		if(sscanf(reply,"%lg",&tmp)<1) return -20;
		if(i==4 && tmp<0) dir=1;
		tmp=fabs(tmp);
		old+=tmp;
		if(tmp<min) min=tmp;
		if(tmp>max) max=tmp;
	}
	if(V!=nullptr) *V=old/5.;
	if(Vvar!=nullptr) *Vvar=max-min;
	return dir;
}

//Fast general checks
int FzTest::TestGeneral() {
	int c,N,ret,itmp[8];
	uint8_t reply[MLENG];
	float ftmp[3];
	
	tGeneral=false;
	printf("Performing general check...\n");
	
	//Get serial number
	if((ret=sock->Send(blk,fee,0xA5,"Q",reply,fVerb))) return ret;
	if(sscanf((char *)reply,"0|%d",&sn)!=1) return -20;
	
	//Get PIC firmware version
	if((ret=sock->Send(blk,fee,0x8B,"",reply,fVerb))) return ret;
	if(sscanf((char *)reply,"0|%d,%d,%d,V%*d",itmp,itmp+1,itmp+2)!=3) return -20;
	sprintf(vPIC,"%02d/%02d/%04d",itmp[0],itmp[1],itmp[2]);
	
	//Get FPGA firmware version
	for(c=0;c<2;c++) {
		if((ret=sock->Send(blk,fee,0x8C,lFPGA[c],reply,fVerb))) return ret;
		if(sscanf((char *)reply,"0|tel=%*c,day=%d,month=%d,year=%d,variant=%*d",itmp,itmp+1,itmp+2)!=3) return -20;
		else sprintf(vFPGA[c],"%02d/%02d/%04d",itmp[0],itmp[1],itmp[2]);
	}
	
	//Get temperatures
	if((ret=sock->Send(blk,fee,0x83,"",reply,fVerb))) return ret;
	if(sscanf((char *)reply,"0|%d,%d,%d,%d,%d,%d",temp,temp+1,temp+2,temp+3,temp+4,temp+5)<6) return -20;
	
	//Get LV values and card version
	if((ret=sock->Send(blk,fee,0x9B,"",reply,fVerb))) return ret;
	for(c=0;reply[c];c++) if(reply[c]==0x2c) reply[c]=0x2e; // ',' -> '.' (sscanf doesn't accept ',' as decimal separator)
	if(sscanf((char *)reply,"0|v:%f %f %f",ftmp,ftmp+1,ftmp+2)!=3) return -20;
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
	if(sscanf((char *)reply,"0|%d,%d,%d,%d,%d,%d,%d,%d",itmp,itmp+1,itmp+2,itmp+3,itmp+4,itmp+5,itmp+6,itmp+7)!=8) return -20;
	lv[15]=itmp[0]; lv[16]=itmp[1]; lv[14]=itmp[2]; lv[13]=itmp[3];
	lv[18]=itmp[4]; lv[17]=itmp[5]; lv[12]=itmp[6]; lv[7] =itmp[7];
	
	//HV calibration status
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
	
	tGeneral=true;
	printf(UP "                                      \n" UP);
	return 0;
}

int FzTest::TestAnalog() {
	int c, N, ret;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	tAnalog=false;
	printf("Testing pre-amplifiers\n");
	
	//Test pre-amp
	gomask=0;
	for(c=0;c<6;c++) {
		sprintf(query,"%s,%d",lFPGA[c/3],(c%3)+1);
		if((ret=sock->Send(blk,fee,0x98,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&N);
		if(ret!=1) return -20;
		if(N) gomask|=(1<<c);
	}
	
	//Test baseline offset
	for(c=0; c<12; c++) {
		printf(UP "Testing offsets %02d/12  \n", c+1);
		if((ret=OffCheck(c))<0) return ret;
	}
	
	// ADC status
	adcmask=0;
	for(c=0;c<6;c++) {
		if(blvar[c2ch[c]] < 0.1 && bl[c2ch[c]]>=-8100 && bl[c2ch[c]]<=8100) adcmask|=(1<<c);
		if(adcbits[c] != 16383) adcmask|=(1<<c);
	}
	
	tAnalog=true;
	printf(UP "                                          \n" UP);
	return 0;
}

//Set serial number
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

//Offset check and calibration functions
//Offset and ADC check routine
int FzTest::OffCheck(const int ch) {
	int c, ret = 0;
	int old, min, max;
	char query[SLENG];
	uint8_t reply[MLENG];
	const double testval[10] = {121, 60, 31, 12, 0, -1, -13, -32, -61, -122};
	int dac, base, bitup = 0, bitdn = -1;
	double m, q;
	
	//Check baseline and sigma baseline (all channels)
	if((ret = BLmeas(ch, 20, bl+ch, blvar+ch))<0) return ret;
	if(ch==1 || ch==2 || ch==4 || ch==7 || ch==8 || ch==10) return 0;
	
	//Check DAC offset and ADC readings (100MHz channels only)
	c = ch2c[ch];
	//Store present DAC value
	sprintf(query, "%s,%d", lFPGA[c/3], (c%3) + 4);
	if((ret = sock->Send(blk, fee, 0x85, query, reply, fVerb))) return ret;
	ret = sscanf((char *)reply, "0|%d", &old);
	if(ret != 1) return -20;
	//Set DAC to 200 and test BL
	sprintf(query, "%s,%d,%d", lFPGA[c/3], (c%3) + 1, 200);
	if((ret = sock->Send(blk, fee, 0x89, query, reply, fVerb))) return ret;
	usleep(100000);
	if((ret = BLmeas(ch, 10, &max, nullptr))<0) return ret;
	//Set DAC to 400 and test BL
	sprintf(query, "%s,%d,%d", lFPGA[c/3], (c%3) + 1, 400);
	if((ret = sock->Send(blk, fee, 0x89, query, reply, fVerb))) return ret;
	usleep(100000);
	if((ret = BLmeas(ch, 10, &min, nullptr)) < 0) return ret;
	dacoff[c] = old;
	dcreact[c] = max - min;
	adcbits[c] = 16383;
	if(max > 8100 || max < -8100) goto offerr;
	if(min > 8100 || min < -8100) goto offerr;
	if(abs(dcreact[c] - reacref) > reacvar) goto offerr;
	m = ((double)(min - max)) / 200.;
	q = (double)(2 * max - min);
	for(int j = 0; j < 10; j++) {
		dac = (testval[j] - q) / m;
		if(dac < 0) dac = 0;
		if(dac > 1023) dac = 1023;
		sprintf(query, "%s,%d,%d", lFPGA[c/3], (c%3) + 1, dac);
		if((ret = sock->Send(blk, fee, 0x89, query, reply, fVerb))) return ret;
		usleep(100000);
		if((ret = BLmeas(ch, 1, &base, nullptr))<0) return ret;
		bitup |= base; bitdn &= base;
		if(abs(base - (int)(testval[j])) > 100) goto offerr;
	}
	adcbits[c] = (bitup & (~bitdn)) & 16383;
	
	offerr:
	//Set DAC to previous value
	sprintf(query, "%s,%d,%d", lFPGA[c/3], (c%3) + 1, old);
	if((ret = sock->Send(blk, fee, 0x89, query, reply, fVerb))) return ret;
	return 0;
}

//BL offset measurement
int FzTest::BLmeas(const int ch, const int tries, int *Bl, double *Blvar) {
	int ret, N=0, tmp;
	double avdiff, av=0, sd=0;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	sprintf(query, "%s,0x%d", lFPGA[ch/6], 1036+100*(ch%6));
	for(int i=0; i<tries; i++) {
		usleep(1000);
		if((ret = sock->Send(blk, fee, 0x85, query, reply, fVerb))) return ret;
		ret = sscanf((char *)reply, "0|%d", &tmp);
		if(ret==1) {
			//running average and standard deviation (after tmp sign adjustment)
			tmp = (tmp<32768) ? tmp : (tmp-65536);
			avdiff = ((double)tmp - av);
			N++;
			av += avdiff / N;
			sd += avdiff * ((double)tmp - av);
		}
	}
	if(N==tries) {
		if(Bl    != nullptr) *Bl    = (int)round(av);
		if(Blvar != nullptr) {
			if(tries < 2) *Blvar = -10000;
			else          *Blvar = sqrt(sd / (N - 1));
		}
		return 0;
	}
	return -20;
}

//HV functions
//Calibration of a specific HV channel
int FzTest::HVCalChan(const int c,const int max,const bool dac) {
	int N=0,i,ret,Vtarg,Vset,Vvar,maxvar,dau,oldau=-1,dacadr,isamadr;
	double VKvar,lastV[2]={0,0},lastD[2]={0,0},bakV,bakD;
	const int cdisc[4]={0,2,1,3};
	struct timeval ti,tf,dt;
	
	switch(c) {
		case 0:  dacadr=82;         isamadr=376; break;
		case 1:  dacadr=v4?142:122; isamadr=436; break;
		case 2:  dacadr=v4?222:192; isamadr=516; break;
		case 3:  dacadr=v4?282:232; isamadr=576; break;
		default: dacadr=-1;         isamadr=-1;
	}
	
	//Setting channel as NOT calibrated
	if((ret=WriteCell(78+cdisc[c],255))<0) return ret;
	if((ret=WriteCell(50+c,255))<0) return ret;
	hvmask&=(~(1<<c));
	
	if(dac) {
		//Check polarity and wire connection
		for(;;) {
			if((ret=SetDAC(c,500))<0) return ret; //around 3.7V for Si1 and 6.0V for Si2
			//Wait for tension to go up
			usleep(500000);
			if((N=GetVoltage(&bakV,nullptr,false))<0) return ret;
			//Return to 0V
			if((ret=SetDAC(c,0))<0) return ret;
			i=0;
			if(bakV<2) {
				printf(YEL "HVCalib " NRM " No voltage measured. Please check soldering...");
				i=1;
			}
			else if(N) {
				printf(YEL "HVCalib " NRM " Inverted probe. Please swap the connectors on Keithley...");
				i=1;
			}
			if(i) {
				printf("\nUser action required: what do you want to do?\n");
				printf("    1) Wiring fixed. Go on!\n");
				printf("    0) Skip this HV channel\n");
				printf("[default=1]> ");
				
				ret=getchar();
				if(ret=='\n') ret=0x31;
				else {
					for(;getchar()!='\n';);
				}
				if(ret!=0x31) goto bad;
			}
			else break;
		}
	}
	else {
		//Force 0V to DAC output also when dac=false
		if((ret=SetDAC(c,0))<0) return ret;
	}
	//Measure time from here
	gettimeofday(&ti,NULL);
	//Wait capacitors discharge
	sleep(1);
	if(dac) {
		//Check Keithley at 0V (waiting for voltage stability)
		Vdac[c][0]=0;
		if((ret=GetVoltage(Vkei[c],nullptr,true))<0) return ret;
		if(Vkei[c][0]>=1) {
			printf(YEL "HVCalib " NRM " measured voltage is not zero (%f). Skipping" Mag " %s-%s\n" NRM,Vkei[c][0],lChan[c%2],lFPGA[c/2]);
			goto bad;
		}
	}
	
	//ADC calibration point at 0V
	if((ret=IVADC(c,Vadc[c],&Vvar,Iadc[c]))<0) goto err;
	maxvar=(int)(Vadc[c][0]/200); //0.5%
	if(maxvar<50) maxvar=50;
	if(Vvar>=maxvar) {
		printf(YEL "HVCalib " NRM " unstable HV: Vtarg=0, ADC=%5.0f, DADC=%5d\n",Vadc[c][0],Vvar);
		goto bad;
	}
	
	//Calibration points every 10V
	for(Vtarg=10,N=1;Vtarg<=max;Vtarg+=10,N++) {
		printf("\n" CYA "HVCalib " BLD "***** NEW TARGET VOLTAGE => %3d V *****\n",Vtarg);
		if(dac) {
			//DAC CALIBRATION!
			bakV=lastV[1]; bakD=lastD[1];
			for(i=0;i<20;i++) {
				//First set 1V more to have a faster current stabilization
				if(i==0) Vset=Vtarg+1;
				else Vset=Vtarg;
				//First point: guess
				if(fabs(lastV[1]-lastV[0])<0.001) {
					dau=V2D[c%2]*Vset;
					if(i==0) printf(CYA "HVCalib " NRM " guessing first DAC value...\n");
				}
				//Then: linear extrapolation
				else {
					dau=(int)(0.5+lastD[0]+(((double)Vset)-lastV[0])*(lastD[1]-lastD[0])/(lastV[1]-lastV[0]));
					if(i==0) printf(CYA "HVCalib " NRM " extrapolating from (%3.0f V: %5.0f), (%3.0f V: %5.0f)\n",lastV[0],lastD[0],lastV[1],lastD[1]);
				}
				
				if((ret=SetDAC(c,dau,oldau))<0) goto err;
				oldau=dau;
				if((ret=GetVoltage(Vkei[c]+N,&VKvar,true))<0) goto err;
				if(VKvar>=1) {
					printf(YEL "HVCalib " NRM " unstable HV: Vset=%d, VKeith=%.3f, DVKeith=%.3f\n",Vset,Vkei[c][N],VKvar);
					goto bad;
				}
				if(fabs(Vkei[c][N]-(double)Vset)>5) {
					printf(YEL "HVCalib " NRM " unexpected HV meas.: Vset=%d, VKeith=%.3f, DVKeith=%.3f\n",Vset,Vkei[c][N],VKvar);
					goto bad;
				}
				printf(CYA "HVCalib " Mag " %s-%s " NRM "       DAC set to %5d => VKeith = " BLD "%7.3f\n" NRM,lChan[c%2],lFPGA[c/2],dau,Vkei[c][N]);
				
				lastV[0]=lastV[1];   lastD[0]=lastD[1];
				lastV[1]=Vkei[c][N]; lastD[1]=(double)dau;
				
				if(fabs(Vkei[c][N]-(double)Vtarg)<1./((double)V2D[c%2])) break;
				if((abs(dau-(int)(lastD[0]))<10) && fabs(Vkei[c][N]-(double)Vtarg)<0.1) break;
// 				if(fabs(Vkei[c][N]-lastV[0])<0.05) break; //pleonastic!
			}
			if(i>=20 || fabs(Vkei[c][N]-(double)Vtarg)>=0.1) {
				printf(YEL "HVCalib " NRM " unable to reach target voltage!\n");
				goto bad;
			}
			lastV[0]=bakV; lastD[0]=bakD;
			Vdac[c][N]=(double)dau;
			
			//Write the DAC value in the correspondent PIC EEPROM cell
			for(i=0;i<2;i++) if((ret=WriteCell(dacadr+(N-1)*2+i,SELBYTE(dau,i)))<0) goto err;
		}
		else if((ret=ApplyHV(c,Vtarg))<0) goto err;
		
		//ADC calibration
		if((ret=IVADC(c,Vadc[c]+N,&Vvar,Iadc[c]+N))<0) goto err;
		maxvar=(int)(Vadc[c][N]/200); //0.5%
		if(maxvar<50) maxvar=50;
		if(Vvar>=maxvar) {
			printf(YEL "HVCalib " NRM " unstable HV: Vtarg=%d, ADC=%5.0f, DADC=%5d\n",Vtarg,Vadc[c][N],Vvar);
			goto bad;
		}
		if(v4) {
			dau=(int)(0.5+Iadc[c][N]);
			//Store I samples only in v4/5 cards
			for(i=0;i<2;i++) if((ret=WriteCell(isamadr+(N-1)*2+i,SELBYTE(dau,i)))<0) goto err;
		}
	}
	//Back to 0V
	if((ret=SetDAC(c,0))<0) return ret;
	printf("\n");
	
	//Update discrete calibration status EEPROM cell
	if((ret=WriteCell(78+cdisc[c],0))<0) return ret;
	
	//Computing linear fit coefficients and check limits
	double ii[41],p0,p1;
	int64_t cAV,cBV,cBI;
	uint64_t cAI;
	int sgI,sgV;
	
	for(i=0;i<41;i++) ii[i]=(double)i;
	if((ret=LinReg(N,Vadc[c],ii,&p1,&p0))<0) return 0;
	if(v4) {
		cAV = (int64_t)(p1*1.e7);
		cBV = labs((int64_t)(p0*1.e7));
		sgV = (p0>=0) ? 1 : 0;
		if(cBV>=(1<<24)) {
			printf(YEL "HVCalib " NRM " V coeff B is outside boundaries\n");
			return 0;
		}
	}
	else {
		if((c%2)==0) {
			cAV = (int64_t)(-p1*1.e8);
			cBV = (int64_t)(p0*1.e8);
		}
		else {
			cAV = (int64_t)(-p1*1.e7);
			cBV = (int64_t)(p0*1.e7);
		}
		sgV=1;
		if((cBV>=(1LL<<31))||(cBV<-(1LL<<31))) {
			printf(YEL "HVCalib " NRM " V coeff B is outside boundaries\n");
			return 0;
		}
	}
	if((cAV>=(1L<<15))||(cAV<-(1L<<15))) {
		printf(YEL "HVCalib " NRM " V coeff A is outside boundaries\n");
		return 0;
	}
	Vp0[c]=-p0/p1; Vp1[c]=1./(10.*p1);
	
	if((ret=LinReg(N,ii,Iadc[c],&p1,&p0))<0) return 0;
	cAI = (uint64_t)(p1*1.e3);
	cBI = labs((int64_t)p0);
	sgI = (p0>=0) ? 1 : 0;
	if(cAI>=(1LL<<32)) {
		printf(YEL "HVCalib " NRM " I coeff A is outside boundaries\n");
		return 0;
	}
	if(cBI>=(1LL<<32)) {
		printf(YEL "HVCalib " NRM " I coeff B is outside boundaries\n");
		return 0;
	}
	Ip0[c]=p0; Ip1[c]=p1/10.;
	printf(GRN "HVCalib " NRM " ADC-V = % 8.3f * V %c %6.3f\n",Vp1[c],(Vp0[c]<0)?'-':'+',fabs(Vp0[c]));
	printf(GRN "HVCalib " NRM " ADC-I = % 8.3f * V %c %6.3f\n",Ip1[c],(Ip0[c]<0)?'-':'+',fabs(Ip0[c]));
	
	for(i=0;i<2;i++) if((ret=WriteCell(54+c*6+i,SELBYTE(cAV,i)))<0) return ret;
	for(i=0;i<4;i++) {
		if(v4&&i==3) {
			if((ret=WriteCell(59+c*6,sgV))<0) return ret;
			break;
		}
		if((ret=WriteCell(56+c*6+i,SELBYTE(cBV,i)))<0) return ret;
	}
	for(i=0;i<4;i++) if((ret=WriteCell(13+c*9+i,SELBYTE(cAI,i)))<0) return ret;
	for(i=0;i<4;i++) if((ret=WriteCell(17+c*9+i,SELBYTE(cBI,i)))<0) return ret;
	if((ret=WriteCell(21+c*9,sgI))<0) return ret;
	
	//Mark the channel as calibrated
	if((ret=WriteCell(50+c,0x38))<0) return ret;
	hvmask|=(1<<c);
	
	//Calculate time needed to calibrate one channel
	gettimeofday(&tf,NULL);
	timersub(&tf,&ti,&dt);
	bakD=(double)(dt.tv_sec)+((double)(dt.tv_usec))*1.e-6;
	tcal[c]=(int)(bakD+0.5);
	printf(GRN "HVCalib " Mag " %s-%s " NRM "  calibration terminated in %2dm%02ds\n",lChan[c%2],lFPGA[c/2],tcal[c]/60,tcal[c]%60);
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

//Apply HV (use only when card is calibrated!)
int FzTest::ApplyHV(const int c,const int V) {
	int i,ret,tmp;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	//Wait HV ready: 60s timeout
	if(!fVerb) printf(YEL "ApplyHV " Mag " %s-%s" NRM " waiting HV ready...\n",lChan[c%2],lFPGA[c/2]);
	for(i=0;i<60;i++) {
		sprintf(query,"%s,%d",lFPGA[c/2],c%2+1);
		if((ret=sock->Send(blk,fee,0xA0,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&tmp);
		if(ret!=1) return -20;
		if(tmp==1) break;
		sleep(1);
	}
	if(i>=60) return -20;
	
	//Apply HV
	sprintf(query,"%s,%d,%d,%d",lFPGA[c/2],c%2+1,V,HVRAMP);
	if((ret=sock->Send(blk,fee,0x86,query,reply,fVerb))) return ret;
	
	//Wait until ramp is completed
	if(!fVerb) printf(UP BLD "ApplyHV " Mag " %s-%s" NRM " ramping...         \n",lChan[c%2],lFPGA[c/2]);
	for(i=0;i<50;i++) {
		sleep(1);
		sprintf(query,"%s,%d",lFPGA[c/2],c%2+1);
		if((ret=sock->Send(blk,fee,0xA0,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&tmp);
		if(ret!=1) return -20;
		if(tmp==1) break;
	}
	if(i==50) return -20;
	if(!fVerb) printf(UP GRN "ApplyHV " Mag " %s-%s" NRM " reached target voltage :                       %3d  V\n",lChan[c%2],lFPGA[c/2],V);
	return 0;
}

//Apply HV to multiple channels (use only when card is calibrated!)
int FzTest::ApplyManyHV(const int testmask,const int *V) {
	int c,i,ret,tmp,cnt;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	//No channel set
	if(testmask==0) return 0;
	
	//Wait HV ready: 60s timeout
	if(!fVerb) printf(YEL "ApplyMHV       waiting HV ready...\n");
	for(i=0;i<60;i++) {
		cnt=0;
		for(c=0;c<4;c++) {
			if((testmask&(1<<c))==0) continue;
			sprintf(query,"%s,%d",lFPGA[c/2],c%2+1);
			if((ret=sock->Send(blk,fee,0xA0,query,reply,fVerb))) return ret;
			ret=sscanf((char *)reply,"0|%d",&tmp);
			if(ret!=1) return -20;
			if(tmp!=1) cnt++;
			usleep(20000);
		}
		if(cnt==0) break;
		usleep(900000);
	}
	if(i>=60) return -20;
	
	//Apply HV
	for(c=0;c<4;c++) {
		if((testmask&(1<<c))==0) continue;
		sprintf(query,"%s,%d,%d,%d",lFPGA[c/2],c%2+1,V[c],HVRAMP);
		if((ret=sock->Send(blk,fee,0x86,query,reply,fVerb))) return ret;
		usleep(20000);
	}
	
	//Wait until ramp is completed
	if(!fVerb) printf(UP BLD "ApplyMHV       ramping...         \n");
	for(i=0;i<50;i++) {
		sleep(1);
		cnt=0;
		for(c=0;c<4;c++) {
			if((testmask&(1<<c))==0) continue;
			sprintf(query,"%s,%d",lFPGA[c/2],c%2+1);
			if((ret=sock->Send(blk,fee,0xA0,query,reply,fVerb))) return ret;
			ret=sscanf((char *)reply,"0|%d",&tmp);
			if(ret!=1) return -20;
			if(tmp!=1) cnt++;
			usleep(20000);
		}
		if(cnt==0) break;
	}
	if(i>=50) return -20;
	if(!fVerb) {
		printf(UP GRN "ApplyMHV" NRM "       reached target voltages:");
		for(c=0;c<4;c++) {
			if((testmask&(1<<c))==0) printf("   ---");
			else printf(BLD "   %3d" NRM "  V",V[c]);
			if(c<3) printf(",");
			else printf("\n");
		}
	}
	return 0;
}

//Measure I and V (use only when card is calibrated!)
int FzTest::IVmeas(const int c, int *V, double *Vvar, int *I, bool wait) {
	int ret,Nch=0,Nstab=0,Vtmp,Itmp,old=-1,dir=0,oldir=0;
	char query[SLENG];
	double avdiff, av=0, sd=0;
	uint8_t reply[MLENG];
	
	if(I==nullptr) wait=false;
	if(wait) {
		if(!fVerb) printf(BLD "IVmeas  " Mag " %s-%s" NRM " waiting for I stabilization...\n",lChan[c%2],lFPGA[c/2]);
		sleep(2);
	}
	else printf("\n");
	
	//60s timeout for current measurement
	for(int i=0;i<20;i++) {
		if(wait) sleep(3);
		sprintf(query,"%s,%d",lFPGA[c/2],c%2+1);
		if((ret=sock->Send(blk,fee,0x87,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&Itmp);
		if(ret!=1) return -20;
		if(!wait) break;
		if(!fVerb) printf(UP BLD "IVmeas  " Mag " %s-%s" NRM " waiting for I stabilization:                           " BLD "%5d" NRM " nA\n",lChan[c%2],lFPGA[c/2],Itmp);
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
		if(!fVerb) printf(UP GRN "IVmeas  " Mag " %s-%s" NRM "                                                ---  V  " BLD "%5d" NRM " nA\n",lChan[c%2],lFPGA[c/2],Itmp);
		return 0;
	}
	
	//voltage measurement (3 meas at 0.5s interval)
	Nch=0;
	sprintf(query,"%s,%d",lFPGA[c/2],c%2+1);
	for(int i=0;i<3;i++) {
		usleep(500000);
		if((ret=sock->Send(blk,fee,0x88,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d,%*d",&Vtmp);
		if(ret==1) {
			//running average and standard deviation (after Vtmp sign change)
			Vtmp   = -Vtmp;
			avdiff = ((double)Vtmp - av);
			Nch++;
			av += avdiff / Nch;
			sd += avdiff * ((double)Vtmp - av);
		}
	}
	if(Nch==3) {
		Vtmp = (int)(0.5+av);
		if(V    != nullptr) *V    = Vtmp;
		if(Vvar != nullptr) *Vvar = sqrt(sd / (Nch - 1));
		if(!fVerb) printf(UP GRN "IVmeas  " Mag " %s-%s" NRM "                                                " BLD "%3d" NRM "  V  " BLD "%5d" NRM " nA\n",lChan[c%2],lFPGA[c/2],Vtmp,Itmp);
		return 0;
	}
	return -20;
}

//Measure multiple I and V (use only when card is calibrated!)
int FzTest::ManyIVmeas(const int testmask, int *V, double *Vvar, int *I, bool wait) {
	int c,ret,cnt,Nch[4]={0,0,0,0},Nstab[4]={0,0,0,0},old[4]={-1,-1,-1,-1},dir[4]={0,0,0,0},oldir[4]={0,0,0,0};
	int Vtmp,Itmp[4]={0,0,0,0};
	char query[SLENG];
	double avdiff, av[4]={0,0,0,0}, sd[4]={0,0,0,0};
	uint8_t reply[MLENG];
	bool stable[4]={false, false, false, false };
	
	//No channel set
	if(testmask==0) return 0;
	
	if(I==nullptr) wait=false;
	if(wait) {
		if(!fVerb) printf(BLD "MIVmeas " NRM "   waiting for I stabilization\n");
		sleep(2);
	}
	else printf("\n");
	
	//About 60s timeout for current measurement
	for(int i=0;i<20;i++) {
		if(wait) sleep(3);
		for(c=0;c<4;c++) {
			if((testmask&(1<<c))==0) continue;
			sprintf(query,"%s,%d",lFPGA[c/2],c%2+1);
			if((ret=sock->Send(blk,fee,0x87,query,reply,fVerb))) return ret;
			ret=sscanf((char *)reply,"0|%d",Itmp+c);
			if(ret!=1) return -20;
			usleep(20000);
		}
		//Break after the reading. Current must be read before!
		if(!wait) break;
		if(!fVerb) {
			printf(UP BLD "MIVmeas " NRM "   waiting for I stabilization:");
			for(c=0;c<4;c++) {
				if((testmask&(1<<c))==0) printf("     ----");
				else printf(BLD " %5d" NRM " nA",Itmp[c]);
				if(c<3) printf(",");
				else printf("\n");
			}
		}
		cnt=0;
		for(c=0;c<4;c++) {
			if((testmask&(1<<c))==0) continue;
			if(stable[c]) continue;
			if(old[c]>=0) {
				dir[c]=((Itmp[c]==old[c])?dir[c]:((Itmp[c]>old[c])?1:-1));
				if(oldir[c]!=0 && oldir[c]!=dir[c]) Nch[c]++;
				if(abs(Itmp[c]-old[c])<=5) Nstab[c]++;
				else Nstab[c]=0;
				//I is considered stable after 3 changes of direction or 3 stable readings (Nstab==2) in a row
				if(Nch[c]==3 || Nstab[c]==2) stable[c]=true;
			}
			oldir[c]=dir[c];
			old[c]=Itmp[c];
			if(!(stable[c])) cnt++;
		}
		if(cnt==0) break;
	}
	
	//Copying to ouput pointers
	if(I!=nullptr) {
		for(c=0;c<4;c++) I[c]=Itmp[c];
	}
	if(V==nullptr) {
		if(Vvar!=nullptr) {
			for(c=0;c<4;c++) Vvar[c]=-1;
		}
		if(!fVerb) {
			printf(UP);
			for(c=0;c<4;c++) printf(GRN "MIVmeas " Mag " %s-%s" NRM "                                                ---  V  " BLD "%5d" NRM " nA\n",lChan[c%2],lFPGA[c/2],Itmp[c]);
		}
		return 0;
	}
	
	//Voltage measurement (3 meas at about 0.5s interval)
	for(c=0;c<4;c++) Nch[c]=0;
	for(int i=0;i<3;i++) {
		usleep(500000);
		for(c=0;c<4;c++) {
			if((testmask&(1<<c))==0) continue;
			sprintf(query,"%s,%d",lFPGA[c/2],c%2+1);
			if((ret=sock->Send(blk,fee,0x88,query,reply,fVerb))) return ret;
			ret=sscanf((char *)reply,"0|%d,%*d",&Vtmp);
			if(ret==1) {
				//running average and standard deviation (after Vtmp sign change)
				Vtmp   = -Vtmp;
				avdiff = ((double)Vtmp - av[c]);
				Nch[c]++;
				av[c] += avdiff / Nch[c];
				sd[c] += avdiff * ((double)Vtmp - av[c]);
			}
			usleep(20000);
		}
	}
	cnt=0;
	if(!fVerb) printf(UP);
	for(c=0;c<4;c++) {
		if((testmask&(1<<c))==0) continue;
		if(Nch[c] == 3) {
			Vtmp = (int)(0.5+av[c]);
			if(V    != nullptr) V[c]    = Vtmp;
			if(Vvar != nullptr) Vvar[c] = sqrt(sd[c] / (Nch[c] - 1));
			if(!fVerb) printf(GRN "MIVmeas " Mag " %s-%s" NRM "                                                " BLD "%3d" NRM "  V  " BLD "%5d" NRM " nA\n",lChan[c%2],lFPGA[c/2],Vtmp,Itmp[c]);
		}
		else cnt++;
	}
	if(cnt) return -20;
	return 0;
}

//Measure raw I and V (not calibrated!)
int FzTest::IVADC(const int c,double *V,int *Vvar,double *I,bool wait /*=true*/) {
	int ret,Nch=0,Nstab=0,tmp,Vmax,Vmin,old=-1,dir=0,oldir=0;
	char query[SLENG];
	uint8_t reply[MLENG];
	double av;
	
	if(I==nullptr) wait=false;
	if(wait) {
		if(!fVerb) printf(BLD "IVADC   " Mag " %s-%s" NRM " waiting for current stabilization\n",lChan[c%2],lFPGA[c/2]);
		sleep(2);
	}
	else printf("\n");
	
	//30s timeout for current stabilization
	sprintf(query,"%d",c);
	for(int i=0;wait && i<60;i++) {
		usleep(500000);
		if((ret=sock->Send(blk,fee,0x8F,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&tmp);
		if(ret!=1) return -20;
		if(!fVerb) printf(UP BLD "IVADC   " Mag " %s-%s" NRM " waiting for current stabilization: " BLD "%5d\n" NRM,lChan[c%2],lFPGA[c/2],tmp);
		if(old>=0) {
			dir=((tmp==old)?dir:((tmp>old)?1:-1));
			if(oldir!=0 && oldir!=dir) Nch++;
			if(abs(tmp-old)<=5) Nstab++;
			else Nstab=0;
			//exit after 3 changes of direction or 4 stable readings (Nstab==3) in a row
			if(Nch==3 || Nstab==3) break;
		}
		oldir=dir;
		old=tmp;
	}
	
	//Current measurement: 10 samples (ADC isn't already averaged onboard, differently from calibrated I measurement!)
	if(!fVerb) printf(UP BLD "IVADC   " Mag " %s-%s" NRM " current stabilized. Measuring current...      \n",lChan[c%2],lFPGA[c/2]);
	av=0;
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
	if(!fVerb) printf(UP BLD "IVADC   " Mag " %s-%s" NRM " current stabilized. Measuring voltage...      \n",lChan[c%2],lFPGA[c/2]);
	Vmax=0; Vmin=65535; av=0;
	sprintf(query,"%d",c+4);
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
	if(!fVerb) printf(UP GRN "IVADC   " Mag " %s-%s" NRM "   ADC readings -> " BLD "%5d" NRM " (V)       " BLD "%5d" NRM " (I)\n",lChan[c%2],lFPGA[c/2],tmp,old);
	return 0;
}

//Simple linear regression routine
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

//Set the HV DAC to a specific value (ramping at ~10 V/s)
int FzTest::SetDAC(const int c, const int vset, int vprev) {
	int N=0,ret,inc;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	//dac range: 0-65535
	if((vset<0)||(vset>65535)) return -20;
	if((vprev<0)||(vprev>65535)) {
		//previous value was unknown, trying to add 1 and reading new value
		sprintf(query,"%s,%d,+,1",lFPGA[c/2],(c%2)+1);
		if((ret=sock->Send(blk,fee,0x8D,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&vprev);
		if(ret!=1) return -20;
	}
	
	if(!fVerb) printf(BLD "SetDAC  " Mag " %s-%s" NRM " ramping...\n",lChan[c%2],lFPGA[c/2]);
	for(;vset!=vprev;) {
		inc=abs(vset-vprev);
		if(inc>2*V2D[c%2]) inc=2*V2D[c%2];
		
		if(vset>vprev) sprintf(query,"%s,%d,+,%d",lFPGA[c/2],(c%2)+1,inc);
		else sprintf(query,"%s,%d,-,%d",lFPGA[c/2],(c%2)+1,inc);
		if((ret=sock->Send(blk,fee,0x8D,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&vprev);
		if(ret!=1) return -20;
		
		usleep(50000);
		//next call is useful to keep under human control the HV (Keithley display is updated), but it is very slow!
		if(ksock) {
			if((ret=ksock->KSend(":read?",(char *)reply,false))<0) return ret;
		}
		if(++N >= 1000) break;
	}
	if(N>=1000) {
		printf(RED "SetDAC  " NRM " timeout. Vprev=%d, Vset=%d\n" NRM,vprev,vset);
		return -20;
	}
	if(!fVerb) printf(UP GRN "SetDAC  " Mag " %s-%s" NRM " reached DAC level " BLD "%5d\n" NRM,lChan[c%2],lFPGA[c/2],vset);
	return 0;
}

//Get the present HV DAC value
int FzTest::GetDAC(const int c) {
	int ret,vdac;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	//Previous value is unknown
	sprintf(query,"%s,%d,+,1",lFPGA[c/2],(c%2)+1);
	if((ret=sock->Send(blk,fee,0x8D,query,reply,fVerb))) {
		if(ret!=-5) return ret;
		else {
			//DAC was at maximum value, +1 was not allowed
			sprintf(query,"%s,%d,-,1",lFPGA[c/2],(c%2)+1);
			if((ret=sock->Send(blk,fee,0x8D,query,reply,fVerb))) return ret;
			sprintf(query,"%s,%d,+,1",lFPGA[c/2],(c%2)+1);
			if((ret=sock->Send(blk,fee,0x8D,query,reply,fVerb))) return ret;
			ret=sscanf((char *)reply,"0|%d",&vdac);
			if(ret!=1) return -20;
		}
	}
	else {
		sprintf(query,"%s,%d,-,1",lFPGA[c/2],(c%2)+1);
		if((ret=sock->Send(blk,fee,0x8D,query,reply,fVerb))) return ret;
		ret=sscanf((char *)reply,"0|%d",&vdac);
		if(ret!=1) return -20;
	}
	return vdac;
}

//EEPROM read/write functions
//Dump all the EEPROM content to a file
int FzTest::FullRead(const char *filename) {
	int ret,lastr;
	FILE *f=fopen(filename,"w");
	if(f==NULL) {
		perror(RED "FullRead" NRM);
		return -50;
	}
	
	if(!tGeneral) {
		printf(YEL "FullRead" NRM " fast test was not performed. Performing general check only...\n");
		if((ret=TestGeneral())<0) goto err;
	}
	lastr=v4?655:303;
	
	if(!fVerb) printf("\n");
	for(int i=0;i<=lastr;i++) {
		if((!fVerb)&&(i%10==0)) printf(UP BLD "FullRead" NRM " reading cell %3d/%3d\n",i,lastr);
		if((ret=ReadCell(i))<0) goto err;
		fprintf(f,"%3d %02X\n",i,ret);
	}
	if(!fVerb) printf(UP GRN "FullRead" NRM " PIC EEPROM fully read and stored!\n");
	return 0;
	
	err:
	fclose(f);
	return ret;
}

//Overwrite all the EEPROM content from a file
int FzTest::FullWrite(const char *filename) {
	int ret,i,N,lastr,add,cont;
	char row[SLENG];
	FILE *f=fopen(filename,"r");
	if(f==NULL) {
		perror(RED "FullWrit" NRM);
		return -50;
	}
	
	if(!tGeneral) {
		printf(YEL "FullWrit" NRM " fast test was not performed. Performing general check only...\n");
		if((ret=TestGeneral())<0) goto err;
	}
	lastr=v4?655:303;
	
	for(N=0;fgets(row,SLENG,f);N++);
	rewind(f);
	
	if(!fVerb) printf("\n");
	for(i=0;fgets(row,SLENG,f);i++) {
		if((!fVerb)&&(i%10==0)) printf(UP BLD "FullWrit" NRM " writing row %3d/%3d\n",i,N);
		ret=sscanf(row," %d %x ",&add,&cont);
		if((ret!=2)||(add<0)||(add>lastr)) continue;
		if((ret=WriteCell(add, cont, false))<0) goto err;
	}
	if(!fVerb) printf(UP);
	printf(GRN "FullWrit" NRM " memory overwrited. Reboot the card to apply the changes\n");
	return 0;
	
	err:
	fclose(f);
	return ret;
}

//Read a cell in EEPROM
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
	if(sscanf((char *)reply,"0|%d",&cont)<1) {
		printf(RED "ReadCell" NRM " bad reply (%s)\n", reply);
		return -20;
	}
	return cont;
}

//Write a cell in EEPROM
int FzTest::WriteCell(const int add, const int cont, const bool verb) {
	int ret;
	char query[SLENG];
	uint8_t reply[MLENG];
	
	if(add<0 || add>1023) {
		printf(RED "Write   " NRM " invalid address (%d)\n",add);
		return -20;
	}
	if(cont<0 || cont>255) {
		printf(RED "Write   " NRM " invalid content (%d)\n",cont);
		return -20;
	}
	sprintf(query,"%d,%d",add,cont);
	if((ret=sock->Send(blk,fee,0x95,query,reply,fVerb))<0) return ret;
	if(verb) printf(GRN "Write   " NRM " stored 0x%02X at address %3d\n",cont,add);
	
	return 0;
}

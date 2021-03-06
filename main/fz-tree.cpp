/*******************************************************************************
*                                                                              *
*                         Simone Valdre' - 22/12/2020                          *
*                  distributed under GPL-3.0-or-later licence                  *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <math.h>
#include <omp.h>
#include <sys/time.h>
#include <TFile.h>
#include <TTree.h>
#include <TF1.h>
#include <TGraph.h>
#include <TSpline.h>
#include <TRandom3.h>
#include <TInterpreter.h>
#include "Fzpbread.h"

#define MAXMTOT 200

//to recognize the edge, signal must be over threshold for (ns):
#define TOVER 500
//signal amplitude threshold expressed in sigma baseline units
#define THRS 2.

//pileup rejection counter thresholds
#define PDDIST 40

//tCFD window (in ns) from maximum
#define TWIN 400.

//Accuracy for secant method in zero crossing detection
#define ACCX 0.1
#define ACCY 0.01

//Qua - Tel conversion from tele[0:15]
const int quac[16]={3,2,3,2,3,2,3,2,4,1,4,1,4,1,4,1};
const int telc[16]={2,3,3,2,1,4,4,1,2,3,3,2,1,4,4,1};

const int sr[6]={10,4,4,10,4,10};
const unsigned QIidx[6] ={0,0,1,2,1,3};
const bool Isig[6]={false,true,false,false,true,false};
const double pztau[6]={3.579e-5,8.744e-5,8.744e-5,3.579e-5,1.0e-4,3.e-4};

typedef struct {
	int n;
	char com[100];
	char arg[50][100];
} frase;

void leggiriga(char *striga,frase *riga) {
	int i,w=0,st=0;
	riga->n=0;
	for(i=0;i<(int)strlen(striga);i++) {
		if(striga[i]>32&&striga[i]<127) {
			if(w==0) {w=1; st=i;}
			if(riga->n==0) riga->com[i-st]=striga[i];
			else riga->arg[riga->n-1][i-st]=striga[i];
		}
		else {
			if(w==1) {
				w=0;
				if(riga->n==0) riga->com[i-st]='\0';
				else riga->arg[riga->n-1][i-st]='\0';
				riga->n++;
			}
		}
	}
	if(w==1) {
		if(riga->n==0) riga->com[i-st]='\0';
		else riga->arg[riga->n-1][i-st]='\0';
		riga->n++;
	}
	riga->n--;
	return;
}

double secanti(TSpline3 *sp,double *t,bool pos=false) {
	double y,zc;
	double xmin=t[1];
	double xmax=t[2];
	double ymin=sp->Eval(xmin);
	double ymax=sp->Eval(xmax);
	if(ymin-ymax<ACCY) return -1;
	else {
		for(;;) {
			zc=(xmax*ymin-xmin*ymax)/(ymin-ymax);
			if(xmax-xmin<ACCX) break;
			if(!isfinite(zc)) {
				zc=-1;
				break;
			}
			y=sp->Eval(zc);
			if(fabs(y)<ACCY) break;
			if((y>0)!=pos) {
				xmin=zc; ymin=y;
			}
			else {
				xmax=zc; ymax=y;
			}
		}
	}
	return zc;
}

double tangenti(TSpline3 *sp,double *t) {
	int i;
	double zcold,zc=t[2];
	double y=sp->Eval(zc);
	for(i=0;(i<10)&&(fabs(y)>=ACCY);i++) {
		zcold=zc;
		zc-=y/sp->Derivative(zc);
		if((zc<t[1])||(zc>t[2])||(fabs(zc-zcold)<ACCX)) break;
		y=sp->Eval(zc);
	}
	if((i==10)||(zc<t[1])||(zc>t[2])) return -1;
	else return zc;
}

double cubica(double *b) {
	double p0=b[0];
	double p1=-1.8333333*b[0]+3.*b[1]-1.5*b[2]+0.33333333*b[3];
	double p2=b[0]-2.5*b[1]+2*b[2]-0.5*b[3];
	double p3=-0.16666667*b[0]+0.5*b[1]-0.5*b[2]+0.16666667*b[3];
	
	double delta0=p2*p2-3*p3*p1;
	double CCr=p2*p2*p2-4.5*p3*p2*p1+13.5*p3*p3*p0;
	double CCi=0;
	
	double A=CCr*CCr-delta0*delta0*delta0;
	if(A>=0) CCr+=sqrt(A);
	else CCi=sqrt(-A);
	double CCphi=atan2(CCi,CCr);
	double Cmod=pow(CCr*CCr+CCi*CCi,0.16666667);
	double Cphi1=0.33333333*CCphi;
	double Cphi2=0.33333333*CCphi+2.0943951;
	double Cphi3=0.33333333*CCphi+4.1887902;
	
	double Cr,Ci;
	int Nsol=0;
	double x,xtmp;
	
	Cr=Cmod*cos(Cphi1); Ci=Cmod*sin(Cphi1);
	xtmp=-(p2+Cr+(delta0*Cr)/(Cmod*Cmod))/(3*p3);
	if((fabs((Ci-(delta0*Ci)/(Cmod*Cmod))/(3*p3))<0.001)&&(xtmp>=1)&&(xtmp<=2)) {
		Nsol++;
		x=xtmp;
	}
	
	Cr=Cmod*cos(Cphi2); Ci=Cmod*sin(Cphi2);
	xtmp=-(p2+Cr+(delta0*Cr)/(Cmod*Cmod))/(3*p3);
	if((fabs((Ci-(delta0*Ci)/(Cmod*Cmod))/(3*p3))<0.001)&&(xtmp>=1)&&(xtmp<=2)) {
		Nsol++;
		x=xtmp;
	}
	
	Cr=Cmod*cos(Cphi3); Ci=Cmod*sin(Cphi3);
	xtmp=-(p2+Cr+(delta0*Cr)/(Cmod*Cmod))/(3*p3);
	if((fabs((Ci-(delta0*Ci)/(Cmod*Cmod))/(3*p3))<0.001)&&(xtmp>=1)&&(xtmp<=2)) {
		Nsol++;
		x=xtmp;
	}
	
	if(Nsol!=1) return -1;
	return x;
}

double tcfd(std::vector<double>&sq,int sr,int r,double frac,double td) {
	TSpline3 *insig;
	std::vector<double> tim;
	bool discrete=false;
	double zc=td/((double)sr);
	if(zc-(double)((int)zc)<0.001) discrete=true;
	if(!discrete) {
		tim.resize(sq.size());
		for(unsigned i=0;i<sq.size();i++) tim[i]=(double)(sr*i);
		insig=new TSpline3("insig",tim.data(),sq.data(),sq.size());
	}
	unsigned Td=(int)(td/((double)sr)+0.5);
	int s=-1;
	double t[4],b[4];
	std::vector<double> bi(sq.size());
	TSpline3 *sp;
	
	//Building a bipolar signal to apply tCFD algorithm
	zc=0;
	for(unsigned i=0;i<sq.size();i++) {
		if(i<Td) bi[i]=0;
		else {
			if(discrete) bi[i]=frac*sq[i]-sq[i-Td];
			else bi[i]=frac*insig->Eval((double)(i*sr))-insig->Eval(((double)(i*sr))-td);
			if((bi[i]>zc)&&(((int)i)>r-20/sr)) {
				zc=bi[i];
				s=i;
			}
		}
	}
	if(!discrete) insig->Delete();
	
	//Applying the digital tCFD filter to find the transition time
	if(s<0) s=r-20/sr;
	if(s<2) s=2;
	r=-1;
	for(unsigned i=s;(i<s+TWIN/sr)&&(i<sq.size());i++) {
		if(bi[i-2]>=0&&bi[i-1]<0&&bi[i]<0) {r=i-3; break;}
	}
	
	//Abort if zero crossing was not found
	if(r<0) return -1;
	
	//Interpolating the bipolar signals to find the zero crossing
	for(int i=0;i<4;i++) {
		t[i]=(double)(sr*(i+r));
		b[i]=bi[r+i];
	}
	
	zc=-1;
	// 	bool pf=false;
	//Analythic 3rd degree interpolation as first choice (faster)
	double zccub=cubica(b);
	if(zccub>0) {
		zc=(double)sr*zccub+t[0];
	}
	else {
		// 		pf=true;
		sp=new TSpline3("spline",t,b,4);
		double zcsec=secanti(sp,t);
		double zctan=tangenti(sp,t);
		sp->Delete();
		if(zcsec>0) {
			if(zctan>0) {
				if(fabs(zcsec-zctan)<10.*ACCX) zc=(zcsec+zctan)/2.;
				else zc=zcsec;
			}
			else zc=zcsec;
		}
		else {
			if(zctan>0) zc=zctan;
		}
	}
	
	return zc;
}

double trap_simp(std::vector<double> sq,unsigned R,unsigned F,int &imax) {
	double max=-1;
	long double y,out=0;
	for(unsigned i=0;i<sq.size();i++) {
		if(i>=2*R+F) y=sq[i]-sq[i-R]-sq[i-R-F]+sq[i-2*R-F];
		else if(i>=R+F) y=sq[i]-sq[i-R]-sq[i-R-F];
		else if(i>=R) y=sq[i]-sq[i-R];
		else y=sq[i];
		out+=y/((long double)R);
		
		if(out>(long double)max) {max=out; imax=(int)i;}
	}
	return max;
}

double trap_save(std::vector<double> sq,std::vector<float> &strap,unsigned R,unsigned F,int &imax) {
	double max=-1;
	long double y,out=0;
	for(unsigned i=0;i<sq.size();i++) {
		if(i>=2*R+F) y=sq[i]-sq[i-R]-sq[i-R-F]+sq[i-2*R-F];
		else if(i>=R+F) y=sq[i]-sq[i-R]-sq[i-R-F];
		else if(i>=R) y=sq[i]-sq[i-R];
		else y=sq[i];
		out+=y/((long double)R);
		
		if(out>(long double)max) {max=out; imax=(int)i;}
		strap[i]=(float)out;
	}
	return max;
}

double trap_pick_simp(std::vector<double> sq,unsigned R,unsigned F,int &imax) {
	double max=-1,spick=-1;
	long double y,out=0;
	
	if((imax<0)||(imax>=(int)(sq.size()))) return -1;
	int pick=imax;
	for(unsigned i=0;i<sq.size();i++) {
		if(i>=2*R+F) y=sq[i]-sq[i-R]-sq[i-R-F]+sq[i-2*R-F];
		else if(i>=R+F) y=sq[i]-sq[i-R]-sq[i-R-F];
		else if(i>=R) y=sq[i]-sq[i-R];
		else y=sq[i];
		out+=y/((long double)R);
		
		if(pick==(int)i) spick=out;
		if(out>(long double)max) {max=out; imax=(int)i;}
	}
	return spick;
}

double trap_pick_save(std::vector<double> sq,std::vector<float> &strap,unsigned R,unsigned F,int &imax) {
	double max=-1,spick=-1;
	long double y,out=0;
	
	int pick=imax;
	for(unsigned i=0;i<sq.size();i++) {
		if(i>=2*R+F) y=sq[i]-sq[i-R]-sq[i-R-F]+sq[i-2*R-F];
		else if(i>=R+F) y=sq[i]-sq[i-R]-sq[i-R-F];
		else if(i>=R) y=sq[i]-sq[i-R];
		else y=sq[i];
		out+=y/((long double)R);
		
		if(pick==(int)i) spick=out;
		if(out>(long double)max) {max=out; imax=(int)i;}
		strap[i]=(float)out;
	}
	return spick;
}

int ImaxParabolic(std::vector<int16_t> si,double &imax) {
	//current maximum extraction
	int16_t imax16=-8192;
	int tmi=-1;
	for(unsigned i=0;i<si.size();i++) {
		if(si[i]>imax16) {
			imax16=si[i];
			tmi=i;
		}
	}
	if(tmi<=0||tmi>=(int)(si.size())-1) {
		imax=-1;
		return -1;
	}
	//Interpolated current maximum calculation
	double y1=((double)(si[tmi-1]));
	double y2=((double)(si[tmi]));
	double y3=((double)(si[tmi+1]));
	imax=y2+0.125*(y3-y1)*(y3-y1)/(2*y2-y1-y3);
	return tmi;
}

int main(int argc,char *argv[]) {
	FILE *fin=NULL;
	if(argc>1) {
		fin=fopen(argv[1],"r");
		if(fin==NULL) perror(argv[1]);
	}
	printf("\n");
	printf("*******************************************\n");
	printf("*   FAZIA tree builder from PB raw data   *\n");
	printf("*            by Simone Valdre'            *\n");
	printf("*           v1.03  (2019-12-17)           *\n");
	printf("*******************************************\n");
	printf("\n");
	if(fin==NULL) {
		printf("Usage: fulltree <config file>\n");
		printf("       further details in the example config files (*.cfg)\n");
		printf("\n");
		return 0;
	}
	char striga[1000],dir[1000]=".",prefix[1000]="run",fnout[1000]="fout.root";
	frase riga;
	int nth=4;
	int firstrun=1,lastrun=-1;
	long long evmax=-1;
	uint16_t trigpat=0;
	float fBf[3]={1,1,1};
	float sBLw=500,sBLp=100,sBLi=100;
	unsigned fRi[4]={200,500,200,200};
	unsigned sRi[4]={200,500,200,200};
	unsigned sRf=200;
	unsigned sFl[4]={100,250,100,1000};
	unsigned sFf=50;
	unsigned sPT[4]={0,0,0,0};
	double arc_del=20,arc_frac=0.2,cfd_del=100;
	bool fPZC=false,fWave=false,fTrap=false,fTime=false;
	TRandom3 ran(0);
	
	for(;fgets(striga,1000,fin)!=NULL;) {
		leggiriga(striga,&riga);
		if(riga.n<0) continue;
		if((strcmp(riga.com,"threads")==0)&&(riga.n>0)) {
			nth=atoi(riga.arg[0]);
		}
		if((strcmp(riga.com,"dir")==0)&&(riga.n>0)) {
			strcpy(dir,riga.arg[0]);
		}
		if((strcmp(riga.com,"prefix")==0)&&(riga.n>0)) {
			strcpy(prefix,riga.arg[0]);
		}
		if((strcmp(riga.com,"runs")==0)&&(riga.n>1)) {
			firstrun=atoi(riga.arg[0]);
			lastrun=atoi(riga.arg[1]);
		}
		if((strcmp(riga.com,"evmax")==0)&&(riga.n>0)) {
			evmax=atoll(riga.arg[0]);
		}
		if((strcmp(riga.com,"trig")==0)&&(riga.n>0)) {
			trigpat=atoi(riga.arg[0]);
		}
		if((strcmp(riga.com,"out")==0)&&(riga.n>0)) {
			strcpy(fnout,riga.arg[0]);
		}
		if((strcmp(riga.com,"fQH1S")==0)&&(riga.n>0)) {
			fRi[0]=atoi(riga.arg[0]);
		}
		if((strcmp(riga.com,"fQ2S")==0)&&(riga.n>0)) {
			fRi[1]=atoi(riga.arg[0]);
		}
		if((strcmp(riga.com,"fQ3sS")==0)&&(riga.n>0)) {
			fRi[2]=atoi(riga.arg[0]);
		}
		if((strcmp(riga.com,"fQ3fS")==0)&&(riga.n>0)) {
			fRi[3]=atoi(riga.arg[0]);
		}
		if((strcmp(riga.com,"fQH1bf")==0)&&(riga.n>0)) {
			fBf[0]=(float)atoi(riga.arg[0]);
		}
		if((strcmp(riga.com,"fQ2bf")==0)&&(riga.n>0)) {
			fBf[1]=(float)atoi(riga.arg[0]);
		}
		if((strcmp(riga.com,"fQ3bf")==0)&&(riga.n>0)) {
			fBf[2]=(float)atoi(riga.arg[0]);
		}
		if((strcmp(riga.com,"sBL")==0)&&(riga.n>2)) {
			sBLw=(float)atoi(riga.arg[0]);
			sBLp=(float)atoi(riga.arg[1]);
			sBLi=(float)atoi(riga.arg[2]);
		}
		if((strcmp(riga.com,"sQH1S")==0)&&(riga.n>1)) {
			sRi[0]=atoi(riga.arg[0]);
			sFl[0]=atoi(riga.arg[1]);
			if(riga.n>2) sPT[0]=atoi(riga.arg[2]);
		}
		if((strcmp(riga.com,"sQL1S")==0)&&(riga.n>1)) {
			sRi[1]=atoi(riga.arg[0]);
			sFl[1]=atoi(riga.arg[1]);
			if(riga.n>2) sPT[1]=atoi(riga.arg[2]);
		}
		if((strcmp(riga.com,"sQ2S")==0)&&(riga.n>1)) {
			sRi[2]=atoi(riga.arg[0]);
			sFl[2]=atoi(riga.arg[1]);
			if(riga.n>2) sPT[2]=atoi(riga.arg[2]);
		}
		if((strcmp(riga.com,"sQ3sS")==0)&&(riga.n>1)) {
			sRi[3]=atoi(riga.arg[0]);
			sFl[3]=atoi(riga.arg[1]);
			if(riga.n>2) sPT[3]=atoi(riga.arg[2]);
		}
		if((strcmp(riga.com,"sQ3fS")==0)&&(riga.n>1)) {
			sRf=atoi(riga.arg[0]);
			sFf=atoi(riga.arg[1]);
			//Picking time for fast has no sense at all
		}
		if((strcmp(riga.com,"arc")==0)&&(riga.n>1)) {
			arc_del=atof(riga.arg[0]);
			arc_frac=atof(riga.arg[1]);
		}
		if((strcmp(riga.com,"cfd")==0)&&(riga.n>0)) {
			cfd_del=atof(riga.arg[0]);
		}
		if((strcmp(riga.com,"pzc")==0)&&(riga.n>0)) {
			if(strcmp(riga.arg[0],"yes")==0) fPZC=true;
		}
		if((strcmp(riga.com,"waveforms")==0)&&(riga.n>0)) {
			if(strcmp(riga.arg[0],"yes")==0) fWave=true;
		}
		if((strcmp(riga.com,"traps")==0)&&(riga.n>0)) {
			if(strcmp(riga.arg[0],"yes")==0) fTrap=true;
		}
		if((strcmp(riga.com,"timing")==0)&&(riga.n>0)) {
			if(strcmp(riga.arg[0],"yes")==0) fTime=true;
		}
	}
	fclose(fin);
	
	omp_set_num_threads(nth);
	
	printf("Creating dataset...\n");
	FzData dataset(dir,prefix,firstrun,lastrun);
	printf("\n\n");
	
	//Tree variables
	ULong64_t treeEv,fEC;
	uint16_t Mtot,fTrigRB,fRateInfo,fBlk[MAXMTOT],fQua[MAXMTOT],fTel[MAXMTOT],fTrig[MAXMTOT];
	unsigned fRun;
	uint64_t fValTag;
	int16_t fDeltaTag[MAXMTOT],tMax[5][MAXMTOT];
	float fTempRB,trgDead[1],tri[12];
	float fE[4][MAXMTOT],fBL[3][MAXMTOT],sMax[7][MAXMTOT];
	float sBL[6][MAXMTOT],sSBL[6][MAXMTOT];
	float sTARC[4][MAXMTOT],sT20[4][MAXMTOT],sT70[4][MAXMTOT];
	unsigned iW[6][MAXMTOT];
	std::vector< std::vector<int16_t> > fW[6];
	std::vector< std::vector<float> > tW[5];
	
	gInterpreter->GenerateDictionary("std::vector< std::vector<int16_t> >", "vector");
	gInterpreter->GenerateDictionary("std::vector< std::vector<float> >", "vector");
	
	TFile *fout=new TFile(fnout,"RECREATE");
	fout->cd();
	TTree *tree=new TTree("faziatree","Fazia data structure");
	//***** Analysis information *****
	tree->Branch("treeEv",&treeEv,"treeEv/l");				// Analysis internal event numbering
	tree->Branch("Mtot",&Mtot,"Mtot/s");					// Total multiplicity
	tree->Branch("fRun",&fRun,"fRun/i");					// Run number from DAQ
	
	//***** Event information *****
	tree->Branch("fEC",&fEC,"fEC/l");						// ReBo event counter (42 bits)
	tree->Branch("fValTag",&fValTag,"fValTag/l");			// ReBo validation timestamp (40 ns units)
	tree->Branch("fTrigRB",&fTrigRB,"fTrigRB/s");			// ReBo trigger bitmask
	tree->Branch("fTempRB",&fTempRB,"fTempRB/F");			// Pt100 read by ReBo
	
	//Trigger rates and dead time
	tree->Branch("fRateInfo",&fRateInfo,"fRateInfo/s");				//Rate info flag (only 1 event every 10s)
	tree->Branch("trgDead",trgDead,"trgDead[fRateInfo]/F");			//Dead Time %
	tree->Branch("trgValRate",tri,"trgValRate[fRateInfo]/F");		//Validations per second
	tree->Branch("trgRawRate",tri+1,"trgRawRate[fRateInfo]/F");		//Raw triggers per second
	tree->Branch("trg1Rate",tri+2,"trg1Rate[fRateInfo]/F");			//Trigger 1 rate (cps)
	tree->Branch("trg2Rate",tri+3,"trg2Rate[fRateInfo]/F");			//Trigger 2 rate (cps)
	tree->Branch("trg3Rate",tri+4,"trg3Rate[fRateInfo]/F");			//Trigger 3 rate (cps)
	tree->Branch("trg4Rate",tri+5,"trg4Rate[fRateInfo]/F");			//Trigger 4 rate (cps)
	tree->Branch("trg5Rate",tri+6,"trg5Rate[fRateInfo]/F");			//Trigger 5 rate (cps)
	tree->Branch("trg6Rate",tri+7,"trg6Rate[fRateInfo]/F");			//Trigger 6 rate (cps)
	tree->Branch("trg7Rate",tri+8,"trg7Rate[fRateInfo]/F");			//Trigger 7 rate (cps)
	tree->Branch("trg8Rate",tri+9,"trg8Rate[fRateInfo]/F");			//Trigger 8 rate (cps)
	tree->Branch("trgManRate",tri+10,"trgManRate[fRateInfo]/F");	//Manual trigger rate (cps)
	tree->Branch("trgExtRate",tri+11,"trgExtRate[fRateInfo]/F");	//External trigger rate (cps)
	
	//***** Particle information *****
	//Telescope ID&Tag
	tree->Branch("fBlk",fBlk,"fBlk[Mtot]/s");				// Block ID
	tree->Branch("fQua",fQua,"fQua[Mtot]/s");				// Quartet ID
	tree->Branch("fTel",fTel,"fTel[Mtot]/s");				// Telescope ID
	tree->Branch("fDeltaTag",fDeltaTag,"fDeltaTag[Mtot]/S");// Validation - local trigger delta (10 ns units)
	tree->Branch("fTrig",fTrig,"fTrig[Mtot]/s");			// Local trigger pattern
	
	//On-board shaped energies
	tree->Branch("fQH1",fE[0],"fQH1[Mtot]/F");
	tree->Branch("fQ2",fE[1],"fQ2[Mtot]/F");
	tree->Branch("fQ3slow",fE[2],"fQ3slow[Mtot]/F");
	tree->Branch("fQ3fast",fE[3],"fQ3fast[Mtot]/F");
	
	//On-board baselines
	tree->Branch("fQH1bl",fBL[0],"fQH1bl[Mtot]/F");
	tree->Branch("fQ2bl",fBL[1],"fQ2bl[Mtot]/F");
	tree->Branch("fQ3bl",fBL[2],"fQ3bl[Mtot]/F");
	
	//Off-line shaped energies (NOT pole-zero compensated) and current maxima
	tree->Branch("sQH1max",sMax[0],"sQH1max[Mtot]/F");
	tree->Branch("sI1max",sMax[1],"sI1max[Mtot]/F");
	tree->Branch("sQL1max",sMax[2],"sQL1max[Mtot]/F");
	tree->Branch("sQ2max",sMax[3],"sQ2max[Mtot]/F");
	tree->Branch("sI2max",sMax[4],"sI2max[Mtot]/F");
	tree->Branch("sQ3max",sMax[5],"sQ3max[Mtot]/F");
	tree->Branch("sQ3fast",sMax[6],"sQ3fast[Mtot]/F");
	
	//Time of trapezoidal shaper maximum
	tree->Branch("tQH1max",tMax[0],"tQH1max[Mtot]/S");
	tree->Branch("tQL1max",tMax[1],"tQL1max[Mtot]/S");
	tree->Branch("tQ2max",tMax[2],"tQ2max[Mtot]/S");
	tree->Branch("tQ3max",tMax[3],"tQ3max[Mtot]/S");
	tree->Branch("tQ3fast",tMax[4],"tQ3fast[Mtot]/S");
	
	//Base line level
	tree->Branch("sQH1bl",sBL[0],"sQH1bl[Mtot]/F");
	tree->Branch("sI1bl",sBL[1],"sI1bl[Mtot]/F");
	tree->Branch("sQL1bl",sBL[2],"sQL1bl[Mtot]/F");
	tree->Branch("sQ2bl",sBL[3],"sQ2bl[Mtot]/F");
	tree->Branch("sI2bl",sBL[4],"sI2bl[Mtot]/F");
	tree->Branch("sQ3bl",sBL[5],"sQ3bl[Mtot]/F");
	
	//Base line std. dev.
	tree->Branch("sQH1sbl",sSBL[0],"sQH1sbl[Mtot]/F");
	tree->Branch("sI1sbl",sSBL[1],"sI1sbl[Mtot]/F");
	tree->Branch("sQL1sbl",sSBL[2],"sQL1sbl[Mtot]/F");
	tree->Branch("sQ2sbl",sSBL[3],"sQ2sbl[Mtot]/F");
	tree->Branch("sI2sbl",sSBL[4],"sI2sbl[Mtot]/F");
	tree->Branch("sQ3sbl",sSBL[5],"sQ3sbl[Mtot]/F");
	
	//Number of triggers on a very fast shaped signal (for pile-up rejection)
// 	tree->Branch("nQH1trig",nTrg,"nQH1trig/s");
// 	tree->Branch("nQL1trig",nTrg+1,"nQL1trig/s");
// 	tree->Branch("nQ2trig",nTrg+2,"nQ2trig/s");
// 	tree->Branch("nQ3trig",nTrg+3,"nQ3trig/s");
	
	if(fTime) {
	//Time marks from charge signals
		tree->Branch("sQH1arc",sTARC[0],"sQH1arc[Mtot]/F");		// ARC-CFD time mark
		tree->Branch("sQH1cfd20",sT20[0],"sQH1cfd20[Mtot]/F");	// time mark at 20%
		tree->Branch("sQH1cfd70",sT70[0],"sQH1cfd70[Mtot]/F");	// time mark at 70%
		
		tree->Branch("sQL1arc",sTARC[1],"sQL1arc[Mtot]/F");		// ARC-CFD time mark
		tree->Branch("sQL1cfd20",sT20[1],"sQL1cfd20[Mtot]/F");	// time mark at 20%
		tree->Branch("sQL1cfd70",sT70[1],"sQL1cfd70[Mtot]/F");	// time mark at 70%
		
		tree->Branch("sQ2arc",sTARC[2],"sQ2arc[Mtot]/F");		// ARC-CFD time mark
		tree->Branch("sQ2cfd20",sT20[2],"sQ2cfd20[Mtot]/F");	// time mark at 20%
		tree->Branch("sQ2cfd70",sT70[2],"sQ2cfd70[Mtot]/F");	// time mark at 70%
		
		tree->Branch("sQ3arc",sTARC[3],"sQ3arc[Mtot]/F");		// ARC-CFD time mark
		tree->Branch("sQ3cfd20",sT20[3],"sQ3cfd20[Mtot]/F");	// time mark at 20%
		tree->Branch("sQ3cfd70",sT70[3],"sQ3cfd70[Mtot]/F");	// time mark at 70%
	}
	
	if(fWave||fTrap) {
		//Waveform offsets, useful to plot in interactive mode:
		//e.g.: faziatree->Draw("wQ3:Iteration$-iQ3","fBlk==4&&fQua==4&&fTel==2","l",1,47);
		tree->Branch("iQH1",iW[0],"iQH1[Mtot]/i");
		tree->Branch("iI1",iW[1],"iI1[Mtot]/i");
		tree->Branch("iQL1",iW[2],"iQL1[Mtot]/i");
		tree->Branch("iQ2",iW[3],"iQ2[Mtot]/i");
		tree->Branch("iI2",iW[4],"iI2[Mtot]/i");
		tree->Branch("iQ3",iW[5],"iQ3[Mtot]/i");
	}
	if(fWave) {
		//Waveforms
		tree->Branch("wQH1",fW);
		tree->Branch("wI1",fW+1);
		tree->Branch("wQL1",fW+2);
		tree->Branch("wQ2",fW+3);
		tree->Branch("wI2",fW+4);
		tree->Branch("wQ3",fW+5);
	}
	if(fTrap) {
		//Trapezoidal shaped signals
		tree->Branch("trapQH1",tW);
		tree->Branch("trapQL1",tW+1);
		tree->Branch("trapQ2",tW+2);
		tree->Branch("trapQ3slow",tW+3);
		tree->Branch("trapQ3fast",tW+4);
	}
	
	//Common variables
	struct timeval tstart,tend,tdelta;
	uint64_t sectot,oldsec=0;
	double usectot;
	ULong64_t glevent=0,oldev=0;
	bool fStart=true,fGo=true;
	
	//Start of analysis: measuring time!
	gettimeofday(&tstart,NULL);
	
	#pragma omp parallel
	{
		//private copies
		EventStr pev;
		ULong64_t pevent;
		std::vector<float> psMax[7],psBL[6],psSBL[6],psTARC[4],psT20[4],psT70[4];
		std::vector<int16_t> ptMax[5];
// 		std::vector<uint16_t> pntrg[4];
		std::vector<unsigned> poff[6];
		std::vector< std::vector<float> > pTrap[5];
		std::vector<double> sQ;
		double prev,tmp,imax;
		int tmi,re;
		unsigned cnt,coff[6];
		
		for(;fGo;) {
			#pragma omp critical
			{
				//Reading the entry and getting waveforms from tree
				fGo=dataset.ReadFullEvent(&pev);
				if(fGo) {
					if(!fStart) glevent++;
					if(fStart) fStart=false;
					if(oldev<glevent) {
						if((evmax>=0)&&(((long long)glevent)>evmax)) {fGo=false; glevent--;}
						//Check time every 10 events; print progress on screen every ~1s
						if(glevent%10==0) {
							gettimeofday(&tend,NULL);
							timersub(&tend,&tstart,&tdelta);
							sectot=tdelta.tv_sec;
							if(sectot>oldsec) {
								oldsec=sectot;
								usectot=(double)(tdelta.tv_usec)+1000000.*((double)(tdelta.tv_sec));
								if(tdelta.tv_usec>=500000) sectot++;
								tmp=1000000.*((double)glevent)/usectot;
								printf("\033[A\033[1m%04lu:%02lu \033[1;35mRun %06d \033[1;32mEv %9Lu\033[0;39m [%6.0f ev/s]\n",sectot/60,sectot%60,pev.run,glevent,tmp);
							}
						}
						
						//Write on disk every 10000 events
						if(glevent%10000==0) {
							fout->Write();
							fout->Purge();
						}
						oldev=glevent;
					}
					pevent=glevent;
				}
			}
			if(!fGo) break; //End of dataset or protobuf error
			if(((pev.Rtrig)&trigpat)!=trigpat) continue; //Trigger selection
			if(pev.Mtot>MAXMTOT) continue; //Unable to process events with too high Mtot
			
			for(int k=0;k<7;k++) {
				psMax[k].clear();
				if(k>=6) continue;
				coff[k]=0;
				psBL[k].clear();
				psSBL[k].clear();
				if(fWave||fTrap) poff[k].clear();
				if(fTrap&&(k<5)) {
					for(unsigned i=0;i<pTrap[k].size();i++) pTrap[k][i].clear();
					pTrap[k].resize(pev.Mtot);
				}
				if(k>=5) continue;
				ptMax[k].clear();
				if(k>=4) continue;
// 				pntrg[k].clear();
				psTARC[k].clear();
				psT20[k].clear();
				psT70[k].clear();
			}
			for(int ip=0;ip<(pev.Mtot);ip++) {
				for(int k=0;k<7;k++) {
					psMax[k].push_back(-1);
					if(k>=6) continue;
					psBL[k].push_back(-10000);
					psSBL[k].push_back(-1);
					if(fWave||fTrap) {
						poff[k].push_back(coff[k]);
						coff[k]+=(pev.wf)[k][ip].size();
					}
					if(fTrap&&(!(Isig[k]))) {
						pTrap[QIidx[k]][ip].resize((pev.wf)[k][ip].size());
						if(k==5) pTrap[4][ip].resize((pev.wf)[k][ip].size());
					}
					if(k>=5) continue;
					ptMax[k].push_back(-1);
					if(k>=4) continue;
// 					pntrg[k].push_back(0);
					psTARC[k].push_back(-1);
					psT20[k].push_back(-1);
					psT70[k].push_back(-1);
				}
				
				for(int k=0;k<6;k++) {
					unsigned tbl     = sBLw/sr[k];
					unsigned deltab  = sBLp/sr[k];
					unsigned deltabi = sBLi/sr[k];
					unsigned wsize   = (pev.wf)[k][ip].size();
					
					if(wsize<tbl) continue;
					//Saturation check and storing in sQ
					re=0;
					if(!(Isig[k])) {
						sQ.resize(wsize);
					}
					for(unsigned i=0;i<wsize;i++) {
						if(((pev.wf)[k][ip][i]<=-8192)||((pev.wf)[k][ip][i]>=8191)) {
							re=1;
							break;
						}
						if(!(Isig[k])) {
							sQ[i]=(pev.wf)[k][ip][i];
						}
					}
					if(re) continue;
					
					//Fast baseline estimation (on sBLw ns)
					psBL[k][ip]=0; psSBL[k][ip]=0;
					for(unsigned i=0;i<tbl;i++) psBL[k][ip]+=(pev.wf)[k][ip][i];
					psBL[k][ip]/=(double)tbl;
					//Fast sigma baseline estimation
					for(unsigned i=0;i<tbl;i++) psSBL[k][ip]+=((pev.wf)[k][ip][i]-psBL[k][ip])*((pev.wf)[k][ip][i]-psBL[k][ip]);
					psSBL[k][ip]=sqrt(psSBL[k][ip]/((double)(tbl-1)));
					
					if(Isig[k]) {
						tmi=ImaxParabolic((pev.wf)[k][ip],imax);
						if(tmi<(int)(tbl+deltabi)) continue;
						//Fine baseline estimation
						psBL[k][ip]=0;
						for(int i=tmi-tbl-deltabi;i<tmi-(int)deltabi;i++) psBL[k][ip]+=(pev.wf)[k][ip][i];
						psBL[k][ip]/=(double)tbl;
						//Fine sigma baseline estimation
						psSBL[k][ip]=0;
						for(int i=tmi-tbl-deltabi;i<tmi-(int)deltabi;i++) psSBL[k][ip]+=((pev.wf)[k][ip][i]-psBL[k][ip])*((pev.wf)[k][ip][i]-psBL[k][ip]);
						psSBL[k][ip]=sqrt(psSBL[k][ip]/((double)(tbl-1)));
						//Current maximum:
						psMax[k][ip]=imax-psBL[k][ip];
					}
					else {
						//Rising edge search
						re=-1; cnt=0;
						for(unsigned i=0;(i<sQ.size())&&(re<0);i++) {
							if(sQ[i]>psBL[k][ip]) cnt++;
							else cnt=0;
							if(cnt==(unsigned)(TOVER/sr[k])) re=i-TOVER/sr[k]+1; //at least TOVER ns over threshold
						}
						if(re>=(int)(tbl+deltab)) {
							//Fine baseline estimation
							psBL[k][ip]=0;
							for(unsigned i=re-tbl-deltab;i<re-deltab;i++) psBL[k][ip]+=(pev.wf)[k][ip][i];
							psBL[k][ip]/=(double)tbl;
							//Fine sigma baseline estimation
							psSBL[k][ip]=0;
							for(unsigned i=re-tbl-deltab;i<re-deltab;i++) psSBL[k][ip]+=((pev.wf)[k][ip][i]-psBL[k][ip])*((pev.wf)[k][ip][i]-psBL[k][ip]);
							psSBL[k][ip]=sqrt(psSBL[k][ip]/((double)(tbl-1)));
						}
						//Trigger count
// 						pntrg[QIidx[k]]=0; cnt=0;
// 						for(unsigned i=PDDIST/sr[k];i<sQ.size();i++) {
// 							if(sQ[i]-sQ[i-PDDIST/sr[k]]>5*psSBL[k]) cnt++;
// 							if(sQ[i]-sQ[i-PDDIST/sr[k]]<0) cnt=0;
// 							if(cnt==1) {
// 								pntrg[QIidx[k]]++;
// 								cnt++;
// 							}
// 						}
						//Baseline subtraction and pole zero correction
						if(fPZC) {
							prev=sQ[0];
							sQ[0]-=psBL[k][ip];
							for(unsigned j=1;j<sQ.size();j++) {
								tmp=(sQ[j]-prev)+pztau[k]*(prev-psBL[k][ip])+sQ[j-1];
								prev=sQ[j];
								sQ[j]=tmp;
							}
						}
						else {
							for(unsigned j=0;j<sQ.size();j++) {
								sQ[j]-=psBL[k][ip];
							}
						}
						//Energy shaping
						if(sPT[QIidx[k]]>0) {
							if(re<0) tmi=-1;
							else tmi=re+sPT[QIidx[k]];
							if(fTrap) psMax[k][ip]=trap_pick_save(sQ,pTrap[QIidx[k]][ip],sRi[QIidx[k]],sFl[QIidx[k]],tmi);
							else psMax[k][ip]=trap_pick_simp(sQ,sRi[QIidx[k]],sFl[QIidx[k]],tmi);
							if(re<0) ptMax[QIidx[k]][ip]=-1;
							else ptMax[QIidx[k]][ip]=tmi-re;
						}
						else {
							tmi=-1;
							if(fTrap) psMax[k][ip]=trap_save(sQ,pTrap[QIidx[k]][ip],sRi[QIidx[k]],sFl[QIidx[k]],tmi);
							else psMax[k][ip]=trap_simp(sQ,sRi[QIidx[k]],sFl[QIidx[k]],tmi);
							if(re<0) ptMax[QIidx[k]][ip]=-1;
							else ptMax[QIidx[k]][ip]=tmi-re;
						}
						if(k==5) {
							tmi=-1;
							if(fTrap) psMax[6][ip]=trap_save(sQ,pTrap[4][ip],sRf,sFf,tmi);
							else psMax[6][ip]=trap_simp(sQ,sRf,sFf,tmi);
							ptMax[4][ip]=tmi;
						}
						//Time marks calculation, only for large enough signals
						if(fTime&&(psMax[k][ip]/psSBL[k][ip]>THRS)&&(re>20/sr[k])) {
							psTARC[QIidx[k]][ip]=tcfd(sQ,sr[k],re,arc_frac,arc_del);
							psT20[QIidx[k]][ip]=tcfd(sQ,sr[k],re,0.2,cfd_del);
							psT70[QIidx[k]][ip]=tcfd(sQ,sr[k],re,0.7,cfd_del);
						}
					}
				}
			}
			
			//Filling the tree
			#pragma omp critical
			{
				//Event variables
				treeEv=pevent; Mtot=pev.Mtot; fRun=pev.run;
				fEC=pev.ec; fValTag=pev.Rtag; fTrigRB=pev.Rtrig;
				if(pev.Rtemp==0) fTempRB=1000;
				else if(pev.Rtemp>=32767) fTempRB=2000;
				else {
					tmp=(double)(pev.Rtemp) + ran.Uniform(-0.5,0.5);
					fTempRB=(float)(-246.192+3.39245e-2*tmp+1.99014e-7*tmp*tmp);
				}
				if(pev.Rcnorm>0) {
					fRateInfo=1;
					if((pev.Rcounters)[1]>0) trgDead[0]=((float)((pev.Rcounters)[1]-(pev.Rcounters)[0]))/((float)((pev.Rcounters)[1]));
					for(int ic=0;ic<12;ic++) tri[ic]=1e6*((float)((pev.Rcounters)[ic]))/((float)(pev.Rcnorm));
				}
				else {
					fRateInfo=0; trgDead[0]=0;
					for(int ic=0;ic<12;ic++) tri[ic]=0;
				}
				
				//Telescope variables
				for(int ip=0;ip<(pev.Mtot);ip++) {
					fBlk[ip]     = (pev.blk)[ip];
					fQua[ip]     = quac[(pev.tel)[ip]];
					fTel[ip]     = telc[(pev.tel)[ip]];
					fDeltaTag[ip]= (pev.dtag)[ip];
					fTrig[ip]    = (pev.Ttrig)[ip];
					for(int k=0;k<7;k++) {
						sMax[k][ip]=psMax[k][ip];
						if(k>=6) continue;
						sBL[k][ip]=psBL[k][ip]; sSBL[k][ip]=psSBL[k][ip];
						if(fWave||fTrap) iW[k][ip]=poff[k][ip];
						if(k>=5) continue;
						tMax[k][ip]=ptMax[k][ip];
						if(k>=4) continue;
						if(fTrig[ip]==65535) fE[k][ip]=(float)((pev.fE)[k][ip]/((double)(fRi[k])));
						else fE[k][ip]=(float)((pev.fE)[k][ip]/1000.);
						if(fTime) {
							sTARC[k][ip]=psTARC[k][ip]; sT20[k][ip]=psT20[k][ip]; sT70[k][ip]=psT70[k][ip];
						}
						if(k<3) fBL[k][ip]=fBf[k]*(float)((pev.fBL)[k][ip]);
					}
				}
				for(int k=0;(k<6)&&(fWave||fTrap);k++) {
					if(fWave) fW[k]=(pev.wf)[k];
					if(fTrap&&(k<5)) tW[k]=pTrap[k];
				}
				tree->Fill();
			}
		}
	}
	gettimeofday(&tend,NULL);
	fout->Write();
	fout->Purge();
	fout->Close();
	
	timersub(&tend,&tstart,&tdelta);
	usectot=(double)(tdelta.tv_usec)+1000000.*((double)(tdelta.tv_sec));
	printf("\033[A\033[1mEnd of analysis.\033[0;39m                             \n");
	printf("    Processed events: %9Lu\n",glevent);
	printf("        Elapsed time:   %04ld:%02ld.%03ld\033[0;39m\n",tdelta.tv_sec/60,tdelta.tv_sec%60,tdelta.tv_usec/1000);
	printf("            Avg Ev/s: %9.0f\033[0;39m\n\n",1000000.*((double)glevent)/usectot);
	
	return 0;
}


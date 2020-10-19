#ifndef FZTEST
#define FZTEST

#include "FzSC.h"

//String maximum length
#define SLENG 100
//Last versions of PIC and FPGA firmwares
#define LASTVPICV3 "28/01/2016"
#define LASTVPICV4 "13/03/2018"
#define LASTVFPGA  "19/09/2019"
//HV ramp (in V/s)
#define HVRAMP 20

//Constants
static const char lFPGA[2][2]={"A","B"};
static const char lChan[3][4]={"Si1","Si2","CsI"};
static const char lADC[6][4]={"QH1"," I1","QL1"," Q2"," I2"," Q3"};
static const int c2ch[6]={0,3,5,6,9,11};
static const int ch2c[12]={0,0,0,1,1,2,3,3,3,4,4,5};
static const char regDAC[3][7]={"0x0004","0x005","0x006"};
static const char lvlabel[19][50]={Mag "      VP5REFA - M121" NRM,Mag "      VP5REFB - M54 " NRM,Mag "         VM27 - M78 " NRM,Cya "       VM2A1A - M92 " NRM,Cya "       VM2A2A - M114" NRM,Cya "       VM2A1B - M37 " NRM,Cya "       VM2A2B - M109" NRM,Blu "         VP37 - M33 " NRM,Cya "       VP3A1A - M104" NRM,Cya "       VP3A2A - M116" NRM,Cya "       VP3A1B - M47 " NRM,Cya "       VP3A2B - M111" NRM,Blu "        VP33A - M89 " NRM,Blu "        VP33D - M85 " NRM,Blu "         VP25 - M3  " NRM,Blu "          VP1 - M119" NRM,Blu "        VP18A - M103" NRM,Blu "        VP18B - M118" NRM,Blu "        VP18D - M90 " NRM};
static const float vref[19]={5000,5000,2500,2000,2000,2000,2000,3700,3000,3000,3000,3000,3300,3300,2500,1000,1800,1800,1800};
static const char lvnotes[19][15]={"From VP12_0","From VP12_0","From VP5_5_IN","From VM27","From VM27","From VM27","From VM27","From VP5_5_IN","From VP37","From VP37","From VP37","From VP37","From VP37","From VP37","From VP5_5_IN","From VP25","From VP25","From VP25","From VP25"};
static const float blref3[6]={-7400,-5650,-4500,-7400,-5650,-7400};
static const float blref5[6]={-7400,-5100,-6150,-7400,-4250,-7400};
static const float bltoll[6]={ 0.02,  0.1,  0.1, 0.02,  0.1, 0.02};
static const float blvtol[6]={   20,  200,  200,   20,  200,   20};
static const char hvissues[4][15]={"V mismatch","Wrong I offset","I mismatch","Unstable HV"};

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

class FzTest {
public:
	FzTest(FzSC *sck, const int Blk=0, const int Fee=0, const bool verbose=false);
	~FzTest();
	
	void KeithleySetup(FzSC *sck);
	int FastTest(bool hv=false);
	void Report();
	int Guided();
	int Manual();
	
private:
	void Init();
	int SetSN(const int sn);
	int BLmeas(const int ch,const int tries,int *Bl,int *Blvar);
	int OffCheck(const int ch);
	int OffCal();
	int OffCurve();
	int WaitHVready(int c);
	int IVmeas(int c,int *V,int *I,bool wait=true);
	int ApplyHV(int c,int V);
	int HVtest();
	
	bool fTested,fVerb;
	FzSC *sock,*ksock;
	int blk,fee;
	int hvstat[5];
	int failmask;
	
	//FEE data
	int v4,sn,temp[6],lv[19],gomask,bl[12],blvar[12],hvmask,dcreact[12];
	char vPIC[11],vFPGA[2][11];
};

#endif

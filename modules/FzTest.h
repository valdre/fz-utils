/*******************************************************************************
*                                                                              *
*                         Simone Valdre' - 22/02/2021                          *
*                  distributed under GPL-3.0-or-later licence                  *
*                                                                              *
*******************************************************************************/

#ifndef FZTEST
#define FZTEST

#include "FzSC.h"
#include <time.h>

//Last versions of PIC and FPGA firmwares
#define LASTVPICV3 "01/12/2021"
#define LASTVPICV4 "13/03/2018"
#define LASTVFPGA  "19/09/2019"
//HV ramp (in V/s)
#define HVRAMP 20
//Select byte
#define SELBYTE(X,I) (((unsigned long int)X>>(8*I))&0xFF)

//Fails:
#define FAIL_TEMP        1
#define FAIL_SN          2
#define FAIL_FW          4
#define FAIL_LV          8
#define FAIL_PREAMP     16
#define FAIL_DC         32
#define FAIL_OFFSET     64
#define FAIL_ADC       128
#define FAIL_HVCALIB   256

#define FAIL_HVADC     512
#define FAIL_HVHARD   1024

//Constants
static const char lFPGA[2][2]    = {"A","B"};
static const char lChan[3][4]    = {"Si1","Si2","CsI"};
static const char lADC[6][4]     = {"QH1"," I1","QL1"," Q2"," I2"," Q3"};
static const char lADCs[6][4]    = {"QH1","I1","QL1","Q2","I2","Q3"};
static const char lADCcomp[6][4] = {"M80","M62","M45","M79","M61","M43"};
static const bool is_charge[6]   = {true, false, true, true, false, true };
static const int  c2ch[6]        = {0,3,5,6,9,11};
static const int  ch2c[12]       = {0,0,0,1,1,2,3,3,3,4,4,5};
static const char lvlabel[19][50]= {Mag "      VP5REFA - M121" NRM,Mag "      VP5REFB - M54 " NRM,Mag "         VM27 - M78 " NRM,Cya "       VM2A1A - M92 " NRM,Cya "       VM2A2A - M114" NRM,Cya "       VM2A1B - M37 " NRM,Cya "       VM2A2B - M109" NRM,Blu "         VP37 - M33 " NRM,Cya "       VP3A1A - M104" NRM,Cya "       VP3A2A - M116" NRM,Cya "       VP3A1B - M47 " NRM,Cya "       VP3A2B - M111" NRM,Blu "        VP33A - M89 " NRM,Blu "        VP33D - M85 " NRM,Blu "         VP25 - M3  " NRM,Blu "          VP1 - M119" NRM,Blu "        VP18A - M103" NRM,Blu "        VP18B - M118" NRM,Blu "        VP18D - M90 " NRM};
static const float vref[19]      = {5000,5000,2500,2000,2000,2000,2000,3700,3000,3000,3000,3000,3300,3300,2500,1000,1800,1800,1800};
static const char lvnotes[19][15]= {"From VP12_0","From VP12_0","From VP5_5_IN","From VM27","From VM27","From VM27","From VM27","From VP5_5_IN","From VP37","From VP37","From VP37","From VP37","From VP37","From VP37","From VP5_5_IN","From VP25","From VP25","From VP25","From VP25"};
//Offset constants
static const float blref3[6]     = {-7403,-5650,-4500,-7403,-5650,-7403};
static const float blref5[6]     = {-7403,-5160,-5800,-7403,-4287,-7403};
static const float bltoll[6]     = {  100,  300, 1000,  100,  300,  100};
static const float blvtol[6]     = {   20,   50,   60,   20,   50,   30};
//DC level constants
static const int dcref     =  675;
static const int dcsigma   =   16;
static const int reacref   = 2448;
static const int reacsigma =   60;
//HV constants:                 Si1     Si2
static const int maxhv3[2] ={    200,    350};
static const int maxhv4[2] ={    300,    400};
static const int    V2D[2] ={    140,     85}; //DAC units per Volt (typical value)

class FzTest {
public:
	//methods in FzTestCore.cpp:
	FzTest(FzSC *sck, const int Blk=0, const int Fee=0, const bool verbose=false);
	~FzTest();
	void KeithleySetup(FzSC *sck);
	int FullRead(const char *filename);
	int FullWrite(const char *filename);
	
	//methods in FzTestUI.cpp:
	int FastTest(bool man=false);
	int Manual();
	void UpdateDB();
	
private:
	//methods in FzTestCore.cpp:
	void Init();
	int GetVoltage(double *V, double *Vvar, const bool wait=true);
	int TestGeneral();
	int TestAnalog();
	int SetSN(const int sn);
	int OffCheck(const int ch);
	int BLmeas(const int ch, const int tries, int *Bl, double *Blvar);
	int HVCalChan(const int c, const int max, const bool dac);
	int ApplyHV(const int c, const int V);
	int ApplyManyHV(const int testmask, const int *V);
	int IVmeas(const int c, int *V, double *Vvar, int *I, bool wait=true);
	int ManyIVmeas(const int testmask, int *V, double *Vvar, int *I, bool wait=true);
	int IVADC(const int c, double *V, int *Vvar, double *I, bool wait=true);
	int LinReg(const int n, const double *x, const double *y, double *p1, double *p0);
	int SetDAC(const int c, const int vset, int vprev=-1);
	int GetDAC(const int c);
	int ReadCell(const int add);
	int WriteCell(const int add, const int cont, const bool verb = true);
	
	//methods in FzTestUI.cpp:
	int OffCal();
	int OffCurve();
	int OffManual();
	int HVTest();
	int HVCalib();
	int HVManual();
	int SCManual();
	
	//common variables
	bool fVerb, fInit;
	bool tGeneral, tAnalog, tCurve, tHV[4], tCalib;
	FzSC *sock,*ksock;
	int blk,fee;
	
	//FEE data
	int v4,sn,temp[6],lv[19],gomask,adcmask,bl[12],dacoff[6],dcreact[6],offmatrix[6][103],hvmask,V20[4],Vfull[4],Ifull[4],I1000[4];
	char vPIC[11],vFPGA[2][11];
	double Score, HVScore, blvar[12], V20var[4],Vfullvar[4],Vkei[4][41],Vdac[4][41],Vadc[4][41],Iadc[4][41],Vp0[4],Vp1[4],Ip0[4],Ip1[4];
	int failmask,tcal[4];
};


//Constructor and destructor in FzTestCore.cpp
class FzTestRef {
public:
	FzTestRef();
	~FzTestRef();
	
	//FEE data
	int v4,sn,temp[6],lv[19],gomask,adcmask,bl[12],dacoff[6],dcreact[6],hvmask,V20[4],Vfull[4],Ifull[4],I1000[4];
	char vPIC[11],vFPGA[2][11];
	double Score, HVScore, blvar[12], V20var[4],Vfullvar[4],Vkei[4][41],Vdac[4][41],Vadc[4][41],Iadc[4][41],Vp0[4],Vp1[4],Ip0[4],Ip1[4];
};

#endif

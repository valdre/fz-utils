#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <fstream>
#include "FzEventSet.pb.h"

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>

#define E_NOTREADY -10
#define E_CRCLENERR -9
#define E_TELRANGE  -3
#define E_FEERANGE  -2
#define E_BLKRANGE  -1

#define WAV_QH1 0
#define WAV_I1  1
#define WAV_QL1 2
#define WAV_Q2  3
#define WAV_I2  4
#define WAV_Q3  5

#define EN_QH1    0
#define EN_Q2     1
#define EN_Q3     2
#define EN_Q3fast 3

typedef struct {
	int run;
	uint64_t ec,Rtag;
	uint16_t Rtrig,Rtemp,Rcounters[12];
	uint32_t Rcnorm;
	int Mtot;
	std::vector<int> blk,tel;
	std::vector<uint64_t> dettag,gttag;
	std::vector<uint16_t> Ttrig;
	std::vector<double> fE[4];
	std::vector<double> fBL[3];
	std::vector< std::vector<int16_t> > wf[6];
} EventStr;

class FzData {
public:
	FzData(const std::string dir,const std::string prefix="run",const int firstrun=1,const int lastrun=-1);
	~FzData();
	
	bool NextEvent();
	bool GetEvent(DAQ::FzEvent& ev);
	
	int GetRunNumber();
	bool GetEventInfo(uint64_t &ecfull,uint64_t &valtag,uint16_t &trig);
	int GetBlkID(int ib);
	int GetBlkID();
	int GetTelID(int ib,int ife,int it);
	int GetTelID();
	bool GetTimeStamps(uint64_t &dettag,uint64_t &valtag,uint16_t &trigpat);
	int GetMultiplicity();
	int NextGoodTel();
	bool GetWaveform(int wave,std::vector<int16_t> &w);
	bool GetWaveforms(std::vector<int16_t> *w);
	bool GetEnergies(double *en,double *bl);
	
	void ClearEvent(EventStr *ev);
	bool ReadFullEvent(EventStr *ev);
private:
	bool read_ok,ev_ready;
	std::string Dir,Prefix;
	int frun,lrun,crun;
	DIR *current_run;
	std::fstream current_file;
	
	DAQ::FzEventSet evset;
	int iev;
	int iblk,ifee,iwav,BLK,FEE,WAV;
	
	bool CheckFile(struct dirent *ent);
	bool NextFile();
	
	bool parse_delimited(std::istream& stream, DAQ::FzEventSet *message);
};

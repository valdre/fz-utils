#include "Fzpbread.h"

//********* Public functions

FzData::FzData(const std::string dir,const std::string prefix,const int firstrun,const int lastrun) {
	// Verify that the version of the library that we linked against is
	// compatible with the version of the headers we compiled against.
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	read_ok=true;
	ev_ready=false;
	frun=0; lrun=0; crun=0;
	iev=0;
	
	Dir=dir;
	Prefix=prefix;
	
	iblk=0; ifee=0; iwav=0;
	BLK=-1; FEE=-1; WAV=-1;
	
	struct dirent *ent;
	int minrun=999999,maxrun=0;
	int run,N=0;
	if((current_run=opendir(dir.c_str()))!=NULL) {
		while((ent=readdir(current_run))!=NULL) {
// 			if(ent->d_type!=DT_DIR) continue;
			for(unsigned i=0;i<strlen(ent->d_name);i++) {
				if(i>=prefix.length()) {
					if(sscanf(ent->d_name+i,"%06d",&run)==1) {
						if(run<minrun) minrun=run;
						if(run>maxrun) maxrun=run;
						N++;
					}
					break;
				}
				if(prefix.c_str()[i]!=ent->d_name[i]) break;
			}
		}
		closedir(current_run);
	}
	else {
		perror("FzData");
		read_ok=false;
	}
	current_run=NULL;
	if(read_ok) {
		bool limit=false;
		frun=minrun;
		lrun=maxrun;
		if(minrun<firstrun) {
			frun=firstrun;
			limit=true;
		}
		if((maxrun>lastrun)&&(lastrun>0)) {
			lrun=lastrun;
			limit=true;
		}
		printf("\033[1;32mFzData\033[0;39m: reading from %s\n",dir.c_str());
		printf("\033[1;32mFzData\033[0;39m: \033[1m%6d\033[0m runs found   (from run %06d to run %06d)\n",N,minrun,maxrun);
		if(limit) printf("\033[1;32mFzData\033[0;39m: \033[1;33mlimiting\033[0;39m to interval from run %06d to run %06d\n",frun,lrun);
		if((N>0)&&(lrun>=frun)) {
			crun=frun;
			char dirname[1000];
			sprintf(dirname,"%s/%s%06d",dir.c_str(),prefix.c_str(),crun);
			current_run=opendir(dirname);
		}
	}
}

FzData::~FzData() {
	google::protobuf::ShutdownProtobufLibrary();
	if(current_file.is_open()) current_file.close();
	if(current_run!=NULL) closedir(current_run);
}

bool FzData::NextEvent() {
	iblk=0; ifee=0; iwav=0;
	BLK=-1; FEE=-1; WAV=-1;
	if(ev_ready) {
		iev++;
		if(iev>=evset.ev_size()) ev_ready=false;
		else return true;
	}
	while(!(current_file.is_open())) {
		if(!(NextFile())) break;
	}
	if(!read_ok) {
		ev_ready=false;
		return false;
	}
	while(parse_delimited(current_file,&evset)!=current_file.good()) {
		if(!(NextFile())) break;
	}
	if(!read_ok) {
		ev_ready=false;
		return false;
	}
	iev=0;
	ev_ready=true;
	return true;
}

bool FzData::GetEvent(DAQ::FzEvent& ev) {
	if(ev_ready) ev=evset.ev(iev);
	return ev_ready;
}

int FzData::GetRunNumber() {
	return crun;
}

bool FzData::GetEventInfo(uint64_t &ecfull,uint64_t &valtag,uint16_t &trig) {
	if(!ev_ready) return E_NOTREADY;
	ecfull=(uint64_t)(evset.ev(iev).ec());
	valtag=0; trig=0;
	bool fEC=false,fGT=false,fTrig=false;
	for(int i=0;i<evset.ev(iev).trinfo_size();i++) {
		if(evset.ev(iev).trinfo(i).id()==269) {
			valtag=evset.ev(iev).trinfo(i).value();
			fGT=true;
		}
		if(evset.ev(iev).trinfo(i).id()==270) {
			ecfull=evset.ev(iev).trinfo(i).value();
			fEC=true;
		}
		if(evset.ev(iev).trinfo(i).id()==271) {
			trig=evset.ev(iev).trinfo(i).value();
			fTrig=true;
		}
		if(fEC&&fGT&&fTrig) break;
	}
	return true;
}

int FzData::GetBlkID(int ib) {
	if(!ev_ready) return E_NOTREADY;
	if(ib<evset.ev(iev).block_size()) return evset.ev(iev).block(ib).blkid();
	return -1;
}

int FzData::GetBlkID() {
	if(!ev_ready) return E_NOTREADY;
	return evset.ev(iev).block(iblk).blkid();
}

//TelID from 0 to 15 (2*FEE + TEL)
int FzData::GetTelID(int ib,int ife,int it) {
	if(!ev_ready) return E_NOTREADY;
	int fee;
	int iwav=6*it;
	if(ib<evset.ev(iev).block_size()) {
		if(evset.ev(iev).block(ib).len_error() || evset.ev(iev).block(ib).crc_error()) return -9;
		if(ife<evset.ev(iev).block(ib).fee_size()) {
			fee=evset.ev(iev).block(ib).fee(ife).feeid();
		}
		else return -2;
	}
	else return -1;
	if(it<0||it>1||iwav>=evset.ev(iev).block(ib).fee(ife).hit_size()) return -3;
	return 2*fee+evset.ev(iev).block(ib).fee(ife).hit(iwav).telid();
}

int FzData::GetTelID() {
	if(!ev_ready) return E_NOTREADY;
	return 2*evset.ev(iev).block(iblk).fee(ifee).feeid()+evset.ev(iev).block(iblk).fee(ifee).hit(iwav).telid();
}

bool FzData::GetTimeStamps(uint64_t &dettag,uint64_t &valtag,uint16_t &trigpat) {
	if(!ev_ready) return false;
	dettag=(uint64_t)(evset.ev(iev).block(iblk).fee(ifee).hit(iwav).dettag());
	valtag=(uint64_t)(evset.ev(iev).block(iblk).fee(ifee).hit(iwav).gttag());
	trigpat=(uint16_t)(evset.ev(iev).block(iblk).fee(ifee).hit(iwav).trigpat());
	return true;
}

int FzData::GetMultiplicity() {
	int M=0;
	int bs=evset.ev(iev).block_size();
	for(int ib=0;ib<bs;ib++) {
		int fs=evset.ev(iev).block(ib).fee_size();
		if(evset.ev(iev).block(ib).len_error() || evset.ev(iev).block(ib).crc_error()) fs=0;
		for(int ic=0;ic<fs;ic++) {
			int ws=evset.ev(iev).block(ib).fee(ic).hit_size();
			if((ws<0)||(ws%6!=0)) ws=0;
			M+=(ws/6);
		}
	}
	return M;
}

//return values:
// neg -> end of file or error;
//   0 -> ok, same event;
// pos -> ok, new event.
int FzData::NextGoodTel() {
	bool allok=true;
	bool new_ev=false;
	iwav+=6;
	while((iwav>=WAV)||(WAV%6!=0)) {
		ifee++;
		while(ifee>=FEE) {
			iblk++;
			while(iblk>=BLK) {
				if((allok=NextEvent())) {
					new_ev=true;
					iblk=0;
					BLK=evset.ev(iev).block_size();
				}
				else break;
			}
			if(allok) {
				ifee=0;
				FEE=evset.ev(iev).block(iblk).fee_size();
				if(evset.ev(iev).block(iblk).len_error() || evset.ev(iev).block(iblk).crc_error()) FEE=0;
			}
			else break;
		}
		if(allok) {
			iwav=0;
			WAV=evset.ev(iev).block(iblk).fee(ifee).hit_size();
		}
		else break;
	}
	if(!allok) return -1;
	if(new_ev) return 1;
	return 0;
}

bool FzData::GetWaveform(int wave,std::vector<int16_t>&w) {
	int N;
	bool allok=true;
	for(int i=0;i<6;i++) {
		if(evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).type()==wave) {
			if(evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).has_waveform()) {
				if(evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).waveform().len_error()==0) {
					N=evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).waveform().sample_size();
					w.resize(N);
					for(int j=0;j<N;j++) {
						w[j]=evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).waveform().sample(j);
						if(w[j]>8191) w[j]|=0xC000;
					}
				}
				else allok=false;
			}
			else allok=false;
			break;
		}
	}
	return allok;
}

bool FzData::GetWaveforms(std::vector<int16_t> *w) {
	int N;
	bool allok=true;
	for(int i=0;i<6;i++) {
		if(evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).has_waveform()) {
			if(evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).waveform().len_error()==0) {
				N=evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).waveform().sample_size();
				w[i].resize(N);
				for(int j=0;j<N;j++) {
					w[i][j]=evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).waveform().sample(j);
					if(w[i][j]>8191) w[i][j]|=0xC000;
				}
			}
			else allok=false;
		}
		else allok=false;
	}
	return allok;
}

bool FzData::GetEnergies(double *en,double *bl) {
	bool allok=true;
	int wave,idx,N;
	for(int i=0;i<4;i++) en[i]=-1;
	for(int i=0;i<6;i++) {
		wave=evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).type();
		switch(wave) {
			case 0:
				idx=0;
				break;
			case 3:
				idx=1;
				break;
			case 5:
				idx=2;
				break;
			default:
				idx=-1;
		}
		if(idx<0) continue;
		if(evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).has_energy()) {
			if(evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).energy().len_error()==0) {
				N=evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).energy().value_size();
				if(N<1) continue;
				if(wave==5&&N<2) continue;
				for(int j=0;j<N;j++) {
					en[idx+j]=(double)(evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).energy().value(j));
					if(en[idx+j]<0) en[idx+j]=0;
				}
			}
			else allok=false;
		}
		else allok=false;
		if(evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).has_baseline()) {
			bl[idx]=(double)(evset.ev(iev).block(iblk).fee(ifee).hit(iwav+i).data(0).baseline());
			if(bl[idx]<-8192.||bl[idx]>8191.) bl[idx]=-10000;
		}
		else bl[idx]=-10000;
	}
	return allok;
}

void FzData::ClearEvent(EventStr *ev) {
	if(ev==nullptr) return;
	(ev->blk).clear();
	(ev->tel).clear();
	(ev->dettag).clear();
	(ev->gttag).clear();
	(ev->Ttrig).clear();
	for(int i=0;i<4;i++) (ev->fE)[i].clear();
	for(int i=0;i<3;i++) (ev->fBL)[i].clear();
	for(int i=0;i<6;i++) {
		for(unsigned j=0;j<(ev->wf)[i].size();j++) {
			(ev->wf)[i][j].clear();
		}
		(ev->wf)[i].clear();
	}
	return;
}

bool FzData::ReadFullEvent(EventStr *ev) {
	if(ev==nullptr) return true; //false è riservato ad errori di lettura
	int newev=0;
	if(!ev_ready) newev=NextGoodTel();
	if(newev<0) return false;
	
	ClearEvent(ev);
	ev->run=crun;
	ev->ec=(uint64_t)(evset.ev(iev).ec());
	ev->Rtag=0; ev->Rtrig=0; ev->Rtemp=32767; ev->Rcnorm=0;
	for(int i=0;i<12;i++) ev->Rcounters[i]=0;
	for(int i=0;i<evset.ev(iev).trinfo_size();i++) {
		switch(evset.ev(iev).trinfo(i).id()) {
			case 256: case 257: case 258: case 259: case 260: case 261: case 262: case 263: case 264: case 265: case 266: case 267:
				ev->Rcounters[evset.ev(iev).trinfo(i).id()-256] = evset.ev(iev).trinfo(i).value();
				break;
			case 268: ev->Rcnorm = evset.ev(iev).trinfo(i).value(); break;
			case 269: ev->Rtag   = evset.ev(iev).trinfo(i).value(); break;
			case 270: ev->ec     = evset.ev(iev).trinfo(i).value(); break;
			case 271: ev->Rtrig  = evset.ev(iev).trinfo(i).value(); break;
			case 272: ev->Rtemp  = evset.ev(iev).trinfo(i).value();
		}
	}
	
	uint64_t gtmsb=((ev->Rtag)>>15);
	ev->Mtot=0;
	bool fOk=true;
	double en[4],bl[3];
	std::vector<int16_t> wf[6];
	for(;;) {
		(ev->blk).push_back(evset.ev(iev).block(iblk).blkid());
		(ev->tel).push_back(2*evset.ev(iev).block(iblk).fee(ifee).feeid()+evset.ev(iev).block(iblk).fee(ifee).hit(iwav).telid());
		uint64_t dettag=(uint64_t)(evset.ev(iev).block(iblk).fee(ifee).hit(iwav).dettag());
		uint64_t valtag=(uint64_t)(evset.ev(iev).block(iblk).fee(ifee).hit(iwav).gttag());
		if((dettag>=16384)&&(valtag<16384)) {
			valtag+=32768;
			if(gtmsb>0) gtmsb--;
		}
		(ev->dettag).push_back(dettag+(gtmsb<<15));
		(ev->gttag).push_back(valtag+(gtmsb<<15));
		
		if(evset.ev(iev).block(iblk).fee(ifee).hit(iwav).has_trigpat()) (ev->Ttrig).push_back(evset.ev(iev).block(iblk).fee(ifee).hit(iwav).trigpat());
		else (ev->Ttrig).push_back(65535);
		
		fOk=GetEnergies(en,bl);
		for(int i=0;;i++) {
			if(!fOk) en[i]=-1;
			(ev->fE)[i].push_back(en[i]);
			if(i==3) break;
			(ev->fBL)[i].push_back(bl[i]);
		}
		
		fOk=GetWaveforms(wf);
		for(int i=0;i<6;i++) {
			if(!fOk) wf[i].clear();
			(ev->wf)[i].push_back(wf[i]);
		}
		(ev->Mtot)++;
		
		newev=NextGoodTel();
		if(newev) break;
	}
	if(newev<0) return false;
	return true;
}


//********* Private functions

bool FzData::CheckFile(struct dirent *ent) {
	if(ent->d_type!=DT_REG) return false;
	if(strlen(ent->d_name)<3) return false;
	if(strcmp(ent->d_name+(strlen(ent->d_name)-3),".pb")!=0) return false;
	return true;
}

bool FzData::NextFile() {
	char fname[1000];
	if((!read_ok)||(current_run==NULL)) {
		ev_ready=false;
		return false;
	}
	struct dirent *ent=NULL;
	for(;;) {
		ent=readdir(current_run);
		while(ent==NULL) {
			if(current_run!=NULL) closedir(current_run);
			crun++;
			if(crun>lrun) {
				current_run=NULL;
				ev_ready=false;
				read_ok=false;
				break;
			}
			sprintf(fname,"%s/%s%06d",Dir.c_str(),Prefix.c_str(),crun);
			current_run=opendir(fname);
			if(current_run==NULL) continue;
			ent=readdir(current_run);
		}
		if(crun>lrun) break;
		if(CheckFile(ent)) break;
	}
	if((crun>lrun)||(ent==NULL)) return false;
	if(current_file.is_open()) current_file.close();
	sprintf(fname,"%s/%s%06d/%s",Dir.c_str(),Prefix.c_str(),crun,ent->d_name);
	current_file.open(fname,std::ios::in|std::ios::binary);
	return true;
}

//Copy-pasted from Gennaro's read-fazia-pb
bool FzData::parse_delimited(std::istream& stream, DAQ::FzEventSet *message) {
	uint32_t messageSize = 0;
	{
		// Read the message size.
		google::protobuf::io::IstreamInputStream istreamWrapper(&stream, sizeof(uint32_t));
		google::protobuf::io::CodedInputStream codedIStream(&istreamWrapper);
		
		// Don't consume more than sizeof(uint32_t) from the stream.
		google::protobuf::io::CodedInputStream::Limit oldLimit = codedIStream.PushLimit(sizeof(uint32_t));
		codedIStream.ReadLittleEndian32(&messageSize);
		codedIStream.PopLimit(oldLimit);
		//assert(messageSize > 0);
		if (messageSize <= 0) return stream.eof();
		//assert(istreamWrapper.ByteCount() == sizeof(uint32_t));
	}
	{
		// Read the message.
		google::protobuf::io::IstreamInputStream istreamWrapper(&stream, messageSize);
		google::protobuf::io::CodedInputStream codedIStream(&istreamWrapper);
		
		// Read the message, but don't consume more than messageSize bytes from the stream.
		google::protobuf::io::CodedInputStream::Limit oldLimit = codedIStream.PushLimit(messageSize);
		message->ParseFromCodedStream(&codedIStream);
		codedIStream.PopLimit(oldLimit);
		//assert(istreamWrapper.ByteCount() == messageSize);
	}
	return stream.good();
}

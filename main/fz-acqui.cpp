/*******************************************************************************
*                                                                              *
*                         Simone Valdre' - 22/12/2020                          *
*                  distributed under GPL-3.0-or-later licence                  *
*                                                                              *
*******************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/un.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <netinet/in.h>
// #include <errno.h>
#include <sys/file.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <TFile.h>
#include <TTree.h>
#include <TInterpreter.h>
#include <TRandom3.h>

#define DEBUG false
#define DUMP false

#define MAXMTOT		200

//Qua - Tel conversion from tele[0:15]
const int quac[16]={3,2,3,2,3,2,3,2,4,1,4,1,4,1,4,1};
const int telc[16]={2,3,3,2,1,4,4,1,2,3,3,2,1,4,4,1};

//FSM states
//COMMON
#define IDLE		  0
#define EC			  1
//FRONTEND
#define TELHDR		  2
#define VALTAG		  3
#define TELTAG		  4
#define TRGPAT		  5
#define DETHDR		  6
#define FEELEN		  7
#define FEECRC		  8
//NEW FORMAT
#define TAGHDR		 10
#define TAGLEN		 11
#define TAGDAT		 12
#define PADDIN		 13
//OLD FORMAT
#define OLDATA		 20
//BLOCK CARD
#define BLKHDR		102
#define BLKLEN		103
#define BLKCRC		104


typedef struct {
	uint8_t detid,wave;
	uint16_t pretrig;
	uint16_t fastrise;
	int32_t fasten;
	uint16_t slowrise;
	int32_t slowen;
	float baseline;
	std::vector<int16_t> wf;
	//RB info
	float tmpt;
	int32_t norm;
	uint32_t gtmsb,ecmsb;
	uint16_t counters[12],rbtrigpat,cent_frame[7];
} wav;

typedef struct {
	int telid;
	int gttag,dettag;
	int trgpat;
	std::vector<wav> w;
} tel;

typedef struct {
	int blkid;
	std::vector<tel> t;
} block;

typedef struct {
	int ec;
	std::vector<block> b;
} event;

int go=1;

uint64_t treeEv=0,Tot=0,fEC,fGTTagRB,fCentTS[1];
uint32_t fCentEN[1];
uint16_t Mtot,fTrigRB,fRateInfo,fCentInfo,fCentStat[1],fBlk[MAXMTOT],fQua[MAXMTOT],fTel[MAXMTOT],fTrig[MAXMTOT];
int16_t fDeltaTag[MAXMTOT];
float fTempRB,trgDead[1],tri[12],fQH1[MAXMTOT],fQ2[MAXMTOT],fQ3slow[MAXMTOT],fQ3fast[MAXMTOT],fQH1bl[MAXMTOT],fQ2bl[MAXMTOT],fQ3bl[MAXMTOT];
uint16_t wQH1pre[MAXMTOT],wI1pre[MAXMTOT],wQL1pre[MAXMTOT],wQ2pre[MAXMTOT],wI2pre[MAXMTOT],wQ3pre[MAXMTOT];
uint32_t iQH1[MAXMTOT],iI1[MAXMTOT],iQL1[MAXMTOT],iQ2[MAXMTOT],iI2[MAXMTOT],iQ3[MAXMTOT];
bool bQH1[MAXMTOT],bI1[MAXMTOT],bQL1[MAXMTOT],bQ2[MAXMTOT],bI2[MAXMTOT],bQ3[MAXMTOT];
std::vector< std::vector<int16_t> > wQH1,wI1,wQL1,wQ2,wI2,wQ3;

void sighandler(int sig) {
	go=0;
	printf("\033[1;34m[ SIGNAL ]\033[0;39m fz-acqui will stop at the end of next event (received %d)...\n",sig);
	return;
}

void clearwav(wav &dt) {
	dt.wf.clear();
	return;
}

void cleartel(tel &tl) {
	for(unsigned i=0;i<tl.w.size();i++) clearwav(tl.w[i]);
	tl.w.clear();
	return;
}

void clearblock(block &bl) {
	for(unsigned i=0;i<bl.t.size();i++) cleartel(bl.t[i]);
	bl.t.clear();
	return;
}

void clearevent(event &ev) {
	for(unsigned i=0;i<ev.b.size();i++) clearblock(ev.b[i]);
	ev.b.clear();
	return;
}

void clearvec(std::vector< std::vector<int16_t> > &vec) {
	for(unsigned i=0;i<vec.size();i++) {
		vec[i].clear();
	}
	vec.clear();
	return;
}

void WriteEv(event *ev,TTree *tree) {
	int irb=-1;
	wav *dt=nullptr;
	uint64_t gtmsb=0,ecmsb=0;
	int gttag=0,dettag=0;
	unsigned off[6];
	//uint16_t cent_crc;
	
	//Looking for RB fake block
	//Searching from the last element because RB fake block is usually the last
	for(int ib=((int)(ev->b.size()))-1;ib>=0;ib--) {
		if(ev->b[ib].blkid==0x7FF) {irb=ib; break;}
	}
	
	Mtot=0;
	fEC=(uint64_t)(ev->ec);
	fGTTagRB=0; fTrigRB=0; fRateInfo=0; trgDead[0]=0; fTempRB=2000;
	fCentInfo=0; fCentStat[0]=0; fCentTS[0]=0; fCentEN[0]=0;
	for(int ic=0;ic<12;ic++) tri[ic]=0;
	
	if(irb>=0) {
		dt=&(ev->b[irb].t[0].w[0]);
		if(dt->norm<0) fRateInfo=0;
		else {
			fRateInfo=1;
			trgDead[0]=((float)(dt->counters[1]-dt->counters[0]))/((float)(dt->counters[1]));
			for(int ic=0;ic<12;ic++) tri[ic]=37.5e6*((float)(dt->counters[ic]))/((float)(dt->norm));
		}
		ecmsb=(uint64_t)(dt->ecmsb);
		gtmsb=(uint64_t)(dt->gtmsb);
		fGTTagRB=(gtmsb<<15)+ev->b[irb].t[0].gttag;
		fTrigRB=dt->rbtrigpat;
		fTempRB=dt->tmpt;
		fEC+=(ecmsb<<12);
		
		fCentStat[0]=((dt->cent_frame[0])>>12);
		if(fCentStat[0]&4) {
			fCentInfo=1;
			fCentTS[0]=(((uint64_t)((dt->cent_frame[0])&0x3f))<<42)+(((uint64_t)(dt->cent_frame[1]))<<27)+(((uint64_t)(dt->cent_frame[2]))<<12)+(((uint64_t)((dt->cent_frame[3])&0xfff8))>>3);
			fCentEN[0]=(((uint32_t)((dt->cent_frame[3])&0x7))<<29)+(((uint32_t)(dt->cent_frame[4]))<<14)+(((uint32_t)((dt->cent_frame[5])&0xfffe))>>1);
			//cent_crc=(((uint16_t)((dt->cent_frame[5])&1))<<15)+(uint16_t)(dt->cent_frame[6]);
		}
	}
	
	for(int i=0;i<6;i++) off[i]=0;
	clearvec(wQH1); clearvec(wI1); clearvec(wQL1); clearvec(wQ2); clearvec(wI2); clearvec(wQ3);
	
	for(unsigned ib=0;ib<ev->b.size();ib++) {
		if(ev->b[ib].blkid==0x7FF) continue;
		fBlk[Mtot]=ev->b[ib].blkid;
		for(unsigned it=0;it<ev->b[ib].t.size();it++) {
			fQua[Mtot]=quac[ev->b[ib].t[it].telid];
			fTel[Mtot]=telc[ev->b[ib].t[it].telid];
			
			gttag=ev->b[ib].t[it].gttag;
			dettag=ev->b[ib].t[it].dettag;
			if(dettag-gttag>=16384) gttag+=32768;
			fDeltaTag[Mtot]=(int16_t)(gttag-dettag);
			fTrig[Mtot]=ev->b[ib].t[it].trgpat;
			
			fQH1[Mtot]=0; fQH1bl[Mtot]=8192.;
			fQ2[Mtot]=0; fQ2bl[Mtot]=8192.;
			fQ3slow[Mtot]=0; fQ3fast[Mtot]=0; fQ3bl[Mtot]=8192.;
			
			wQH1pre[Mtot]=0; wI1pre[Mtot]=0; wQL1pre[Mtot]=0; wQ2pre[Mtot]=0; wI2pre[Mtot]=0; wQ3pre[Mtot]=0;
			iQH1[Mtot]=0; iI1[Mtot]=0; iQL1[Mtot]=0; iQ2[Mtot]=0; iI2[Mtot]=0; iQ3[Mtot]=0;
			bQH1[Mtot]=false; bI1[Mtot]=false; bQL1[Mtot]=false; bQ2[Mtot]=false; bI2[Mtot]=false; bQ3[Mtot]=false;
			
			for(unsigned iw=0;iw<ev->b[ib].t[it].w.size();iw++) {
				int ww=ev->b[ib].t[it].w[iw].detid*10+ev->b[ib].t[it].w[iw].wave;
				int e[2];
				e[0]=ev->b[ib].t[it].w[iw].slowen;
				if(e[0]>=0x20000000) e[0]-=0x40000000;
				e[1]=ev->b[ib].t[it].w[iw].fasten;
				if(e[1]>=0x20000000) e[1]-=0x40000000;
				switch(ww) {
					case  0:
						fQH1[Mtot]=((float)(e[0]))/((float)(ev->b[ib].t[it].w[iw].slowrise));
						fQH1bl[Mtot]=ev->b[ib].t[it].w[iw].baseline;
						
						wQH1pre[Mtot]=ev->b[ib].t[it].w[iw].pretrig;
						wQH1.push_back(ev->b[ib].t[it].w[iw].wf);
						iQH1[Mtot]=off[0]; bQH1[Mtot]=true;
						off[0]+=ev->b[ib].t[it].w[iw].wf.size();
						break;
					case  1:
						wI1pre[Mtot]=ev->b[ib].t[it].w[iw].pretrig;
						wI1.push_back(ev->b[ib].t[it].w[iw].wf);
						iI1[Mtot]=off[1]; bI1[Mtot]=true;
						off[1]+=ev->b[ib].t[it].w[iw].wf.size();
						break;
					case 2:
						wQL1pre[Mtot]=ev->b[ib].t[it].w[iw].pretrig;
						wQL1.push_back(ev->b[ib].t[it].w[iw].wf);
						iQL1[Mtot]=off[2]; bQL1[Mtot]=true;
						off[2]+=ev->b[ib].t[it].w[iw].wf.size();
						break;
					case 10:
						fQ2[Mtot]=((float)(e[0]))/((float)(ev->b[ib].t[it].w[iw].slowrise));
						fQ2bl[Mtot]=ev->b[ib].t[it].w[iw].baseline;
						
						wQ2pre[Mtot]=ev->b[ib].t[it].w[iw].pretrig;
						wQ2.push_back(ev->b[ib].t[it].w[iw].wf);
						iQ2[Mtot]=off[3]; bQ2[Mtot]=true;
						off[3]+=ev->b[ib].t[it].w[iw].wf.size();
						break;
					case 11:
						wI2pre[Mtot]=ev->b[ib].t[it].w[iw].pretrig;
						wI2.push_back(ev->b[ib].t[it].w[iw].wf);
						iI2[Mtot]=off[4]; bI2[Mtot]=true;
						off[4]+=ev->b[ib].t[it].w[iw].wf.size();
						break;
					case 20:
						fQ3slow[Mtot]=((float)(e[0]))/((float)(ev->b[ib].t[it].w[iw].slowrise));
						fQ3fast[Mtot]=((float)(e[1]))/((float)(ev->b[ib].t[it].w[iw].fastrise));
						fQ3bl[Mtot]=ev->b[ib].t[it].w[iw].baseline;
						
						wQ3pre[Mtot]=ev->b[ib].t[it].w[iw].pretrig;
						wQ3.push_back(ev->b[ib].t[it].w[iw].wf);
						iQ3[Mtot]=off[5]; bQ3[Mtot]=true;
						off[5]+=ev->b[ib].t[it].w[iw].wf.size();
				}
			}
			Mtot++;
		}
	}
	tree->Fill();
	treeEv++;
	
	return;
}

int main(int argc,char **argv) {
	struct sockaddr src;
	struct sockaddr_in sockin;
	int i,j,sd_in,port=50000;
	char ndst[100];
	uint8_t reply[100000];
	unsigned dim;
	char fnout[100]="output.root";
	TRandom3 ran(0);
	
	printf("\n");
	printf("*******************************************\n");
	printf("*      FAZIA mini acquisition to ROOT     *\n");
	printf("*            by Simone Valdre'            *\n");
	printf("*            v1.3 (2019-12-16)            *\n");
	printf("*******************************************\n");
	printf("\n");
	
	gethostname(ndst,sizeof(ndst));
	sockin.sin_addr.s_addr = *((int *)(gethostbyname(ndst)->h_addr_list[0]));
	sockin.sin_port = ((port & 0xff) << 8) | ((port & 0xff00) >> 8);
	sockin.sin_family = AF_INET;
	printf("\033[1;32m[  INIT  ]\033[0;39m This PC is %s (%s:%d)\n",ndst,inet_ntoa(sockin.sin_addr),port);
	
	if((sd_in=socket(AF_INET,SOCK_DGRAM,0))<0) {
		perror("\033[1;31m[ SOCKET ] \033[0;39m");
		exit(1);
	}
	
	if(getsockopt(sd_in,SOL_SOCKET,SO_KEEPALIVE,&i,&dim)<0) {
		perror("\033[1;31m[ SOCKOP ] \033[0;39m");
		close(sd_in);
		exit(1);
	}
	
	if(bind(sd_in,(struct sockaddr *)&sockin,16)<0) {
		perror("\033[1;31m[  BIND  ] \033[0;39m");
		close(sd_in);
		exit(1);
	}
	
	printf("\033[1;32m[  INIT  ]\033[0;39m Generating ROOT dictionaries...\n");
	gInterpreter->GenerateDictionary("std::vector< std::vector<int16_t> >", "vector");
	
	if(argc>1) {
		if(strlen(argv[1])<100) strcpy(fnout,argv[1]);
	}
	TFile *fout=new TFile(fnout,"RECREATE");
	if(fout->IsZombie()) {
		close(sd_in);
		exit(1);
	}
	fout->cd();
	TTree *tree=new TTree("faziatree","Fazia data structure");
	
	//***** DAQ and event information *****
	//Event info
	tree->Branch("treeEv",&treeEv,"treeEv/l");						// DAQ internal event numbering
	tree->Branch("Mtot",&Mtot,"Mtot/s");							// Total multiplicity
	
	//Regional Board general info
	tree->Branch("fEC",&fEC,"fEC/l");								// ReBo event counter (42 bits)
	tree->Branch("fGTTagRB",&fGTTagRB,"fGTTagRB/l");				// ReBo validation timestamp (40 ns units)
	tree->Branch("fTrigRB",&fTrigRB,"fTrigRB/s");					// ReBo trigger bitmask
	tree->Branch("fTempRB",&fTempRB,"fTempRB/F");					// Pt100 read by ReBo
	
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
	
	//Centrum info
	tree->Branch("fCentInfo",&fCentInfo,"fCentInfo/s");				//Centrum info flag (it is '1' only if CENTRUM is enabled)
	tree->Branch("fCentStat",&fCentStat,"fCentStat[fCentInfo]/s");	//Centrum stat bitmask
	tree->Branch("fCentTS",fCentTS,"fCentTS[fCentInfo]/l");			//Centrum time stamp
	tree->Branch("fCentEN",fCentEN,"fCentEN[fCentInfo]/i");			//Centrum event number
	
	//***** Particle information *****
	//Telescope ID&Tag
	tree->Branch("fBlk",fBlk,"fBlk[Mtot]/s");						// Block ID
	tree->Branch("fQua",fQua,"fQua[Mtot]/s");						// Quartet ID
	tree->Branch("fTel",fTel,"fTel[Mtot]/s");						// Telescope ID
	tree->Branch("fDeltaTag",fDeltaTag,"fDeltaTag[Mtot]/S");		// Validation - local trigger delta (10 ns units)
	tree->Branch("fTrig",fTrig,"fTrig[Mtot]/s");					// Local trigger pattern
	
	//On-board shaped energies
	tree->Branch("fQH1",fQH1,"fQH1[Mtot]/F");
	tree->Branch("fQ2",fQ2,"fQ2[Mtot]/F");
	tree->Branch("fQ3slow",fQ3slow,"fQ3slow[Mtot]/F");
	tree->Branch("fQ3fast",fQ3fast,"fQ3fast[Mtot]/F");
	
	//On-board baselines
	tree->Branch("fQH1bl",fQH1bl,"fQH1bl[Mtot]/F");
	tree->Branch("fQ2bl",fQ2bl,"fQ2bl[Mtot]/F");
	tree->Branch("fQ3bl",fQ3bl,"fQ3bl[Mtot]/F");
	
	//Waveform pre-trigger samples
	tree->Branch("wQH1pre",&wQH1pre,"wQH1pre[Mtot]/s");
	tree->Branch("wI1pre", &wI1pre, "wI1pre[Mtot]/s");
	tree->Branch("wQL1pre",&wQL1pre,"wQL1pre[Mtot]/s");
	tree->Branch("wQ2pre", &wQ2pre, "wQ2pre[Mtot]/s");
	tree->Branch("wI2pre", &wI2pre, "wI2pre[Mtot]/s");
	tree->Branch("wQ3pre", &wQ3pre, "wQ3pre[Mtot]/s");
	
	//Waveform offsets and presence flags, useful to plot in interactive mode:
	//e.g.: faziatree->Draw("wQ3:Iteration$-iQ3","fBlk==4&&fQua==4&&fTel==2&&bQ3","l",1,47);
	tree->Branch("iQH1",iQH1,"iQH1[Mtot]/i");
	tree->Branch("iI1",iI1,"iI1[Mtot]/i");
	tree->Branch("iQL1",iQL1,"iQL1[Mtot]/i");
	tree->Branch("iQ2",iQ2,"iQ2[Mtot]/i");
	tree->Branch("iI2",iI2,"iI2[Mtot]/i");
	tree->Branch("iQ3",iQ3,"iQ3[Mtot]/i");
	
	tree->Branch("bQH1",bQH1,"bQH1[Mtot]/O");
	tree->Branch("bI1",bI1,"bI1[Mtot]/O");
	tree->Branch("bQL1",bQL1,"bQL1[Mtot]/O");
	tree->Branch("bQ2",bQ2,"bQ2[Mtot]/O");
	tree->Branch("bI2",bI2,"bI2[Mtot]/O");
	tree->Branch("bQ3",bQ3,"bQ3[Mtot]/O");
	
	//Waveforms
	tree->Branch("wQH1",&wQH1);
	tree->Branch("wI1",&wI1);
	tree->Branch("wQL1",&wQL1);
	tree->Branch("wQ2",&wQ2);
	tree->Branch("wI2",&wI2);
	tree->Branch("wQ3",&wQ3);
	
	signal(SIGINT, sighandler);
	
	printf("\033[1;32m[  INIT  ]\033[0;39m Output file is %s\n",fnout);
	printf("\033[1;32m[  INIT  ]\033[0;39m Starting acquisition... Press CTRL+C to stop.\n");
	
	event ev;
	ev.ec=-1;
	block bl;
	tel tl;
	wav dt;
	int STATE=IDLE,data=0xc800,next=0xc800;
	unsigned flen=0,blen=0,dlen=0,tag=0,dstart=0,cntr=0;
	uint8_t fcrc=0,bcrc=0;
	bool clean=false,dump=false;
	int base8;
	double tmp;
	
	FILE *fdump;
	if(DUMP) fdump=fopen("dump.txt","w");
	
	for(;;) {
		i=0;
		i=recvfrom(sd_in,(void *)reply,sizeof(reply),0,&src,&dim);
		if(i<4) {
			perror("\033[1;31m[RECVFROM] \033[0;39m");
			break;
		}
		for(j=5;j<i;j+=2) {
			if(next!=0x8080) data=next;
			next=(unsigned)(reply[j-1])+(((unsigned)(reply[j]))<<8);
			if(go&&DUMP&&dump) fprintf(fdump,"0x%04x\n",data);
			if(next==0x8080) continue;
			clean=false; dump=true;
			switch(STATE) {
				case IDLE:
					//********** NEXT STATE **********
					if((next&0xf000)==0xe000) {
						if(DEBUG) printf("[  IDLE  ] next=0x%04x: entering state EC...\n",next);
						STATE=EC;
					}
					break;
				case EC:
					//Event Counter (0xExxx)
					if((ev.ec>=0)&&(ev.ec!=(data&0x0fff))) {
						printf("\033[1;31m[   EC   ]\033[0;39m Event counter mismatch: expecting %03x, read %03x\n",ev.ec,(data&0x0fff));
						clean=true;
					}
					else {
						ev.ec=(data&0x0fff);
						fcrc=((data>>8)&0xff)^(data&0xff);
						bcrc^=((data>>8)&0xff)^(data&0xff);
						flen=0; blen++;
					}
					//********** NEXT STATE **********
					if((next&0xf800)==0x9800) {
						if(DEBUG) printf("[   EC   ] next=0x%04x: entering state TELHDR...\n",next);
						STATE=TELHDR;
					}
					else if((next&0xf000)==0xc000) {
						if(DEBUG) printf("[   EC   ] next=0x%04x: entering state BLKHDR...\n",next);
						STATE=BLKHDR;
					}
					else {
						printf("\033[1;31m[   EC   ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					break;
				case TELHDR:
					//Telescope header (0x98xx)
					tl.telid=data&0x1f;
					tl.gttag=0;
					tl.dettag=0;
					tl.trgpat=0;
					fcrc^=((data>>8)&0xff)^(data&0xff);
					bcrc^=((data>>8)&0xff)^(data&0xff);
					flen++; blen++;
					//********** NEXT STATE **********
					if(next&0x8000) {
						printf("\033[1;31m[ TELHDR ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					else {
						if(DEBUG) printf("[ TELHDR ] next=0x%04x: entering state VALTAG...\n",next);
						STATE=VALTAG;
					}
					break;
				case VALTAG:
					//Validation time tag (DATA)
					tl.gttag=data;
					fcrc^=((data>>8)&0xff)^(data&0xff);
					bcrc^=((data>>8)&0xff)^(data&0xff);
					flen++; blen++;
					//********** NEXT STATE **********
					if(next&0x8000) {
						printf("\033[1;31m[ VALTAG ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					else {
						if(DEBUG) printf("[ VALTAG ] next=0x%04x: entering state TELTAG...\n",next);
						STATE=TELTAG;
					}
					break;
				case TELTAG:
					//Local trigger time tag (DATA)
					tl.dettag=data;
					fcrc^=((data>>8)&0xff)^(data&0xff);
					bcrc^=((data>>8)&0xff)^(data&0xff);
					flen++; blen++;
					//********** NEXT STATE **********
					if((next&0xf800)==0x9000) {
						if(DEBUG) printf("[ TELTAG ] next=0x%04x: entering state DETHDR...\n",next);
						STATE=DETHDR;
					}
					else if((next&0x8000)==0) {
						if(DEBUG) printf("\033[1;33m[ TELTAG ]\033[0;39m next=0x%04x: entering state TRGPAT...\n",next);
						STATE=TRGPAT;
					}
					else {
						printf("\033[1;31m[ TELTAG ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					break;
				case TRGPAT:
					//Trigger pattern (DATA)
					tl.trgpat=data;
					fcrc^=((data>>8)&0xff)^(data&0xff);
					bcrc^=((data>>8)&0xff)^(data&0xff);
					flen++; blen++;
					//********** NEXT STATE **********
					if((next&0xf800)==0x9000) {
						if(DEBUG) printf("[ TRGPAT ] next=0x%04x: entering state DETHDR...\n",next);
						STATE=DETHDR;
					}
					else {
						printf("\033[1;31m[ TRGPAT ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					break;
				case DETHDR:
					//Waveform header (0x9xxx)
					if(tl.telid!=(data&0x1f)) {
						printf("\033[1;31m[ DETHDR ]\033[0;39m TELID mismatch: expecting %04x, read %04x\n",tl.telid,data&0x1f);
						clean=true;
					}
					else {
						dt.detid=(data&0x0e0)>>5;
						dt.wave=(data&0x700)>>8;
						dt.pretrig=0;
						dt.fastrise=200;
						dt.fasten=0;
						dt.slowrise=200;
						dt.slowen=0;
						dt.baseline=8192;
						dt.wf.clear(); //probabilmente è inutile... valutare se togliere riga per rendere più efficiente l'acquisizione
						dt.norm=-1;
						for(int ic=0;ic<12;ic++) dt.counters[ic]=0;
						for(int ic=0;ic<7;ic++) dt.cent_frame[ic]=0;
						dt.rbtrigpat=0; dt.gtmsb=0; dt.ecmsb=0; dt.tmpt=2000;
						dstart=2;
						if((dt.detid==0)&&(dt.wave==0)) dstart=4;
						if((dt.detid==1)&&(dt.wave==0)) dstart=4;
						if((dt.detid==2)&&(dt.wave==0)) dstart=6;
						dlen=dstart;
						fcrc^=((data>>8)&0xff)^(data&0xff);
						bcrc^=((data>>8)&0xff)^(data&0xff);
						flen++; blen++;
					}
					//********** NEXT STATE **********
					if((next&0xf000)==0x7000) {
						if(DEBUG) printf("[ DETHDR ] next=0x%04x: entering state TAGHDR...\n",next);
						STATE=TAGHDR;
					}
					else if(next==0x3030) {
						if(DEBUG) printf("[ DETHDR ] next=0x%04x: entering state PADDIN...\n",next);
						STATE=PADDIN;
					}
					else if((next&0x8000)==0) {
						cntr=0;
						if(DEBUG) printf("[ DETHDR ] next=0x%04x: entering state OLDATA...\n",next);
						STATE=OLDATA;
					}
					else {
						printf("\033[1;31m[ DETHDR ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					break;
				case TAGHDR:
					//Data TAG (0x7xxx)
					tag=(data&0xfff);
					fcrc^=((data>>8)&0xff)^(data&0xff);
					bcrc^=((data>>8)&0xff)^(data&0xff);
					flen++; blen++;
					//********** NEXT STATE **********
					if((next&0x8000)==0) {
						if(DEBUG) printf("[ TAGHDR ] next=0x%04x: entering state TAGLEN...\n",next);
						STATE=TAGLEN;
					}
					else {
						printf("\033[1;31m[ TAGHDR ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					break;
				case TAGLEN:
					//Data length (DATA)
					dlen=data;
					fcrc^=((data>>8)&0xff)^(data&0xff);
					bcrc^=((data>>8)&0xff)^(data&0xff);
					flen++; blen++;
					//********** NEXT STATE **********
					if((next&0x8000)==0) {
						cntr=0;
						if(DEBUG) printf("[ TAGLEN ] next=0x%04x: entering state TAGDAT...\n",next);
						STATE=TAGDAT;
					}
					else {
						printf("\033[1;31m[ TAGLEN ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					break;
				case TAGDAT:
					//Data (DATA)
					switch(tag) {
						case 1:
							if(cntr==0) dt.slowrise=data;
							if(cntr==1) dt.slowen=data<<15;
							if(cntr==2) dt.slowen+=data;
							break;
						case 2:
							if(cntr==0) dt.fastrise=data;
							if(cntr==1) dt.fasten=data<<15;
							if(cntr==2) dt.fasten+=data;
							break;
						case 3:
							if(cntr==0) {
								if(dlen==1) {
									//First revision of new fw
									if(data>8191) dt.baseline=((float)(data-16384));
									else dt.baseline=(float)data;
								}
								else base8=data<<15; //Final revision
							}
							if(cntr==1) {
								base8+=data;
								if(base8>536870911) base8-=1073741824;
								dt.baseline=((float)base8)/16.;
							}
							break;
						case 4: dt.pretrig=data; break;
						case 5: dt.wf.push_back((data>=0x2000)?(data-0x4000):data); break;
						//RB info
						case 0x100:
							if(cntr<12) dt.counters[cntr]=data;
							break;
						case 0x101:
							if(cntr==0) dt.norm=data<<15;
							if(cntr==1) dt.norm+=data;
							break;
						case 0x110:
							if(cntr==0) dt.gtmsb=data<<15;
							if(cntr==1) dt.gtmsb+=data;
							break;
						case 0x111:
							if(cntr==0) dt.ecmsb=data<<15;
							if(cntr==1) dt.ecmsb+=data;
							break;
						case 0x112: dt.rbtrigpat=data; break;
						case 0x113:
							if(data==0) dt.tmpt=1000;
							else if(data>=32767) dt.tmpt=2000;
							else {
								tmp=(double)data + ran.Uniform(-0.5,0.5);
								dt.tmpt=(float)(-246.192+3.39245e-2*tmp+1.99014e-7*tmp*tmp);
							}
							break;
						
						//CENTRUM
						case 0x200:
							if(cntr<7) dt.cent_frame[cntr]=data;
					}
					cntr++;
					fcrc^=((data>>8)&0xff)^(data&0xff);
					bcrc^=((data>>8)&0xff)^(data&0xff);
					flen++; blen++;
					//********** NEXT STATE **********
					if(cntr<dlen) {
						if(next&0x8000) {
							printf("\033[1;31m[ TAGDAT ]\033[0;39m Invalid next data (0x%04x)\n",next);
							clean=true;
						}
						else {
							if(DEBUG&&tag!=5) printf("[ TAGDAT ] next=0x%04x: staying in state TAGDAT...\n",next);
						}
					}
					else {
						if((next&0xf000)==0x7000) {
							if(DEBUG) printf("[ TAGDAT ] next=0x%04x: entering state TAGHDR...\n",next);
							STATE=TAGHDR;
						}
						else if(next==0x3030) {
							if(DEBUG) printf("[ TAGDAT ] next=0x%04x: entering state PADDIN...\n",next);
							STATE=PADDIN;
						}
						else if((next&0xf800)==0x9000) {
							tl.w.push_back(dt);
							clearwav(dt);
							if(DEBUG) printf("[ TAGDAT ] next=0x%04x: entering state DETHDR...\n",next);
							STATE=DETHDR;
						}
						else if((next&0xf800)==0x9800) {
							tl.w.push_back(dt);
							bl.t.push_back(tl);
							clearwav(dt);
							cleartel(tl);
							if(DEBUG) printf("[ TAGDAT ] next=0x%04x: entering state TELHDR...\n",next);
							STATE=TELHDR;
						}
						else if((next&0xf000)==0xa000) {
							tl.w.push_back(dt);
							bl.t.push_back(tl);
							clearwav(dt);
							cleartel(tl);
							if(DEBUG) printf("[ TAGDAT ] next=0x%04x: entering state FEELEN...\n",next);
							STATE=FEELEN;
						}
						else {
							printf("\033[1;31m[ TAGDAT ]\033[0;39m Invalid next data (0x%04x)\n",next);
							clean=true;
						}
					}
					break;
				case PADDIN:
					// Padding data in TAG format
					fcrc^=((data>>8)&0xff)^(data&0xff);
					bcrc^=((data>>8)&0xff)^(data&0xff);
					flen++; blen++;
					//********** NEXT STATE **********
					if((next&0xf000)==0x7000) {
						if(DEBUG) printf("[ PADDIN ] next=0x%04x: entering state TAGHDR...\n",next);
						STATE=TAGHDR;
					}
					else if(next==0x3030) {
						if(DEBUG) printf("[ PADDIN ] next=0x%04x: entering state PADDIN...\n",next);
						STATE=PADDIN;
					}
					else if((next&0xf800)==0x9000) {
						tl.w.push_back(dt);
						clearwav(dt);
						if(DEBUG) printf("[ PADDIN ] next=0x%04x: entering state DETHDR...\n",next);
						STATE=DETHDR;
					}
					else if((next&0xf800)==0x9800) {
						tl.w.push_back(dt);
						bl.t.push_back(tl);
						clearwav(dt);
						cleartel(tl);
						if(DEBUG) printf("[ PADDIN ] next=0x%04x: entering state TELHDR...\n",next);
						STATE=TELHDR;
					}
					else if((next&0xf000)==0xa000) {
						tl.w.push_back(dt);
						bl.t.push_back(tl);
						clearwav(dt);
						cleartel(tl);
						if(DEBUG) printf("[ PADDIN ] next=0x%04x: entering state FEELEN...\n",next);
						STATE=FEELEN;
					}
					else {
						printf("\033[1;31m[ PADDIN ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					break;
				case OLDATA:
					//Old data format
					if(cntr<dstart-2) {
						if(cntr==0) dt.slowen=data<<15;
						if(cntr==1) dt.slowen+=data;
						if(cntr==2) dt.fasten=data<<15;
						if(cntr==3) dt.fasten+=data;
					}
					else if(cntr==dstart-2) {
						dt.pretrig=data;
					}
					else if(cntr==dstart-1) {
						dlen=data;
					}
					else {
						dt.wf.push_back((data>=0x2000)?(data-0x4000):data);
					}
					cntr++;
					fcrc^=((data>>8)&0xff)^(data&0xff);
					bcrc^=((data>>8)&0xff)^(data&0xff);
					flen++; blen++;
					//********** NEXT STATE **********
					if(cntr<dstart+dlen) {
						if(next&0x8000) {
							printf("\033[1;31m[ OLDATA ]\033[0;39m Invalid next data (0x%04x)\n",next);
							clean=true;
						}
					}
					else {
						if((next&0xf800)==0x9000) {
							tl.w.push_back(dt);
							clearwav(dt);
							if(DEBUG) printf("[ OLDATA ] next=0x%04x: entering state DETHDR...\n",next);
							STATE=DETHDR;
						}
						else if((next&0xf800)==0x9800) {
							tl.w.push_back(dt);
							bl.t.push_back(tl);
							clearwav(dt);
							cleartel(tl);
							if(DEBUG) printf("[ OLDATA ] next=0x%04x: entering state TELHDR...\n",next);
							STATE=TELHDR;
						}
						else if((next&0xf000)==0xa000) {
							tl.w.push_back(dt);
							bl.t.push_back(tl);
							clearwav(dt);
							cleartel(tl);
							if(DEBUG) printf("[ OLDATA ] next=0x%04x: entering state FEELEN...\n",next);
							STATE=FEELEN;
						}
						else {
							printf("\033[1;31m[ OLDATA ]\033[0;39m Invalid next data (0x%04x)\n",next);
							clean=true;
						}
					}
					break;
				case FEELEN:
					//FEE length (0xAxxx)
					if((flen+1)%4096!=(unsigned)(data&0xfff)) {
						printf("\033[1;31m[ FEELEN ]\033[0;39m FEE length mismatch: expecting %u words, read %u words (mod 4096)\n",(flen+1)%4096,(data&0x0fff));
						clean=true;
					}
					else {
						bcrc^=((data>>8)&0xff)^(data&0xff);
						blen++;
					}
					//********** NEXT STATE **********
					if((next&0xf000)==0xb000) {
						if(DEBUG) printf("[ FEELEN ] next=0x%04x: entering state FEECRC...\n",next);
						STATE=FEECRC;
					}
					else {
						printf("\033[1;31m[ FEELEN ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					break;
				case FEECRC:
					//FEE CRC and End of FEE (0xBxxx)
					if(fcrc!=((data>>4)&0xff)) {
						printf("\033[1;31m[ FEECRC ]\033[0;39m FEE CRC mismatch: expecting %02x, read %02x\n",(unsigned)fcrc,(unsigned)((data>>4)&0xff));
						clean=true;
					}
					else {
						bcrc^=((data>>8)&0xff)^(data&0xff);
						blen++;
					}
					//********** NEXT STATE **********
					if((next&0xf000)==0xe000) {
						if(DEBUG) printf("[ FEECRC ] next=0x%04x: entering state EC...\n",next);
						STATE=EC;
					}
					else {
						printf("\033[1;31m[ FEECRC ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					break;
				case BLKHDR:
					//Block ID (0xCxxx)
					bl.blkid=(data&0x0fff);
					bcrc^=((data>>8)&0xff)^(data&0xff);
					blen++;
					//********** NEXT STATE **********
					if((next&0xf000)==0xa000) {
						if(DEBUG) printf("[ BLKHDR ] next=0x%04x: entering state BLKLEN...\n",next);
						STATE=BLKLEN;
					}
					else {
						printf("\033[1;31m[ BLKHDR ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					break;
				case BLKLEN:
					//Block length (0xAxxx)
					if((blen+2)%4096!=(data&0x0fff)) {
						printf("\033[1;31m[ BLKLEN ]\033[0;39m Block length mismatch: expecting %u words, read %u words (mod 4096)\n",(blen+2)%4096,(data&0x0fff));
						clean=true;
					}
					else {
						bcrc^=((data>>8)&0xff)^(data&0xff);
					}
					//********** NEXT STATE **********
					if((next&0xf000)==0xd000) {
						if(DEBUG) printf("[ BLKLEN ] next=0x%04x: entering state BLKCRC...\n",next);
						STATE=BLKCRC;
					}
					else {
						printf("\033[1;31m[ BLKLEN ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
					break;
				case BLKCRC:
					//Block CRC and End of Block (0xDxxx)
					if(bcrc!=((data>>4)&0xff)) {
						printf("\033[1;31m[ BLKCRC ]\033[0;39m Blk %d CRC mismatch: expecting %02x, read %02x\n",bl.blkid,(unsigned)bcrc,(unsigned)((data>>4)&0xff));
						clean=true;
					}
					else {
						ev.b.push_back(bl);
						clearblock(bl);
						bcrc=0; blen=0;
					}
					//********** NEXT STATE **********
					if((next&0xf000)==0xe000) {
						if(DEBUG) printf("[ BLKCRC ] next=0x%04x: entering state EC...\n",next);
						STATE=EC;
					}
					else if((next&0xff00)==0xc800) {
						WriteEv(&ev,tree);
						if(treeEv%1000==0) {
							printf("\033[1m[  STAT  ]\033[0;39m %12lu events written on disk.\n",treeEv);
							printf("\033[1m[  STAT  ]\033[0;39m %12lu bad events (error rate: %6.2lf %%)\n\n",(Tot+1)-treeEv,100.*((double)((Tot+1)-treeEv))/((double)(Tot+1)));
							fout->Write();
							fout->Purge();
						}
						clean=true;
					}
					else {
						printf("\033[1;31m[ BLKCRC ]\033[0;39m Invalid next data (0x%04x)\n",next);
						clean=true;
					}
			}
			if(clean) {
				//Freeing memory and cleaning structure
				clearwav(dt); cleartel(tl); clearblock(bl); clearevent(ev);
				ev.ec=-1;
				fcrc=0;bcrc=0;
				flen=0;blen=0;
				dlen=0;
				Tot++;
				STATE=IDLE;
			}
		}
		if(!go) break;
	}
	if(DUMP) fclose(fdump);
	clearwav(dt); cleartel(tl); clearblock(bl); clearevent(ev);
	fout->Write();
	fout->Purge();
	fout->Close();
	close(sd_in);
	return 0;
}

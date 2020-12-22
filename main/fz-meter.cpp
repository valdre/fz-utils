/*******************************************************************************
*                                                                              *
*                         Simone Valdre' - 22/12/2020                          *
*                  distributed under GPL-3.0-or-later licence                  *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include "FzMeter.h"

static MyMainFrame *me;
pid_t pproc;
int pd[2];
int go;

void handle_usr2(int num) {
	go=0;
	num++; //Just to not show a warning during compilation
	return;
}

extern "C" {
	static void handle_usr1(int) {
		me->refresh();
		return;
	}
}

void MyMainFrame::refresh() {
	double trig[12],time,dead;
	int bitmask;
	
	read(pd[0],(void *)(&time),sizeof(double));
	read(pd[0],(void *)trig,12*sizeof(double));
	read(pd[0],(void *)(&bitmask),sizeof(int));
	dead=100.*(1.-trig[11]/trig[0]);
	
	val->SetText(Form("%.0lf",trig[11]/time));
	pbtrg->Reset();
	if(trig[0]/time<1) pbtrg->SetPosition(0);
	else pbtrg->SetPosition(log10(trig[0]/time));
	trg->SetText(Form("%.0lf",trig[0]/time));
	for(int i=0;i<10;i++) {
		pbt[i]->Reset();
		if(trig[1+i]/time<1) pbt[i]->SetPosition(0);
		else pbt[i]->SetPosition(log10(trig[1+i]/time));
		
		tet[i]->SetText(Form("%.0lf",trig[1+i]/time));
		if(i>7) continue;
		if(bitmask&(1<<i)) {
			tet[i]->SetAlignment(kTextRight);
			pbt[i]->SetBarColor("green");
		}
		else {
			tet[i]->SetAlignment(kTextLeft);
			tet[i]->SetText("disabled");
			pbt[i]->SetPosition(1000);
			pbt[i]->SetBarColor("gray");
		}
	}
	dt->SetText(Form("%.1lf %%",dead));
	return;
}

MyMainFrame::MyMainFrame(const TGWindow *p,UInt_t w,UInt_t h) {
	me=this;
	signal(SIGUSR1,handle_usr1);
	
	FontStruct_t font1 = gClient->GetFontByName("-adobe-helvetica-medium-r-*-*-48-*-*-*-*-*-iso8859-1");
	FontStruct_t font2 = gClient->GetFontByName("-adobe-helvetica-medium-r-*-*-24-*-*-*-*-*-iso8859-1");
	FontStruct_t font3 = gClient->GetFontByName("-adobe-helvetica-bold-r-*-*-24-*-*-*-*-*-iso8859-1");
	
	// Create a main frame
	fMain = new TGMainFrame(p,w,h);
	// vlay starts
	TGVerticalFrame *vlay=new TGVerticalFrame(fMain);
	// hf10
	TGHorizontalFrame *hf10=new TGHorizontalFrame(vlay);
	TGLabel *ltt = new TGLabel(hf10,"AAAAAAAAAAAA");
	ltt->SetTextFont(font3);
	hf10->AddFrame(ltt, new TGLayoutHints(kLHintsCenterX,5,5,3,4));
	TGLabel *lval = new TGLabel(hf10,"AAAAAAAAAAAA");
	lval->SetTextFont(font3);
	hf10->AddFrame(lval, new TGLayoutHints(kLHintsCenterX,5,5,3,4));
	TGLabel *ldt = new TGLabel(hf10,"AAAAAAAAAAAA");
	ldt->SetTextFont(font3);
	hf10->AddFrame(ldt, new TGLayoutHints(kLHintsCenterX,5,5,3,4));
	vlay->AddFrame(hf10, new TGLayoutHints(kLHintsExpandX|kLHintsCenterX|kLHintsCenterY,2,2,2,2));
	// hf11
	TGHorizontalFrame *hf11=new TGHorizontalFrame(vlay);
	trg=new TGTextEntry(hf11,"********");
	trg->SetFont(font1);
	trg->Resize(200,48);
	trg->SetEnabled(kFALSE);
	trg->SetAlignment(kTextRight);
	hf11->AddFrame(trg, new TGLayoutHints(kLHintsCenterX,5,5,3,4));
	val=new TGTextEntry(hf11,"********");
	val->SetFont(font1);
	val->Resize(200,48);
	val->SetEnabled(kFALSE);
	val->SetAlignment(kTextRight);
	hf11->AddFrame(val, new TGLayoutHints(kLHintsCenterX,5,5,3,4));
	dt=new TGTextEntry(hf11,"********");
	dt->SetFont(font1);
	dt->Resize(200,48);
	dt->SetEnabled(kFALSE);
	dt->SetAlignment(kTextRight);
	hf11->AddFrame(dt, new TGLayoutHints(kLHintsCenterX,5,5,3,4));
	vlay->AddFrame(hf11, new TGLayoutHints(kLHintsExpandX|kLHintsCenterX|kLHintsCenterY,2,2,2,2));
	// hpb
	pbtrg=new TGHProgressBar(vlay,200,48);
	pbtrg->SetBarColor("blue");
	pbtrg->SetRange(0,4);
	pbtrg->SetPosition(4);
	pbtrg->ShowPosition(kFALSE);
	vlay->AddFrame(pbtrg, new TGLayoutHints(kLHintsExpandX|kLHintsCenterX|kLHintsCenterY,2,2,2,2));
	// triggers
	TGLabel *ltrg = new TGLabel(vlay,"Individual trigger rate");
	ltrg->SetTextFont(font3);
	vlay->AddFrame(ltrg, new TGLayoutHints(kLHintsCenterX,5,5,3,4));
	// hf2[10]
	TGHorizontalFrame *hf2[10];
	TGLabel *lt[10];
	for(int i=0;i<10;i++) {
		hf2[i]=new TGHorizontalFrame(vlay);
		pbt[i]=new TGHProgressBar(hf2[i],400,32);
		pbt[i]->SetRange(0,4);
		pbt[i]->SetPosition(4);
		pbt[i]->SetBarColor("green");
		pbt[i]->ShowPosition(kFALSE);
		hf2[i]->AddFrame(pbt[i], new TGLayoutHints(kLHintsCenterX,5,5,3,4));
		lt[i] = new TGLabel(hf2[i],"AAAAA");
		lt[i]->SetTextFont(font3);
		hf2[i]->AddFrame(lt[i], new TGLayoutHints(kLHintsCenterX,5,5,3,4));
		tet[i]=new TGTextEntry(hf2[i],"********");
		tet[i]->SetFont(font2);
		tet[i]->Resize(160,32);
		tet[i]->SetEnabled(kFALSE);
		tet[i]->SetAlignment(kTextRight);
		hf2[i]->AddFrame(tet[i], new TGLayoutHints(kLHintsCenterX,5,5,3,4));
		vlay->AddFrame(hf2[i], new TGLayoutHints(kLHintsExpandX|kLHintsCenterX|kLHintsCenterY,2,2,2,2));
	}
	// exit
	TGTextButton *exit = new TGTextButton(vlay,"AAAAAAA");
	exit->Connect("Clicked()","MyMainFrame",this,"close_cycle()");
	exit->Resize(240,40);
	exit->SetFont(font3);
	vlay->AddFrame(exit, new TGLayoutHints(kLHintsCenterX,5,5,3,4));
	
	fMain->AddFrame(vlay, new TGLayoutHints(kLHintsCenterX|kLHintsCenterY|kLHintsExpandY|kLHintsExpandX,2,2,2,2));
	
	// Set a name to the main frame
	fMain->SetWindowName("FAZIA meter");
	
	// Map all subwindows of main frame
	fMain->MapSubwindows();
	
	// Initialize the layout algorithm
	fMain->Resize(fMain->GetDefaultSize());
	
	// Map main frame
	fMain->MapWindow();
	
	ltt->SetText("Trigger rate");
	lval->SetText("Validation rate");
	ldt->SetText("Dead time");
	for(int i=0;i<8;i++) {
		lt[i]->SetText(Form("Trg %d",i+1));
	}
	lt[8]->SetText("MAN");
	lt[9]->SetText("EXT");
	exit->SetText("&Close");
}

void MyMainFrame::close_cycle() {
	kill(pproc,SIGUSR2);
	sleep(1);
	gApplication->Terminate(0);
	return;
}

MyMainFrame::~MyMainFrame() {
	// Clean up used widgets: frames, buttons, layout hints
	fMain->Cleanup();
	delete fMain;
	return;
}

int main(int argc, char **argv) {
	pipe(pd);
	pproc=fork();
	
	if(pproc<0) {
		perror("fork");
	}
	else {
		if(pproc==0) { //processo figlio
			double trig[12],time;
			int bitmask;
			FzSC sock;
			close(pd[0]);
			signal(SIGUSR2,handle_usr2);
			go=1;
			sleep(2);
			for(;go;) {
				if(sock.SockOK()) {
					sock.Meter(&time,trig,&bitmask);
					write(pd[1],(void *)(&time),sizeof(double));
					write(pd[1],(void *)trig,12*sizeof(double));
					write(pd[1],(void *)(&bitmask),sizeof(int));
					kill(getppid(),SIGUSR1);
				}
				sleep(10);
			}
			return 0;
		}
		else { //processo genitore
			close(pd[1]);
			TApplication theApp("App",&argc,argv);
			new MyMainFrame(gClient->GetRoot(),800,600);
			theApp.Run();
		}
	}
	return 0;
}

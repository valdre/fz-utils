#include <TQObject.h>
#include <RQ_OBJECT.h>
#include <TGTextEntry.h>
#include <TGLabel.h>
#include <TGProgressBar.h>
#include <TString.h>
#include <TROOT.h>
#include <TStyle.h>
#include <TApplication.h>
#include <TGClient.h>
#include <TGButton.h>
#include <TGTextBuffer.h>
#include "FzUDP.h"

#define NRIGHE 13

class TGWindow;
class TGMainFrame;

class MyMainFrame {
	RQ_OBJECT("MyMainFrame")
private:
	TGMainFrame *fMain;
	TGTextEntry *val,*dt,*trg,*tet[10];
	TGHProgressBar *pbtrg,*pbt[10];
public:
	MyMainFrame(const TGWindow *p,UInt_t w,UInt_t h);
	void refresh();
	void close_cycle();
	virtual ~MyMainFrame();
};

#include "pdp_core.h"
#include "PDP_ISA.h"
// Main 4K 18bit memory
// Requires a MCU that has more than 12KBytes of RAM
// ii[5:0] = [5:0] {INDIRECT, INSTRUCTION[4:0]}
// ad[11:0] = [17:6] MEMORY_ADDR_Y[11:0]
// uint8_t pdp_memory_ii[4096];
// uint16_t pdp_memory_ad[4096];
uint32_t memory[4096];
uint8_t pdp_countdown;	// System Cycle Count-down
uint32_t ac, io, pc, y, ib, ov;
uint8_t flags = 0;
uint8_t status = 0;
uint8_t sense = 0;
uint32_t control = 0;
uint8_t testword = 0;
uint8_t halt_sst = 0;

// Cycle Control
uint32_t ctc = 0;
uint32_t ioc = 0;
uint32_t msecs = 0;

// Global settings
// | 7 | 6 | 5 | 4 | 3 | 2 | 1 : 0 |
//           |   |   |   |   '------- 00: RUN, 01: HALT, 10: SST
//           |   |   |   '----------- Enable Mul, Div
//           |   |   '--------------- Merge R(Control) and R(Testword)
//           |   '------------------- Odd Frame Fix
//           '----------------------- Rotate Controls
uint8_t settings = 0; 
void (*trapPtr)(void* arg);	// Trap Callback Function Pointer
void (*plotCRTPtr)(uint16_t x, uint16_t y, uint16_t i);	// CRT Callback Pointer

/*
const uint16_t shiftMasksLo[]={0, 01, 03, 07, 017, 037, 077, 0177, 0377, 0777},
const uint32_t shiftMasksHi[]={0, 0400000, 0600000, 0700000, 0740000, 0760000, \
								0770000, 0774000, 0776000, 0777000};
*/

// void PDP_setEmulationSpeed(float v){}
// float PDP_getEmulationSpeed();

// PAPER TAPE FUNCTIONS
void tapeReset();
int32_t tapeReadBin(uint8_t rtb);
int32_t tapeReadIn(uint32_t* mem);
int32_t tapeReadAlpha(uint8_t rtb);
int32_t tapeReadBuffer();

// INTERNAL FUNCTIONS


// CORE ACCESS 
uint32_t PDP_step(){
	ctc = 0;
	settings &= ~01;
	PDP_dispatch(memory[pc++],-1);
	if(ioc) {
		ioc -= ctc;
		if(ioc<0) ioc=0;
	}
	msecs += ctc;
	return pc;
}


void PDP_initialize(){
	PDP_resetMem();
	PDP_resetRegs();
}

void PDP_resetMem(){
	for(uint16_t i=0;i<4096;i++) memory[i] = 0;
}

void PDP_resetRegs(){
	ac=0; io=0; pc=4; ov=0;
	flags=0; status=0; control=0; settings&=~01;
	msecs=0; ioc=0;
}

// INTERNAL VARIABLE ACCESS
uint8_t PDP_getTC(){ return msecs; }
void PDP_clearTC(){ msecs = 0; }
void PDP_setPC(uint32_t addr){ pc = addr; }
uint32_t PDP_getPC(){ return pc; }

void PDP_setSense(uint8_t n, uint8_t v) {
	if(n<6)
		if(v) sense|=BIT(n);
		else  sense&=(~BIT(n))&077;
}
uint8_t PDP_getSense() { return sense; }

void PDP_setControl(uint8_t n, uint8_t v) {
	if(n<18)
		if(v) control|=BIT(n);
		else  control&=(~BIT(n))&0777777;
}

uint32_t PDP_getControl() { return control; }

void PDP_setTestWord(uint8_t n, uint8_t v) {
	if(n<18)
		if(v) testword|=BIT(n);
		else  testword&=(~BIT(n))&0777777;
}

uint32_t PDP_getTestWord() { return testword; }

// SETTINGS
void PDP_setMulDiv(uint8_t d){
	if(d)	settings|=BIT(2);
	else	settings&=~BIT(2);
}

void PDP_setMergeControls(uint8_t d){
	if(d)	settings|=BIT(3);
	else	settings&=~BIT(3);
}

void PDP_setFrame(uint8_t d){
	if(d)	settings|=BIT(4);
	else	settings&=~BIT(4);
}

void PDP_setRotateControls(uint8_t d){
	if(d)	settings|=BIT(5);
	else	settings&=~BIT(5);
}

void PDP_setTrapCallback(void(*cb)(void*)){trapPtr=cb;}
void PDP_setCRTCallback(void(*cb)(uint16_t, uint16_t, uint16_t)){plotCRTPtr=cb;}

// EXECUTION
/*
	DEC PDP-1 EMULATION

	Original emulation by Barry Silverman, Brian Silverman, 
	 and Vadim Gerasimov, 2012 (http://spacewar.oversigma.com/html5/)
	Extended by N.Landsteiner 2012-2015 (https://www.masswerk.at/)
	
	Ported to C by A.Lithia 2019 (https://www.lithcore.cn/)
	Important Changes:
		* High and low memories are now divided into two arrays, 
			24-bit per entry versus 32-bit in the original design
			saves space for embedded platforms
		* Shifting uses the older algorithm to save RAM space
*/
void PDP_dispatch(uint32_t md) {
	y = md&07777;
	ib = (md>>12)&1;
	// IND Check (ea)
	switch(md>>13) {
		case SKP:
		case SFT:
		case LAW:
		case IOT:
		case OPR: break;
		// loop until we got the terminal indirect address
		default: 
			ctc+=5;
			while(1){
				if(ib==0) return;
				ib=(memory[y]>>12)&1;
				y=memory[y]&07777;
			};
	}
	switch(md>>13) {
		case AND: ac&=memory[y]; break;
		case IOR: ac|=memory[y]; break;
		case XOR: ac^=memory[y]; break;
		case XCT: dispatch(memory[y]); break;
		case LAC: ac=memory[y]; break;
		case LIO: io=memory[y]; break;
		case DAC: memory[y]=ac; break;
		case DAP: memory[y]=(memory[y]&0770000)+(ac&07777); break;
		case DIO: memory[y]=io; break;
		case DZM: memory[y]=0; break;
		case ADD:
			ac = ac+memory[y];
			ov = ac>>18;
			ac = (ac+ov)&0777777;
			if(ac==0777777) ac=0;
			break;
		case SUB:
			uint32_t diffsigns = ((ac>>17)^(memory[y]>>17))==1;
			ac += memory[y]^0777777;
			ac = (ac+(ac>>18))&0777777;
			if(ac==0777777) ac=0;
			if(diffsigns&&(memory[y]>>17==ac>>17)) ov=1;
			break;
		case IDX:
			ac = memory[y]+1;
			if(ac==0777777) ac=0;
			memory[y] = ac;
			break;
		case ISP:
			ac = memory[y]+1;
			if(ac==0777777) ac=0;
			memory[y] = ac;
			if((ac&0400000)==0) pc++;
			break;
		case SAD: if(ac!=memory[y]) pc++; break;
		case SAS: if(ac==memory[y]) pc++; break;
		case MUS:
			if ((io&1)==1){
				ac=ac+memory[y];
				ac=(ac+(ac>>18))&0777777;
				if (ac==0777777) ac=0;
			}
			io=(io>>1|ac<<17)&0777777;
			ac>>=1;
			break;
		case DIS:
			uint32_t acl=ac>>17;
			ac=(ac<<1|io>>17)&0777777;
			io=((io<<1|acl)&0777777)^1;
			if ((io&1)==1){
				ac=ac+(memory[y]^0777777);
				ac=(ac+(ac>>18))&0777777;
			} else {
				ac=ac+1+memory[y];
				ac=(ac+(ac>>18))&0777777;
			}
			if (ac==0777777) ac=0;
			break;
		case JMP: pc=y; break;
		case JSP: ac=(ov<<17)+pc; pc=y; break;
		case SKP:
			uint8_t cond =
				(((y&0100)==0100)&&(ac==0)) ||
				(((y&0200)==0200)&&(ac>>17==0)) ||
				(((y&0400)==0400)&&(ac>>17==1)) ||
				(((y&01000)==01000)&&(ov==0)) ||
				(((y&02000)==02000)&&(io>>17==0))||
				(((y&7)!=0)&&!flag[y&7])||
				(((y&070)!=0)&&!sense[(y&070)>>3])||
				((y&070)==010);
			if (ib==0) {if(cond) pc++;}
				else {if(!cond) pc++;}
			if ((y&01000)==01000) ov=0;
			break;
		case SFT:
			uint8_t nshift=0, mask=md&0777;
			while (mask!=0) {nshift+=mask&1; mask=mask>>1;}
			for(uint8_t i=0;i<nshift;i++) {
				switch((md>>9)&017){
					case 1: ac=(ac<<1|ac>>17)&0777777; break;
					case 2: io=(io<<1|io>>17)&0777777; break;
					case 3:	
						uint32_t both=ac*k19+io*2+Math.floor(ac/k17);
						ac = Math.floor(both/k18)%k18;
						io = both%k18;
						break;
					case 5: ac=((ac<<1|ac>>17)&0377777)+(ac&0400000); break;
					case 6: io=((io<<1|io>>17)&0377777)+(io&0400000); break;
					case 7:	
						uint32_t both = (ac&0177777)*k19+io*2+Math.floor(ac/k17);
						both += (ac&0400000)*k18;
						ac = Math.floor(both/k18)%k18;
						io = both%k18;
						break;
					case 9: ac=(ac>>1|ac<<17)&0777777; break;
					case 10: io=(io>>1|io<<17)&0777777; break;
					case 11: 
						uint32_t both = ac*k17+Math.floor(io/2)+(io&1)*k35;
						ac = Math.floor(both/k18)%k18;
						io = both%k18;
						break;
					case 13: ac=(ac>>1)+(ac&0400000); break;
					case 14: io=(io>>1)+(io&0400000); break;
					case 15: 
						uint32_t both = ac*k17+Math.floor(io/2)+(ac&0400000)*k18;
						ac = Math.floor(both/k18)%k18;
						io = both%k18;
						break;
					default:
					//console.log('Undefined shift:',os(md),'at'+os(pc-1));
			      	}
		      }
		case LAW: ac=(ib==0)?y:y^0777777; break;
		case IOT:
			if((y&077)==007) {dpy(); break;}
			if((y&077)==011) {io=control; break;}
			break;
		case OPR:
			if((y&00200)==00200) ac=0;
			if((y&04000)==04000) io=0;
			if((y&01000)==01000) ac^=0777777;
			if((y&00400)==00400) panelrunpc = -1;
			uint8_t nflag = y&07;
			if(nflag<2) break;
			uint8_t state = (y&010)==010;
			if(nflag==7){
				for(uint8_t i=2;i<7;i++) flag[i]=state;
				break;
			}
			flag[nflag]=state;
			break;
		default:
		// undefined
	}
}

void dpy() {
	x = (ac+0400000)&0777777;
	y = (io+0400000)&0777777;
	x = x*550/0777777;
	y = y*550/0777777;
	// TODO: write DAC
}

uint8_t* tdata;
uint16_t tsize;
uint16_t tpos;
uint32_t tbuf;

void PDP_mountTape(tape_t* tape){
	if(tape) {
		tsize = (tape->tapeLeng)?tape->tapeLeng:0;
		if(tsize){
			tdata = tape->content;
			tpos = 0;
		} else 
			tdata = null;
	}
}

void PDP_tapeReadIn() {
	int32_t sa, te, tp, w, ds, de;
	PDP_resetRegs();
	pc = PDP_tapeReadIn(memory);
	if(pc==07751) {
		// Macro Loader
		sa = -1; te = -1; tp = tpos;
		w = 0; ds = 0; de = -1;
		while(w>=0) {
			w = tapeReadBin(0);
			if(w<0) return w;
			int32_t op = w>>12;
			if(op==060)	{	// jmp
				te = tpos;
				sa = w&07777;
				break;
			}
			if(op==032) {	//dio
				if (ds<0) { // start address
					ds = w&07777;
				} else { // end address
					de = w&07777;
					// payload and checksum
					for (int32_t i=ds; i<=de && w>=0; i++) 
						w=tapeReadBin();
					if (w<0) return w;
					ds = -1; de= -1;
				}
			} else
				return -5;
		}
		if (sa<0) return -6;
		// now run it as it should (emulated)
		tpos=tp;
		/*
		var tmpTrap=haltTrap;
		haltTrap=null;
		for (var i=0;
			i<0777777 && pc>=0 && (pc!=sa || tpos!=te) && !halted;
			i++) 
			pc=step();
		haltTrap=tmpTrap;
		if (halted) 	return -4;
		if (i==0777777)	return -1;
		*/
	}
	return pc;
}

void tapeReset(){
	tpos = 0;
	tbuf = 0;
	status &= 057777;	// Clear Status Bit 1
}

int32_t tapeReadBin(uint8_t rtb){
	uint32_t w;	// 18 bit buffer
	for(uint8_t i=0; i<3; i++)
		for(;;) {
			if(tpos>=tsize) {
				status &= 0577777;
				return -1;
			}
			uint32_t line = tdata[tpos++];
			if(line&0200) {
				w = (w<<6)|(line&077);
				break;
			}
		}
	if(rtb) {
		status |= 0200000;
		tbuf = w;
		return 0;
	} else
		return w;
}

int32_t tapeReadIn(uint32_t* mem){
	int32_t op, y;
	tbuf = 0;
	status &= 0577777;
	for(;;) {
		op = tapeReadBin(0);
		if(op<0) return op;
		y = op&07777;
		switch(op>>12){
			case 032:
				op = tapeReadBin(0);
				if(op<0) return op;
				mem[y] = (uint32_t) op;
				break;
			case 060:
				return y;
			default:
				// undefined instruction
				return -3;
		}
	}
}

int32_t tapeReadAlpha(uint8_t rtb) {
	if(tpos>=tsize) {
		status &= 0577777;
		return -1;
	}
	if(rtb) {
		status |= 0200000;
		tbuf = tdata[tpos++]&0377;
		return 0;
	} else 
		return  tdata[tpos++]&0377;
}

int32_t tapeReadBuffer() {
	if(status&0200000==0200000) {
		status &= 0577777;
		return tbuf;
	} else {
		return -2;
	}
}
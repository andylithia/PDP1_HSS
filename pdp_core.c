#include "pdp_core.h"
#include "PDP_ISA.h"
#include "BCG.h"
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
//   |   |   |   |   |   |   '------- 00: RUN, x1: HALT, 10: SST
//   |   |   |   |   |   '----------- Enable Mul, Div
//   |   |   |   |   '--------------- Merge R(Control) and R(Testword)
//   |   |   |   '------------------- Odd Frame Fix
//   |   |   '----------------------- Rotate Controls
//   |   '--------------------------- Enhanced CRT Function
//   '------------------------------- CRT Selectable Origin
uint16_t settings = 0; 

#define S_HWMULDIV_B    2
#define S_MERGECTRL_B   3
#define S_ODDFRAMEFIX_B 4
#define S_ROTATECTRL_B  5
#define S_ECRT_B        6
#define S_CRTSSORIG_B   7

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
void PDP_dispatch(uint32_t md, uint32_t xpc) {
	uint32_t mb, t, i;

	y = md&07777;
	ib = (md>>12)&1;
	ctc+=5;
	// IND Check (ea)
	switch((md>>12)&076) {
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
	switch((md>>12)&076) {
		case AND: ac&=memory[y]; break;
		case IOR: ac|=memory[y]; break;
		case XOR: ac^=memory[y]; break;
		case XCT: dispatch(memory[y],y); break;
		case CAL_JDA:
			uint32_t target=(ib==0)?0100:y;
			memory[target]=ac;
			ac=(ov<<17)+pc;
			pc=target+1;
			ctc+=5;
			break;
		case LAC: ac=memory[y]; break;
		case LIO: io=memory[y]; break;
		case DAC: memory[y]=ac; break;
		case DAP: memory[y]=(memory[y]&0770000)|(ac&0007777); break;
		case DIP: memory[y]=(memory[y]&0007777)|(ac&0770000); break;
		case DIO: memory[y]=io; break;
		case DZM: memory[y]=0; break;
		case ADD:
			mb = memory[y];
			uint32_t ov2=(ac>>17==mb>>17);
			ac+=mb;
			if(ac&01000000) ac=(ac+1)&0777777;
			if(ov2&(mb>>17!=ac>>17)) ov=1;
			if(ac==0777777) ac=0;
			/*
			ac = ac+memory[y];
			ov = ac>>18;
			ac = (ac+ov)&0777777;
			if(ac==0777777) ac=0;
			*/
			break;
		case SUB:
			mb=memory[y];
			ac^=0777777;
			uint32_t ov2=(ac>>17==mb>>17);
			ac+=mb;
			if(ac^01000000) ac=(ac+1)&0777777;
			if(ov2&(mb>>17!=ac>>17)) ov=1;
			ac^=0777777;
			/*
			uint32_t diffsigns = ((ac>>17)^(memory[y]>>17))==1;
			ac += memory[y]^0777777;
			ac = (ac+(ac>>18))&0777777;
			if(ac==0777777) ac=0;
			if(diffsigns&&(memory[y]>>17==ac>>17)) ov=1;
			*/
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
			if((ac&0400000)==0) pc=(xpc>=0)?xpc+1:pc+1;
			break;
		case SAD: if(ac!=memory[y]) pc++; break;
		case SAS: if(ac==memory[y]) pc++; break;
		case MUS_MUL:
			if(settings&BIT(S_HWMULDIV_B)) {	// MUL
				uint8_t scr, smb, srm;
				mb=memory[y];
				io=ac;
				if(mb&0400000) {
					smb=1;
					mb^=0777777;
				} else
					smb=0;
				if(io&0400000) {
					srm=1;
					io^=0777777;
				} else
					srm=0;
				ac=0;
				scr=1;
				while(scr<022) {
					if(io&1) {
						ac+=mb;
						if(a&01000000) ac=(ac+1)&0777777;
					}
					io=io>>1|((ac&1)<<17);
					ac>>=1;
					scr++;
				}
				if(smb!=srm&&(ac|io!=0)) {
					ac=ac^0777777;
					io=io^0777777;
				}
				ctc+=10;
			} else {
				if ((io&1)==1){
					ac=ac+memory[y];
					ac=(ac+(ac>>18))&0777777;
					if (ac==0777777) ac=0;
				}
				io=(io>>1|ac<<17)&0777777;
				ac>>=1;
			}
			break;
		case DIS_DIV:
			if(settings&BIT(S_HWMULDIV_B)) {	// DIV
				uint8_t scr, smb, srm;
				mb=memory[y];
				if(mb&0400000) smb=1;
				else {
					mb^=0777777;
					smb=0;
				}

				if(ac&0400000) {
					srm=1;
					ac^=0777777;
					io^=0777777;
				} else
					srm=0;
				scr=0;
				while(1) {
					ac+=mb;
					if (ac==0777777)		ac^=0777777;
					else if (ac&01000000)	ac=(ac+1)&0777777;
					if (mb&0400000)			mb^=0777777;
					if (scr==022 || (scr==0 && (ac&0400000)==0)) break;
					scr++;
					if ((ac&0400000)==0) mb^=0777777;
					t=(ac>>17)^1;
					ac=((ac<<1)|(io>>17))&0777777;
					io=((io<<1)|t)&0777777;
					if ((io&1)==0) ac=(ac+1)&0777777;
				}
				ac+=mb;
				if (ac==0777777)		ac^=0777777;
				else if (ac&01000000)	ac=(ac+1)&0777777;
				if (scr!=0) {
					pc++;
					ac>>=1;
				}
				if (srm && ac!=0777777) ac^=0777777;
				if (scr==0) {
					if (srm) io^=0777777;
					ctc+=2;
				} else {
					if (srm!=smb && io!=0) io^=0777777;
					mb=io;
					io=ac;
					ac=mb;
					ctc+=25; // average 35 us of 30-40 us
				}
			} else {	// DIS
				t=(ac>>17)^1;
				ac=(ac<<1|io>>17)&0777777;
				io=((io<<1|t)&0777777);
				if (t)	ac=ac^0777777+memory[y];
				else 	ac=((ac+1)&0777777)+memory[y];
				if(ac&01000000)	ac=(ac+1)&0777777;
				if(t)			ac^=0777777;
				if(ac==0777777)	ac=0;
			}
			break;
			
		case JMP: pc=y; break;
		case JSP: ac=(ov<<17)+pc; pc=y; break;
		case SKP:
			uint8_t cond =
				(((y&0100)==0100)&&(ac==0)) ||		// SZA
				(((y&0200)==0200)&&(ac>>17==0)) ||	// SPA
				(((y&0400)==0400)&&(ac>>17==1)) ||	// SMA
				(((y&01000)==01000)&&(ov==0)) ||	// SZO
				(((y&02000)==02000)&&(io>>17==0));	// SPI
			if ((y&01000)==01000) ov=0;
			if(cond==0&&(y&070)) {					// SZS
				t=(y&070)>>3;
				if(t==7) 		cond=(sense)?0:1;
				else if(t!=0)	cond=(sense&(1<<(t-1)))?0:1;
			}
			if(cond==0&&(y&07)) {					// SZF
				t=y&07;
				if(t==7)		cond=(flags)?0:1;
				else if(t!=0)	cond=(flags&(1<<(t-1)))?0:1;
			}
			if (cond^ib) {							// negate on i-bite
				if (xpc>=0)	pc=xpc+1
				else 		pc++;
			}
			/*
				(((y&7)!=0)&&!flag[y&7])||			// szf
				(((y&070)!=0)&&!sense[(y&070)>>3])||// szs
				((y&070)==010);
			if (ib==0) {if(cond) pc++;}
				else {if(!cond) pc++;}
			*/
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
			switch(y&077) {
				case 000:	// I/O Wait
					if(ib&&ioc) {
						ioc-=ctc;
						if(ioc>ctc) ctc=ioc;
						ioc=0;
					}
					break;
				case 007:	// dpy
					uint32_t px,py;
					if(settings&BIT(S_ECRT_B)) {
						if(settings&BIT(S_CRTSSORIG_B)) {
							switch((y>>9)&03) {
								case 0: // origin at center
									px=(ac & 0400000)? 511-((ac>>8)^01777) : 512+(ac>>8);
									py=(io & 0400000)? 512+((io>>8)^01777) : 511-(io>>8);
									break;
								case 1: // origin at bottom edge (center)
									px=(ac & 0400000)? 511-((ac>>8)^01777) : 512+(ac>>8);
									py=01777-((io>>8)&01777);
									break;
								case 2: // origin at middle left edge
									px=(ac>>8)&01777;
									py=(io & 0400000)? 512+((io>>8)^01777) : 511-(io>>8);
									break;
								case 3: // origin at lower left corner
									px=(ac>>8)&01777;
									py=01777-((io>>8)&01777);
									break;
							}
						} else {
							px=(ac & 0400000)? 511-((ac>>8)^01777) : 512+(ac>>8);
							py=(io & 0400000)? 512+((io>>8)^01777) : 511-(io>>8);
						}
						(*plotCRTPtr)(px, py, (y>>6)&07);
						status&=0377777;
						if(ib) {
							ctc+=45;
							ioc=0;
						} else if((md>>11)&2) {
							ioc=50;
						}
					} else {
						px = (ac+0400000)&0777777;
						py = (io+0400000)&0777777;
						px = x*550/0777777;
						py = y*550/0777777;
						(*plotCRTPtr)(px, py, 07);
					}
					break;
				case 011:	// Read paper pin block
					t=(settings&BIT(S_MERGECTRL_B))?control|testword:control;
					if(y&100) {
						io=(settings&BIT(S_ODDFRAMEFIX_B))?t&15:t&0740000;
						break;
					}
					io|=(settings&BIT(S_ROTATECTRL_B))?((t<<4)&0777777)|(t>>14):t;
				case 001:	// rpa
					t=tapeReadAlpha(md&014000==0);
					io=t;
					ctc=0;
				case 002:	// rpb
					t=tapeReadBin(md&014000==0);
					io=t;
					ctc=0;
				case 030:	// rrb
					t=tapeReadBuffer();
					io=t;
					ctc=0;
					break;
				case 033:	// cks
					io = status;
					break;
				default:
			}
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
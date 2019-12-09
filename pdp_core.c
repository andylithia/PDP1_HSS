#include "pdp_core.h"
#include "PDP_ISA.h"
// Main 4K 18bit memory
// Requires a MCU that has more than 12KBytes of RAM
// ii[5:0] = [5:0] {INDIRECT, INSTRUCTION[4:0]}
// ad[11:0] = [17:6] MEMORY_ADDR_Y[11:0]
// uint8_t pdp_memory_ii[4096];
// uint16_t pdp_memory_ad[4096];
uint32_t memory[4096];
uint8_t pdp_countdown;	// Memory Cycle Count-down
uint32_t ac, io, pc, y, ib, ov;
uint8_t flag = {0,0,0,0,0,0,0};
uint8_t sense = {0,0,0,0,0,0,0};
uint32_t control = 0;



void dispatch(uint32_t md) {
	y = md&07777;
	ib = (md>>12)&1;
	// IND Check
	switch(md>>13) {
		case SKP:
		case SFT:
		case LAW:
		case IOT:
		case OPR: break;
		// loop until we got the terminal indirect address
		default:  
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
		case IDX:
		case ISP:
		case SAD: if(ac!=memory[y]) pc++; break;
		case SAS: if(ac==memory[y]) pc++; break;
		case MUS:
		case DIS:
		case JMP: pc=y; break;
		case JSP: ac=(ov<<17)+pc; pc=y; break;
		case SKP:
		case SFT:
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
	}
}

void dpy() {
	x = (ac+0400000)&0777777;
	y = (io+0400000)&0777777;
	x = x*550/0777777;
	y = y*550/0777777;
	// TODO: write DAC
}

uint32_t os(uint32_t n) {
	n += 01000000;
	return '0';
}
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
uint8_t flag = {0,0,0,0,0,0,0};
uint8_t sense = {0,0,0,0,0,0,0};
uint32_t control = 0;
uint32_t k17=0400000, k18=01000000, k19=02000000, k35=0400000000000;

void PDP_step(){
	PDP_dispatch(memory[pc++]);
}

void PDP_dispatch(uint32_t md) {
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
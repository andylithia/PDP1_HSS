#ifndef _PDP_CORE_H
#define _PDP_CORE_H
#include "pdp_tapes.h"
// Fill Program Memory
#define PDP_M(A,D) do{ \
	pdp_memory_ii[(A)]=((D)&077); \
	pdp_memory_ad[(A)]=(((D)>>6)&07777); \
	}while(0)

typedef struct {
	uint8_t* name;
	uint32_t startAddr;
	uint8_t tapeLeng,
	uint8_t* content
} tape_t;

void PDP_step();
void PDP_initialize();
void PDP_resetMem();
void PDP_resetRegs();
void PDP_jump(uint16_t addr);
void PDP_dispatch(uint32_t md);
void PDP_mountTape(tape_t* tape);
void PDP_tapeReadIn();

// REGS
uint8_t  PDP_getTC();
void     PDP_clearTC();
void     PDP_setPC(uint32_t addr);
uint32_t PDP_getPC();
void     PDP_setSense(uint8_t n, uint8_t v);
uint8_t  PDP_getSense();
void     PDP_setControl(uint8_t n, uint8_t v);
uint32_t PDP_getControl();
void     PDP_setTestWord(uint8_t n, uint8_t v);
uint32_t PDP_getTestWord();

// SETTINGS
void PDP_setMulDiv(uint8_t d);
void PDP_setMergeControls(uint8_t d);
void PDP_setFrame(uint8_t d);
void PDP_setRotateControls(uint8_t d);
void PDP_setTrapCallback(void(*cb)(void*));
void PDP_setCRTCallback(void(*cb)(uint16_t, uint16_t, uint16_t));



#endif /* _PDP_CORE_H */
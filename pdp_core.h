#ifndef _PDP_CORE_H
#define _PDP_CORE_H

// Fill Program Memory
#define PDP_M(A,D) do{ \
	pdp_memory_ii[(A)]=((D)&077); \
	pdp_memory_ad[(A)]=(((D)>>6)&07777); \
	}while(0)

void PDP_step();
void PDP_jump(uint16_t addr);
void PDP_dispatch(uint32_t md);

#endif /* _PDP_CORE_H */
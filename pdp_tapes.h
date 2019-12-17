#ifndef _PDP_TAPES
#define _PDP_TAPES

typedef struct {
	uint8_t* name;
	uint32_t startAddr;
	uint8_t tapeLeng,
	uint8_t* content
} tape_t;



#endif /* _PDP_TAPES */
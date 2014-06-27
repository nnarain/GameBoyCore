
#include "gbapi.h"
#include "instructionset.h"
#include "opts.h"

#include <time.h>

/**
	execute the next optcode on the core

	@param core

	@param optCode
*/
int execute(struct Core* core, uint8_t optCode)
{
	uint8_t cycles;

	clock_t tick = clock();

	if(optCode != 0xCB){
		cycles = instructionSet1[optCode].cycles;
		instructionSet1[optCode].impl(core);
	}
	else{
		cycles = instructionSet2[optCode].cycles;
		instructionSet2[core->rom[++core->PC]].impl(core);
	}
 
	float elapseMillis = ( ((float)clock() - (float)tick) / (float)CLOCKS_PER_SEC ) * 1000.0f;

	while(elapseMillis < (CLK_PERIOD * 1000.0f * cycles)){
		elapseMillis = ( ((float)clock() - (float)tick) / (float)CLOCKS_PER_SEC ) * 1000.0f;
	}

	return 0;
}

/**
	@param core
*/
void initCore(struct Core* core)
{
	// Initialize all core members to 0
	core->AF.val = 0;
	core->BC.val = 0;
	core->DE.val = 0;
	core->HL.val = 0;
	core->SP     = WORK_RAM_BANK_0_END;
	core->PC     = 0x00;
}

/**
	Get a pointer to rom at the specified address

	@param core

	@param addr
*/
uint8_t* getAddress(struct Core* core, uint16_t addr)
{
	return &core->rom[addr];
}


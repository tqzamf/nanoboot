#include <stdint.h>
#include "cache.h"

void _main(void) __attribute__((noreturn));

void _init(void) __attribute__((section(".init")));
void _init(void) {
	// enable CP0 and disable all interrupts. zero the rest of the
	// register as well to get well-defined defaults.
	MTC0_STATUS(ST0_CU0);
	// clear all interrupt flags
	MTC0_CAUSE(0);
	
	// disable cache. it should be disabled by default, but make sure it
	// really is. if we initialize it while it's enabled, we risk massive
	// cache corruption.
	cache_disable();
	MTC0_TAGLO(0);
	MTC0_TAGHI(0);
	// these two are probably unnecessary unless there are separate D
	// cache tag registers.
	MTC0_DTAGLO(0);
	MTC0_DTAGHI(0);
	
	// setup CMEM registers. these are the values that Linux uses, except
	// for PCI which we don't use.
	MTC0_CMEM(0, CMEM_MMIO_BASE, CMEM_MMIO_SIZE);
	MTC0_CMEM(1, CMEM_XIO_BASE, CMEM_XIO_SIZE);
	MTC0_CMEM(2, 0, CMEM_DISABLED);
	MTC0_CMEM(3, 0, CMEM_DISABLED);

	// disable TLB and static mapping, and stop the timers. 
	// also fills any hazard slots for cache disable.
	uint32_t configPR = MFC0_CONFIGPR();
	configPR &= CPR_VALID;
	configPR &= ~(CPR_TLB | CPR_MAP);
	configPR |= CPR_T1 | CPR_T2 | CPR_T3;
	MTC0_CONFIGPR(configPR);

	// initialize both caches.
	cache_init();
	
	// enable caches. instruction fetches might not immediately use the
	// cache, but it should work anyway.
	// need to make sure we don't do the mtc0 in the branch delay slot
	// though; that's a bit TOO bold. 
	cache_enable();

	// ready to run C code in cached mode. enter main loop.
	// this over-complicated way of calling forces the compiler to jump
	// using an absolute address, so that it runs in KSEG0 (ie. cached)
	// even though the bootloader starts in KSEG1 (ie. uncached).
	void (*__main)(void) = _main;
	asm volatile("" : "=r" (__main) : "r" (__main));
	__main();
	// note: this code is so ugly it crashes the compiler. literally so;
	// if __main is an uint32_t and not declared as an output operand,
	// the above code actually causes an internal error in GCC 4.4.5...
}

#include <asm.h>

#in
#	r3	channel

#out
#	r3	res

.include "constants.inc"

# Emulated channels need nothing here. Slot B (channel 1) when it is not
# emulated is the real device: drop its chip select, keep the interrupt masks.
EXIDeselect:
	cmpwi	r3,	1
	bne		Done
	lis		r4,	NinCfgConfig@h
	lwz		r4,	NinCfgConfig@l(r4)
	andis.	r4,	r4,	NIN_CFG_MC_SLOTB@h
	bne		Done
	lis		r4,	0xCD80			# EXI channel 1 at 0xCD806814, as Patch.c maps EXI
	lwz		r5,	0x6814(r4)		# channel 1 CSR
	andi.	r5,	r5,	0x405
	stw		r5,	0x6814(r4)
Done:
	li		r3,	1
	blr

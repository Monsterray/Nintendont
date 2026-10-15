#include <asm.h>

#in
#	r3	channel
#	r4	device
#	r5	freq

#out
#	r3	res

.include "constants.inc"

EXISelect:
	# Slot B (channel 1) not emulated: select the real device, as the SDK
	# does: keep the interrupt masks (0x405), set chip select and clock.
	cmpwi	r3,	1
	bne		Emulated
	lis		r7,	NinCfgConfig@h
	lwz		r7,	NinCfgConfig@l(r7)
	andis.	r7,	r7,	NIN_CFG_MC_SLOTB@h
	bne		Emulated
	lis		r7,	0xCC00
	lwz		r6,	0x6814(r7)		# channel 1 CSR
	andi.	r6,	r6,	0x405
	li		r0,	0x80
	slw		r0,	r0,	r4		# CS: 1 << (device + 7)
	or		r6,	r6,	r0
	slwi	r0,	r5,	4		# CLK
	or		r6,	r6,	r0
	stw		r6,	0x6814(r7)
	li		r3,	1
	blr

Emulated:
	lis		r7,	0xD302
	lis		r0,	0x1000

	slwi	r4,	r4,	8
	slwi	r5,	r5,	16

	or		r0,	r0,	r3
	or		r0,	r0,	r4
	or		r0,	r0,	r5

	stw		r0,	EXI_CMD_0@l(r7)

ready_loop:
	lwz		r3,	EXI_CMD_0@l(r7)
	cmpw	r3,	r0
	beq		ready_loop

	lwz		r3,	EXI_CMD_1@l(r7)
	blr

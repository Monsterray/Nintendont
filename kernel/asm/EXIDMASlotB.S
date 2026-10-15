#include <asm.h>

#in
#	r3	channel (1)
#	r4	data
#	r5	len
#	r6	mode
#	r7	cb
#	r8	this channel's EXI control block (set by EXIDMA.S)

#out
#	r3	res

.include "constants.inc"

# EXIDMA.S branches here for Slot B when it is not emulated: run the DMA on
# the real channel 1, wait for it, and call the callback now, as the TC
# interrupt would. The game's own TC interrupt never sees channel 1 (Patch.c
# points its EXI accesses at the emulated registers). The microphone library
# calls EXIDma with interrupts off and checks only the result afterwards.
EXIDMASlotB:
	lis		r9,	0xCD80			# EXI channel 1 at 0xCD806814, as Patch.c maps EXI
	rlwinm	r10,	r4,	0,	6,	26	# physical address, 32-byte aligned
	stw		r10,	0x6818(r9)		# MAR
	stw		r5,	0x681C(r9)		# LENGTH
	slwi	r10,	r6,	2		# RW
	ori		r10,	r10,	3		# DMA | TSTART
	stw		r10,	0x6820(r9)		# CR
Wait:
	lwz		r10,	0x6820(r9)
	andi.	r10,	r10,	1
	bne		Wait

	li		r10,	0
	stw		r10,	0x4(r8)		# no transfer pending any more
	cmpwi	r7,	0
	beq		Done
	mflr	r0
	stw		r0,	4(%sp)
	stwu	%sp,	-0x10(%sp)
	mtctr	r7
	li		r3,	1
	li		r4,	0
	bctrl
	lwz		r0,	0x14(%sp)
	addi	%sp,	%sp,	0x10
	mtlr	r0
Done:
	li		r3,	1
	blr

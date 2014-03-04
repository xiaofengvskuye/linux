/*
 * Copyright 2014 Imagination Technologies, Ltd
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <asm/asm.h>
#include <asm/cacheops.h>
#include <asm/pm.h>
#include <asm/regdef.h>

	.extern _machine_restart

	.text
	.set noreorder
	.set noat
	.align	5

LEAF(malta_sleep)

	/* Save CPU state */
	SUSPEND_SAVE

	/*
	 * prefetch a few cachelines of instructions, as RAM will soon be
	 * inaccessible (this assumes 32byte cache lines).
	 */
	la	t0, 1f
	.set	mips3
	cache	Fill, 0(t0)
	cache	Fill, 32(t0)
	cache	Fill, 64(t0)
	cache	Fill, 96(t0)
	.set	mips0
	sync

1:
	/* Put RAM into self refresh here */

	/*
	 * Power down the processor here
	 * For Malta, we just restart the machine for now
	 */
	la	t1, _machine_restart
	LONG_L	t0, 0(t1)
	jalr	t0
	 nop

END(malta_sleep)


	/*
	 * This is the entry point after resume.
	 * Reload the CPU state and return.
	 */
LEAF(malta_sleep_wakeup)
	RESUME_RESTORE_RETURN
END(malta_sleep_wakeup)

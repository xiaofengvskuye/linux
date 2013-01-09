/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Chris Dearman (chris@mips.com)
 * Leonid Yegoshin (yegoshin@mips.com)
 * Copyright (C) 2012 Mips Technologies, Inc.
 */
#ifndef __ASM_MACH_MIPS_KERNEL_ENTRY_INIT_H
#define __ASM_MACH_MIPS_KERNEL_ENTRY_INIT_H

	.macro  eva_entry
#ifdef CONFIG_EVA_3GB
	li      t0, ((MIPS_SEGCFG_UK << MIPS_SEGCFG_AM_SHIFT) |             \
		(0 << MIPS_SEGCFG_PA_SHIFT) | (2 << MIPS_SEGCFG_C_SHIFT) |  \
		(1 << MIPS_SEGCFG_EU_SHIFT)) |                              \
		(((MIPS_SEGCFG_MK << MIPS_SEGCFG_AM_SHIFT) |                \
		(0 << MIPS_SEGCFG_PA_SHIFT) | (3 << MIPS_SEGCFG_C_SHIFT) |  \
		(1 << MIPS_SEGCFG_EU_SHIFT)) << 16)
	mtc0    t0, $5, 2
	li      t0, ((MIPS_SEGCFG_MUSK << MIPS_SEGCFG_AM_SHIFT) |             \
		(0 << MIPS_SEGCFG_PA_SHIFT) | (3 << MIPS_SEGCFG_C_SHIFT) |  \
		(1 << MIPS_SEGCFG_EU_SHIFT)) |                              \
		(((MIPS_SEGCFG_MUSUK << MIPS_SEGCFG_AM_SHIFT) |                \
		(4 << MIPS_SEGCFG_PA_SHIFT) | (3 << MIPS_SEGCFG_C_SHIFT) |  \
		(1 << MIPS_SEGCFG_EU_SHIFT)) << 16)
	mtc0    t0, $5, 3
#else
	li      t0, ((MIPS_SEGCFG_UK << MIPS_SEGCFG_AM_SHIFT) |             \
		(0 << MIPS_SEGCFG_PA_SHIFT) | (2 << MIPS_SEGCFG_C_SHIFT) |  \
		(1 << MIPS_SEGCFG_EU_SHIFT)) |                              \
		(((MIPS_SEGCFG_UK << MIPS_SEGCFG_AM_SHIFT) |                \
		(4 << MIPS_SEGCFG_PA_SHIFT) | (3 << MIPS_SEGCFG_C_SHIFT) |  \
		(1 << MIPS_SEGCFG_EU_SHIFT)) << 16)
	mtc0    t0, $5, 3
#endif
	jal	mips_ihb
	.endm


	.macro	kernel_entry_setup
#ifdef CONFIG_MIPS_MT_SMTC
	mfc0	t0, CP0_CONFIG
	bgez	t0, 9f
	mfc0	t0, CP0_CONFIG, 1
	bgez	t0, 9f
	mfc0	t0, CP0_CONFIG, 2
	bgez	t0, 9f
	mfc0	t0, CP0_CONFIG, 3
	and	t0, 1<<2
	bnez	t0, 0f
9:
	/* Assume we came from YAMON... */
	PTR_LA	v0, 0x9fc00534	/* YAMON print */
	lw	v0, (v0)
	move	a0, zero
	PTR_LA	a1, nonmt_processor
	jal	v0

	PTR_LA	v0, 0x9fc00520	/* YAMON exit */
	lw	v0, (v0)
	li	a0, 1
	jal	v0

1:	b	1b

	__INITDATA
nonmt_processor:
	.asciz	"SMTC kernel requires the MT ASE to run\n"
	__FINIT
0:
#endif

#ifdef CONFIG_EVA
	sync
	ehb

	mfc0    t0, CP0_CONFIG
	bgez	t0, 9f
	mfc0	t0, CP0_CONFIG, 1
	bgez	t0, 9f
	mfc0	t0, CP0_CONFIG, 2
	bgez	t0, 9f
	mfc0	t0, CP0_CONFIG, 3
	sll     t0, t0, 6   /* SC bit */
	bgez    t0, 9f

	eva_entry
	b       0f

9:
	/* Assume we came from YAMON... */
	PTR_LA	v0, 0x9fc00534	/* YAMON print */
	lw	v0, (v0)
	move	a0, zero
	PTR_LA  a1, nonsc_processor
	jal	v0

	PTR_LA	v0, 0x9fc00520	/* YAMON exit */
	lw	v0, (v0)
	li	a0, 1
	jal	v0

1:	b	1b
	nop

	__INITDATA
nonsc_processor:
	.asciz  "Kernel requires the Segment/EVA to run\n"
	__FINIT
#endif

0:
	.endm

/*
 * Do SMP slave processor setup necessary before we can safely execute C code.
 */
	.macro	smp_slave_setup

#ifdef CONFIG_EVA

	sync
	ehb
	eva_entry
#endif

	.endm

#endif /* __ASM_MACH_MIPS_KERNEL_ENTRY_INIT_H */

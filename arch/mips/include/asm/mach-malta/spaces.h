/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Author: Leonid Yegoshin (yegoshin@mips.com)
 * Copyright (C) 2012 MIPS Technologies, Inc.
 */

#ifndef _ASM_MALTA_SPACES_H
#define _ASM_MALTA_SPACES_H

#ifdef CONFIG_EVA

/* Malta board EVA memory map basic's:

   Phys memory - 80000000 to ffffffff - 2GB (last 64KB reserved for
		 correct HIGHMEM macros arithmetics)
   KV memory   - 0 - 7fffffff (2GB) or even higher
   Kernel code is located in the same place (80000000) just for keeping
		 the same YAMON and other stuff, so KSEG0 is used "illegally",
		 using Malta mirroring of 1st 256MB (see also __pa_symbol below)
   IO/UNCAC_ADDR ... well, even KSEG1 or KSEG3 but physaddr is 0UL (256MB-512MB)
   CAC_ADDR      ... it used to revert effect of UNCAC_ADDR
   VMALLOC is cut to C0000000 to E0000000 (KSEG2)
   PKMAP/kmap_coherent should be not used - no HIGHMEM
 */

#define PAGE_OFFSET             _AC(0x0, UL)
#define PHYS_OFFSET             _AC(0x80000000, UL)
#define HIGHMEM_START           _AC(0xffff0000, UL)

/* trick definition, just to use kernel symbols from KSEG0 but keep
   all dynamic memory in EVA's MUSUK KUSEG segment - I am lazy
   to move kernel code from 80000000 to zero
   Don't copy it for other boards, it is likely to have a different kernel
   location */
#define __pa_symbol(x)          (RELOC_HIDE((unsigned long)(x), 0))

/*  I put INDEX_BASE here to underline the fact that in EVA mode kernel
    may be located somethere and not in CKSEG0, so CKSEG0 may have
    a "surprise" location and index-based CACHE may give unexpected result */
#define INDEX_BASE      CKSEG0

/*
 * If the Instruction Pointer is in module space (0xc0000000), return true;
 * otherwise, it is in kernel space (0x80000000), return false.
 *
 * FIXME: This will not work when the kernel space and module space are the
 * same. If they are the same, we need to modify scripts/recordmcount.pl,
 * ftrace_make_nop/call() and the other related parts to ensure the
 * enabling/disabling of the calling site to _mcount is right for both kernel
 * and module.
 *
 * It must be changed for 3.5GB memory map. LY22
 */
#define in_module(ip)   (((unsigned long)ip) & 0x40000000)

#ifdef CONFIG_EVA_3GB

#define UNCAC_BASE              _AC(0xe0000000, UL)
#define IO_BASE                 UNCAC_BASE

#define KSEG
#define KUSEG                   0x00000000
#define KSEG0                   0x80000000
#define KSEG3                   0xa0000000
#define KSEG2                   0xc0000000
#define KSEG1                   0xe0000000

#define CKUSEG                  0x00000000
#define CKSEG0                  0x80000000
#define CKSEG3                  0xa0000000
#define CKSEG2                  0xc0000000
#define CKSEG1                  0xe0000000

#define MAP_BASE                CKSEG2
#define VMALLOC_END             (MAP_BASE + _AC(0x20000000, UL) - 2*PAGE_SIZE)

#endif  /* CONFIG_EVA_3GB */

#endif  /* CONFIG_EVA */

#include <asm/mach-generic/spaces.h>

#endif /* __ASM_MALTA_SPACES_H */

#ifndef __ASM_MMU_H
#define __ASM_MMU_H

typedef struct {
	unsigned long asid[NR_CPUS];
	void *vdso;
} mm_context_t;

#if (_MIPS_SZPTR == 32)
#define PTR	.word
#endif

#if (_MIPS_SZPTR == 64)
#define PTR	.dword
#endif

#define STR(x)  __STR(x)
#define __STR(x)  #x

#if (_MIPS_SZLONG == 32)
#define L_ADDIU   "addiu"
#endif

#if (_MIPS_SZLONG == 64)
#define L_ADDIU   "daddiu"
#endif

#ifndef CONFIG_MIPS_MT_SMTC
#define ASID_INC(asid)						\
({								\
	unsigned long __asid = asid;				\
	__asm__("1:\t" L_ADDIU "\t%0,1\t\t\t\t# patched\n\t"          \
	".section\t__asid_inc,\"a\"\n\t"			\
	STR(PTR)"\t1b\n\t"                                         \
	"\t.previous"                                             \
	:"=r" (__asid)						\
	:"0" (__asid));						\
	__asid;							\
})
#define ASID_MASK(asid)						\
({								\
	unsigned long __asid = asid;				\
	__asm__("1:\tandi\t%0,%1,0xfc0\t\t\t# patched\n\t"      \
	".section\t__asid_mask,\"a\"\n\t"			\
	STR(PTR)"\t1b\n\t"                                         \
	".previous"						\
	:"=r" (__asid)						\
	:"r" (__asid));						\
	__asid;							\
})
#define ASID_VERSION_MASK					\
({								\
	unsigned long __asid;					\
	__asm__("1:\t" L_ADDIU "\t%0,$0,0xff00\t\t\t\t# patched\n\t"        \
	".section\t__asid_version_mask,\"a\"\n\t"		\
	STR(PTR)"\t1b\n\t"                                         \
	".previous"						\
	:"=r" (__asid));					\
	__asid;							\
})
#define ASID_FIRST_VERSION					\
({								\
	unsigned long __asid = asid;				\
	__asm__("1:\tli\t%0,0x100\t\t\t\t# patched\n\t"		\
	".section\t__asid_first_version,\"a\"\n\t"		\
	STR(PTR)"\t1b\n\t"                                         \
	".previous"						\
	:"=r" (__asid));					\
	__asid;							\
})

#define ASID_FIRST_VERSION_R3000	0x1000
#define ASID_FIRST_VERSION_R4000	0x100
#define ASID_FIRST_VERSION_R8000	0x1000
#define ASID_FIRST_VERSION_RM9000	0x1000
#define ASID_FIRST_VERSION_99K		0x1000

#else
/* SMTC/34K debug hack. */
#define ASID_INC	0x1
extern unsigned long smtc_asid_mask;
#define ASID_MASK	(smtc_asid_mask)
#define	HW_ASID_MASK	0xff
#endif

#endif /* __ASM_MMU_H */

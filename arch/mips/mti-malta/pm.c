/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Imagination Technologies Ltd.
 */

#include <linux/init.h>
#include <linux/suspend.h>

#include <asm/cacheflush.h>
#include <asm/mach-malta/suspend.h>
#include <asm/pm.h>
#include <asm/tlbflush.h>

#define EXC_VECTOR_SIZE 0x400

/**
 * struct malta_suspend_state - State to save across suspend to RAM.
 * @ebase:	CP0 EBase register.
 * @excvec:	Memory for saving exception vector.
 *
 * This structure contains state that must be saved across suspend to RAM, but
 * can be done so by C code and allocated dynamically.
 */
struct malta_suspend_state {
	u32 ebase;
	void *excvec;
};

/* Suspend state storage */
struct malta_suspend_state *malta_suspend_state;

struct malta_suspend_state *malta_alloc_suspend_state(void)
{
	struct malta_suspend_state *s;

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return NULL;

	/* Allocate memory to save exception vector */
	s->excvec = kmalloc(EXC_VECTOR_SIZE, GFP_KERNEL);
	if (!s->excvec) {
		kfree(s);
		return NULL;
	}

	return s;
}

void malta_free_suspend_state(struct malta_suspend_state *s)
{
	kfree(s->excvec);
	kfree(s);
}

void malta_save_suspend_state(struct malta_suspend_state *s)
{
	void *ebase_addr;

	/* Save exception vector */
	s->ebase = read_c0_ebase();
	ebase_addr = (void *)(long)(s32)(s->ebase & 0xfffff000);
	memcpy(s->excvec, ebase_addr, EXC_VECTOR_SIZE);
}

void malta_restore_suspend_state(const struct malta_suspend_state *s)
{
	void *ebase_addr;

	/* Restore CP0 state */
	write_c0_ebase(s->ebase);

	/* Restore exception vector */
	ebase_addr = (void *)(long)(s32)(s->ebase & 0xfffff000);
	memcpy(ebase_addr, s->excvec, EXC_VECTOR_SIZE);

	/* flush exception vector from icache so it gets used */
	local_flush_icache_range((unsigned long)ebase_addr,
				 (unsigned long)ebase_addr + EXC_VECTOR_SIZE);

	/* flush any pre-existing TLB entries */
	local_flush_tlb_all();
}

static int malta_pm_enter(suspend_state_t state)
{
	/* Save important CPU state */
	malta_save_suspend_state(malta_suspend_state);

	/*
	 * Perform the sleep.
	 * After wake, this will return.
	 */
	malta_sleep();

	/* Restore important CPU state */
	malta_restore_suspend_state(malta_suspend_state);

	return 0;
}

static int malta_pm_begin(suspend_state_t state)
{
	/*
	 * Inform the user where to resume from.
	 * Ideally the resume address would have been saved in a scratch
	 * register somewhere from sleep asm code, to allow the bootloader to
	 * automatically resume from this address.
	 */
	pr_info("*** Please resume from %p\n", malta_sleep_wakeup);

	malta_suspend_state = malta_alloc_suspend_state();
	if (!malta_suspend_state)
		return -ENOMEM;

	return 0;
}

static void malta_pm_end(void)
{
	malta_free_suspend_state(malta_suspend_state);
}

static const struct platform_suspend_ops malta_pm_ops = {
	.valid		= suspend_valid_only_mem,
	.begin		= malta_pm_begin,
	.enter		= malta_pm_enter,
	.end		= malta_pm_end,
};

/*
 * Initialize suspend interface
 */
static int __init malta_pm_init(void)
{
	suspend_set_ops(&malta_pm_ops);

	return 0;
}

late_initcall(malta_pm_init);

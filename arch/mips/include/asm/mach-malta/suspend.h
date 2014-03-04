/*
 * Copyright (C) 2014 Imagination Technologies Ltd
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Malta Suspend to RAM functions.
 */

#ifndef __ASM_MALTA_SUSPEND_H
#define __ASM_MALTA_SUSPEND_H

/* PM: arch/mips/mti-malta/suspend.S */
void malta_sleep(void);
void malta_sleep_wakeup(void);

#endif /* __ASM_MALTA_SUSPEND_H */

/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/screen_info.h>

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/irq.h>
#include <asm/mips-boards/generic.h>
#include <asm/mips-boards/prom.h>
#include <asm/mips-boards/malta.h>
#include <asm/mips-boards/maltaint.h>
#include <asm/dma.h>
#include <asm/time.h>
#include <asm/traps.h>
#include <asm/gcmpregs.h>
#ifdef CONFIG_VT
#include <linux/console.h>
#endif

extern void mips_reboot_setup(void);
extern void mips_time_init(void);
extern unsigned long mips_rtc_get_time(void);

#ifdef CONFIG_KGDB
extern void kgdb_config(void);
#endif

struct resource standard_io_resources[] = {
	{ .name = "dma1", .start = 0x00, .end = 0x1f, .flags = IORESOURCE_BUSY },
	{ .name = "timer", .start = 0x40, .end = 0x5f, .flags = IORESOURCE_BUSY },
	{ .name = "keyboard", .start = 0x60, .end = 0x6f, .flags = IORESOURCE_BUSY },
	{ .name = "dma page reg", .start = 0x80, .end = 0x8f, .flags = IORESOURCE_BUSY },
	{ .name = "dma2", .start = 0xc0, .end = 0xdf, .flags = IORESOURCE_BUSY },
};

const char *get_system_type(void)
{
	return "MIPS Malta";
}

#if defined(CONFIG_MIPS_MT_SMTC)
const char display_string[] = "       SMTC LINUX ON MALTA       ";
#else
const char display_string[] = "        LINUX ON MALTA       ";
#endif /* CONFIG_MIPS_MT_SMTC */

#ifdef CONFIG_BLK_DEV_FD
void __init fd_activate(void)
{
	/*
	 * Activate Floppy Controller in the SMSC FDC37M817 Super I/O
	 * Controller.
	 * Done by YAMON 2.00 onwards
	 */
	/* Entering config state. */
	SMSC_WRITE(SMSC_CONFIG_ENTER, SMSC_CONFIG_REG);

	/* Activate floppy controller. */
	SMSC_WRITE(SMSC_CONFIG_DEVNUM, SMSC_CONFIG_REG);
	SMSC_WRITE(SMSC_CONFIG_DEVNUM_FLOPPY, SMSC_DATA_REG);
	SMSC_WRITE(SMSC_CONFIG_ACTIVATE, SMSC_CONFIG_REG);
	SMSC_WRITE(SMSC_CONFIG_ACTIVATE_ENABLE, SMSC_DATA_REG);

	/* Exit config state. */
	SMSC_WRITE(SMSC_CONFIG_EXIT, SMSC_CONFIG_REG);
}
#endif

int coherentio = -1;
static int __init setcoherentio(char *str)
{
	if (coherentio < 0)
		printk(KERN_INFO "Command line checking done before plat_setup_iocoherency!!\n");
	if (coherentio == 0)
		printk(KERN_INFO "Command line enabling coherentio (this will break...)!!\n");

	coherentio = 1;
	printk(KERN_INFO "Hardware DMA cache coherency (command line)\n");
	return 1;
}
__setup("coherentio", setcoherentio);

static int __init setnocoherentio(char *str)
{
	if (coherentio < 0)
		printk(KERN_INFO "Command line checking done before plat_setup_iocoherency!!\n");
	if (coherentio == 1)
		printk(KERN_INFO "Command line disabling coherentio\n");

	coherentio = 0;
	printk(KERN_INFO "Software DMA cache coherency (command line)\n");
	return 1;
}
__setup("nocoherentio", setnocoherentio);

static int __init
plat_enable_iocoherency(void)
{
	int supported = 0;
	if (mips_revision_sconid == MIPS_REVISION_SCON_BONITO) {
		if (BONITO_PCICACHECTRL & BONITO_PCICACHECTRL_CPUCOH_PRES) {
			BONITO_PCICACHECTRL |= BONITO_PCICACHECTRL_CPUCOH_EN;
			printk(KERN_INFO "Enabled Bonito CPU coherency\n");
			supported = 1;
		}
		if (strstr(prom_getcmdline(), "iobcuncached")) {
			BONITO_PCICACHECTRL &= ~BONITO_PCICACHECTRL_IOBCCOH_EN;
			BONITO_PCIMEMBASECFG = BONITO_PCIMEMBASECFG &
				~(BONITO_PCIMEMBASECFG_MEMBASE0_CACHED |
				  BONITO_PCIMEMBASECFG_MEMBASE1_CACHED);
			printk(KERN_INFO "Disabled Bonito IOBC coherency\n");
		} else {
			BONITO_PCICACHECTRL |= BONITO_PCICACHECTRL_IOBCCOH_EN;
			BONITO_PCIMEMBASECFG |=
				(BONITO_PCIMEMBASECFG_MEMBASE0_CACHED |
				 BONITO_PCIMEMBASECFG_MEMBASE1_CACHED);
			printk(KERN_INFO "Enabled Bonito IOBC coherency\n");
		}
	} else if (gcmp_niocu () != 0) {
		/* Nothing special needs to be done to enable coherency */
		printk(KERN_INFO "CMP IOCU detected\n");
		if ((*(unsigned int *)0xbf403000 & 0x81) != 0x81) {
		  printk(KERN_CRIT "IOCU OPERATION DISABLED BY SWITCH - DEFAULTING TO SW IO COHERENCY\n");
		  return 0;
		}
		supported = 1;
	}
	return supported;
}

static void __init
plat_setup_iocoherency(void)
{
#ifdef CONFIG_DMA_NONCOHERENT
	/*
	 * Kernel has been configured with software coherency
	 * but we might choose to turn it off
	 */
	if (plat_enable_iocoherency()) {
		if (coherentio == 0)
			printk(KERN_INFO "Hardware DMA cache coherency supported but disabled from command line\n");
		else {
			coherentio = 1;
			printk(KERN_INFO "Hardware DMA cache coherency\n");
		}
	} else {
		if (coherentio == 1)
			printk(KERN_INFO "Hardware DMA cache coherency not supported but enabled from command line\n");
		else {
			coherentio = 0;
			printk(KERN_INFO "Software DMA cache coherency\n");
		}
	}
#else
	if (!plat_enable_iocoherency())
		panic("Hardware DMA cache coherency not supported");
#endif
}

void __init plat_mem_setup(void)
{
	unsigned int i;

	mips_pcibios_init();

	/* Request I/O space for devices used on the Malta board. */
	for (i = 0; i < ARRAY_SIZE(standard_io_resources); i++)
		request_resource(&ioport_resource, standard_io_resources+i);

	/*
	 * Enable DMA channel 4 (cascade channel) in the PIIX4 south bridge.
	 */
	enable_dma(4);

#ifdef CONFIG_KGDB
	kgdb_config ();
#endif

	if (mips_revision_sconid == MIPS_REVISION_SCON_BONITO) {
		if (strstr(prom_getcmdline(), "debug")) {
			BONITO_BONGENCFG |= BONITO_BONGENCFG_DEBUGMODE;
			printk ("Enabled Bonito debug mode\n");
		}
		else
			BONITO_BONGENCFG &= ~BONITO_BONGENCFG_DEBUGMODE;
	}

	plat_setup_iocoherency();

#ifdef CONFIG_BLK_DEV_IDE
	/* Check PCI clock */
	{
		unsigned int __iomem *jmpr_p = (unsigned int *) ioremap(MALTA_JMPRS_REG, sizeof(unsigned int));
		int jmpr = (__raw_readl(jmpr_p) >> 2) & 0x07;
		static int pciclocks[] __initdata = {
			33, 20, 25, 30, 12, 16, 37, 10
		};
		int pciclock = pciclocks[jmpr];
		char *argptr = prom_getcmdline();

		if (pciclock != 33 && !strstr (argptr, "idebus=")) {
			printk("WARNING: PCI clock is %dMHz, setting idebus\n", pciclock);
			argptr += strlen(argptr);
			sprintf (argptr, " idebus=%d", pciclock);
			if (pciclock < 20 || pciclock > 66)
				printk ("WARNING: IDE timing calculations will be incorrect\n");
		}
	}
#endif
#ifdef CONFIG_BLK_DEV_FD
	fd_activate ();
#endif
#ifdef CONFIG_VT
#if defined(CONFIG_VGA_CONSOLE)
	screen_info = (struct screen_info) {
		0, 25,			/* orig-x, orig-y */
		0,			/* unused */
		0,			/* orig-video-page */
		0,			/* orig-video-mode */
		80,			/* orig-video-cols */
		0,0,0,			/* ega_ax, ega_bx, ega_cx */
		25,			/* orig-video-lines */
		VIDEO_TYPE_VGAC,	/* orig-video-isVGA */
		16			/* orig-video-points */
	};
#endif
#endif
	mips_reboot_setup();

	board_time_init = mips_time_init;
	rtc_mips_get_time = mips_rtc_get_time;
}

/* Enable PCI 2.1 compatibility in PIIX4 */
static void __init quirk_dlcsetup(struct pci_dev *dev)
{
	  u8 odlc, ndlc;
	(void) pci_read_config_byte(dev, 0x82, &odlc);
	/* Enable passive releases and delayed transaction */
	ndlc = odlc | 7;
	(void) pci_write_config_byte(dev, 0x82, ndlc);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371AB_0,
			quirk_dlcsetup);

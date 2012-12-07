/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2012 MIPS Technologies, Inc.  All rights reserved.
 */
#include <linux/init.h>
#include <linux/string.h>
#include <linux/cpu.h>
#include <linux/percpu.h>

#include <asm/page.h>


#define __BUILD_CP0_SYSFS(reg)					\
static DEFINE_PER_CPU(unsigned int, cpu_config##reg);		\
static ssize_t show_config##reg(struct device *dev,		\
		struct device_attribute *attr, char *buf)	\
{								\
	struct cpu *cpu = container_of(dev, struct cpu, dev);	\
	int n = snprintf(buf, PAGE_SIZE-2, "%x\n",		\
		per_cpu(cpu_config##reg, cpu->dev.id));		\
	return n;						\
}								\
static DEVICE_ATTR(config##reg, 0444, show_config##reg, NULL);

__BUILD_CP0_SYSFS(0)
__BUILD_CP0_SYSFS(1)
__BUILD_CP0_SYSFS(2)
__BUILD_CP0_SYSFS(3)
__BUILD_CP0_SYSFS(4)
__BUILD_CP0_SYSFS(5)
__BUILD_CP0_SYSFS(6)
__BUILD_CP0_SYSFS(7)

static void read_c0_registers(void *arg)
{
	struct device *dev = get_cpu_device(smp_processor_id());
	struct cpu *cpu;
	int ok;

	if (dev != NULL) {
		cpu = container_of(dev, struct cpu, dev);
		per_cpu(cpu_config0, cpu->dev.id) = read_c0_config();
		device_create_file(dev, &dev_attr_config0);
		ok = per_cpu(cpu_config0, cpu->dev.id) & MIPS_CONF_M;
	} else
		return;

	if (ok) {
		per_cpu(cpu_config1, cpu->dev.id) = read_c0_config1();
		device_create_file(dev, &dev_attr_config1);
		ok = per_cpu(cpu_config1, cpu->dev.id) & MIPS_CONF_M;
	}
	if (ok) {
		per_cpu(cpu_config2, cpu->dev.id) = read_c0_config2();
		device_create_file(dev, &dev_attr_config2);
		ok = per_cpu(cpu_config2, cpu->dev.id) & MIPS_CONF_M;
	}
	if (ok) {
		per_cpu(cpu_config3, cpu->dev.id) = read_c0_config3();
		device_create_file(dev, &dev_attr_config3);
		ok = per_cpu(cpu_config3, cpu->dev.id) & MIPS_CONF_M;
	}
	if (ok) {
		per_cpu(cpu_config4, cpu->dev.id) = read_c0_config4();
		device_create_file(dev, &dev_attr_config4);
		ok = per_cpu(cpu_config4, cpu->dev.id) & MIPS_CONF_M;
	}
	if (ok) {
		per_cpu(cpu_config5, cpu->dev.id) = read_c0_config5();
		device_create_file(dev, &dev_attr_config5);
		ok = per_cpu(cpu_config5, cpu->dev.id) & MIPS_CONF_M;
	}
	if (ok) {
		per_cpu(cpu_config6, cpu->dev.id) = read_c0_config6();
		device_create_file(dev, &dev_attr_config6);
		ok = per_cpu(cpu_config6, cpu->dev.id) & MIPS_CONF_M;
	}
	if (ok) {
		per_cpu(cpu_config7, cpu->dev.id) = read_c0_config7();
		device_create_file(dev, &dev_attr_config7);
		ok = per_cpu(cpu_config7, cpu->dev.id) & MIPS_CONF_M;
	}
}

static int __init mips_sysfs_registers(void)
{
	on_each_cpu(read_c0_registers, NULL, 1);
	return 0;
}
late_initcall(mips_sysfs_registers);

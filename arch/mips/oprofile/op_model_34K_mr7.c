/*
 * First cut implementation - checking SMTC support
 */

#define DEBUG

#include <linux/oprofile.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <asm/irq_regs.h>
#include <asm/mipsmtregs.h>
#include "op_impl.h"

#define M_PERFCTL_EXL			(1UL      <<  0)
#define M_PERFCTL_KERNEL		(1UL      <<  1)
#define M_PERFCTL_SUPERVISOR		(1UL      <<  2)
#define M_PERFCTL_USER			(1UL      <<  3)
#define M_PERFCTL_INTERRUPT_ENABLE	(1UL      <<  4)
#define M_PERFCTL_EVENT(event)		(((event) & 0x3f)  << 5)
#define M_PERFCTL_VPEID(vpe)		((vpe)    << 16)
#define M_PERFCTL_MT_EN(filter)		((filter) << 20)
#define    M_TC_EN_ALL			M_PERFCTL_MT_EN(0)
#define    M_TC_EN_VPE			M_PERFCTL_MT_EN(1)
#define    M_TC_EN_TC			M_PERFCTL_MT_EN(2)
#define PERFCTL_TCID_SHIFT		22
#define PERFCTL_TCID_MSK		(0xff << PERFCTL_TCID_SHIFT)
#define PERFCTL_TCID_GET(control)	((control & PERFCTL_TCID_MSK) 	\
						>> PERFCTL_TCID_SHIFT)
#define M_PERFCTL_TCID(tcid)		((tcid)   << PERFCTL_TCID_SHIFT)
#define M_PERFCTL_WIDE			(1UL      << 30)
#define M_PERFCTL_MORE			(1UL      << 31)
#define M_COUNTER_OVERFLOW		(1UL      << 31)
#define WHAT				(M_TC_EN_TC | \
					  M_PERFCTL_TCID(smp_processor_id()))
#define NUM_MAX_COUNTERS		18
#define PRINT_FUNC_NAME()		pr_debug("Function %s called\n", \
						__FUNCTION__)

static unsigned int first_free_tc;
static unsigned int free_tcs_per_active_tc;
static unsigned int phys_cntrs_per_tc;
static unsigned int use_free_counters;

/* Ideally we would want these accessors to work with any number of counters
 * but that is not possible because the mftc/mttc macros expect constant
 * arguments. No preprocessor munging.
 */
static unsigned int r_c0_perfcntr(int tc, int num)
{
	switch (num) {
	case 0:
		settc(tc);
		return mftc0(25,1);
	case 1:
		settc(tc);
		return mftc0(25,3);
	case 2:
		settc(tc);
		return mftc0(25,5);
	case 3:
		settc(tc);
		return mftc0(25,7);
	default:
		BUG();
	}
	return 0;
}

static unsigned int r_c0_perfctrl(int tc, int num)
{
	switch (num) {
	case 0:
		settc(tc);
		return mftc0(25,0);
	case 1:
		settc(tc);
		return mftc0(25,2);
	case 2:
		settc(tc);
		return mftc0(25,4);
	case 3:
		settc(tc);
		return mftc0(25,6);
	default:
		BUG();
	}
	return 0;
}

static void w_c0_perfcntr(int tc, int num, int val)
{
	switch (num) {
	case 0:
		settc(tc);
		mttc0(25,1,val);
		ehb();
		break;
	case 1:
		settc(tc);
		mttc0(25,3,val);
		ehb();
		break;
	case 2:
		settc(tc);
		mttc0(25,5,val);
		ehb();
		break;
	case 3:
		settc(tc);
		mttc0(25,7,val);
		ehb();
		break;
	default:
		BUG();
	};
}

static void w_c0_perfctrl(int tc, int num, int val)
{
	switch (num) {
	case 0:
		settc(tc);
		mttc0(25,0,val);
		ehb();
		break;
	case 1:
		settc(tc);
		mttc0(25,2,val);
		ehb();
		break;
	case 2:
		settc(tc);
		mttc0(25,4,val);
		ehb();
		break;
	case 3:
		settc(tc);
		mttc0(25,6,val);
		ehb();
		break;
	default:
		BUG();
	};
}

/* Find the target "free" TC associated with an active TC */
static unsigned int __targettc(int reference_tc, int counter)
{
	unsigned int retval;

	if (counter < phys_cntrs_per_tc)
		retval = reference_tc;
	else
		retval = (first_free_tc + 
				(free_tcs_per_active_tc * reference_tc) + 
					(counter/phys_cntrs_per_tc) - 1);
	return retval;
}

/* Access a given TC's virtual counters. ie counter > phys_cntrs_per_tc and
 * present on an associated unused TC
 */
#define __accessors(r)							\
									\
static unsigned int read_ ## r (int tc, int counter)			\
{									\
	int targettc;							\
	targettc = __targettc(tc, counter);				\
	return r_c0_ ## r (targettc, (counter % phys_cntrs_per_tc));	\
}									\
									\
static void write_ ## r (int tc, int counter, int value)		\
{									\
	int targettc;							\
	targettc = __targettc(tc, counter);				\
	w_c0_ ## r (targettc, (counter % phys_cntrs_per_tc), value);	\
}

__accessors(perfcntr)
__accessors(perfctrl)

static int __init get_first_free_tc(char *str)
{
	get_option(&str, &first_free_tc);
	return 1;
}

static int __init set_use_free_counters(char *str)
{
	use_free_counters = 1;
	return 1;
}

/* 
 * Specify the first free tc to harvest for counters.
 * This allows a "window" of TCs to be left unused.
 */
__setup("fftc=", get_first_free_tc);

/*
 *  Specifying this pulls in free counters, if any
 */
__setup("ufcntrs", set_use_free_counters);

/*
 * 	Try and discover the best fit :
 * 	1. Free TCs that can be harvested
 * 	2. Free counters per active TC
 * 	3. Total Oprofile Counters in the system
 */
static int __init counter_layout(void)
{
	unsigned int free_counters = 0, free_counters_per_tc = 0, free_tcs = 0;
	unsigned int total_tcs = 0, active_tcs = 0;

	/* Are we to use free counters at all ? */
	if (!use_free_counters)
		return phys_cntrs_per_tc;

	active_tcs = num_online_cpus();

#ifdef CONFIG_MIPS_MT_SMTC
	total_tcs = num_possible_cpus();
#elif defined(CONFIG_MIPS_MT_SMP)
	total_tcs = (read_c0_mvpconf0() & MVPCONF0_PTC);
	total_tcs >>= MVPCONF0_PTC_SHIFT;
	total_tcs += 1;
#else
	return phys_cntrs_per_tc;
#endif
	/* Do we use a specific TC as a starting point for harvesting
	 * counters, leaving an unused window of TCs for other purposes ?
	 */
	if (!first_free_tc)
		first_free_tc = num_online_cpus();

	free_tcs = total_tcs - first_free_tc;
	/* Round down to an even set */
	free_tcs &= ~(0x1);
	free_tcs_per_active_tc = free_tcs/active_tcs;
	free_counters = (free_tcs * phys_cntrs_per_tc);
	free_counters_per_tc = free_counters / active_tcs;	

	pr_debug("oprofile : \n\ttotal_tcs = %d, active_tcs = %d\n",
			total_tcs, active_tcs);
	pr_debug("\tfree_tcs = %d, free_tcs_per_active_tc = %d\n",
			free_tcs, free_tcs_per_active_tc);
	pr_debug("\ttotal_free_counters : %d, free_counters_per_tc = %d\n",
			free_counters, free_counters_per_tc);
	pr_debug("\tfirst_free_tc = %d\n", first_free_tc);

	return (free_counters_per_tc + phys_cntrs_per_tc);
}

struct op_mips_model op_model_34K_mr7_ops;

static struct mips_register_config {
	unsigned int control[NUM_MAX_COUNTERS];
	unsigned int counter[NUM_MAX_COUNTERS];
} reg;

#ifdef CONFIG_MIPS_MT_SMTC
extern int smtc_log_sample(unsigned long pc, int is_kernel, 
			unsigned long event, unsigned int tc);

static int mips_34K_perfcount_handler(void)
{
	unsigned int counters = op_model_34K_mr7_ops.num_counters;
	unsigned int counter, control, cp0_status, pc, is_kernel;
	int i, tc, handled = IRQ_NONE;

	if (cpu_has_mips_r2 && !(read_c0_cause() & (1 << 26)))
		return handled;

	for_each_online_cpu(tc) {
		for (i = 0; i < counters; i++) {
			control = read_perfctrl(tc, i);
			counter = read_perfcntr(tc, i);
			if ((control & M_PERFCTL_INTERRUPT_ENABLE) &&
				(counter & M_COUNTER_OVERFLOW)) {
				if (PERFCTL_TCID_GET(control) == smp_processor_id()) {
					oprofile_add_sample(get_irq_regs(), i);
					write_perfcntr(tc, i, reg.counter[i]);
					return IRQ_HANDLED;
				}
				else {
					settc(tc);
        				write_tc_c0_tchalt(TCHALT_H);
        				instruction_hazard();
					settc(tc);
        				cp0_status = read_tc_c0_tcstatus();
					settc(tc);
        				pc = read_tc_c0_tcrestart();
        				is_kernel = !((cp0_status & KU_MASK) == KU_USER);
        				smtc_log_sample (pc, is_kernel, i, tc);
					write_perfcntr(tc, i, reg.counter[i]);
					settc(tc);
					write_tc_c0_tchalt(0);
					instruction_hazard();
					return IRQ_HANDLED;
				}
			}
		}
	}
	return handled;
}
#else
static int mips_34K_perfcount_handler(void)
{
	unsigned int counters = op_model_34K_mr7_ops.num_counters;
	unsigned int control, counter, i;
	int handled = IRQ_NONE;

	if (cpu_has_mips_r2 && !(read_c0_cause() & (1 << 26)))
		return handled;

	for (i = 0; i < counters; i++) {
		control = read_perfctrl(smp_processor_id(), i);
		counter = read_perfcntr(smp_processor_id(), i);
		if ((control & M_PERFCTL_INTERRUPT_ENABLE) &&
		    (counter & M_COUNTER_OVERFLOW)) {
			oprofile_add_sample(get_irq_regs(), i);
			write_perfcntr(smp_processor_id(), i, reg.counter[i]);
			handled = IRQ_HANDLED;
		}
	}
	return handled;
}
#endif

/* Compute all of the registers in preparation for enabling profiling.  */
static void mips_34K_reg_setup(struct op_counter_config *ctr)
{
	unsigned int counters = op_model_34K_mr7_ops.num_counters;
	int i;

	PRINT_FUNC_NAME();

	/* Compute the performance counter control word.  */
	for (i = 0; i < counters; i++) {
		reg.control[i] = 0;
		reg.counter[i] = 0;

		if (!ctr[i].enabled)
			continue;

		reg.control[i] = M_PERFCTL_EVENT(ctr[i].event) |
		                 M_PERFCTL_INTERRUPT_ENABLE;
		if (ctr[i].kernel)
			reg.control[i] |= M_PERFCTL_KERNEL;
		if (ctr[i].user)
			reg.control[i] |= M_PERFCTL_USER;
		if (ctr[i].exl)
			reg.control[i] |= M_PERFCTL_EXL;
		reg.counter[i] = 0x80000000 - ctr[i].count;

		pr_debug("oprofile : Cntr %d : %x : %x\n",
				i, reg.control[i], reg.counter[i]);
	}
}

/* Program all of the registers in preparation for enabling profiling.  */
static void mips_34K_cpu_setup (void *args)
{
	unsigned int counters = op_model_34K_mr7_ops.num_counters;
	unsigned int i;

	PRINT_FUNC_NAME();

	for (i = 0; i < counters; i++) {
		write_perfctrl(smp_processor_id(), i, 0);
		write_perfcntr(smp_processor_id(), i, reg.counter[i]);
	}
}

/* Start all counters on current CPU */
static void mips_34K_cpu_start(void *args)
{
	unsigned int counters = op_model_34K_mr7_ops.num_counters;
	unsigned int  i;

	PRINT_FUNC_NAME();

	for (i = 0; i < counters; i++)
		write_perfctrl(smp_processor_id(), i, (WHAT | reg.control[i]));

	perf_irq = mips_34K_perfcount_handler;
}

/* Stop all counters on current CPU */
static void mips_34K_cpu_stop(void *args)
{
	unsigned int counters = op_model_34K_mr7_ops.num_counters;
	unsigned int i;

	PRINT_FUNC_NAME();

	for (i = 0; i < counters; i++)
		write_perfctrl(smp_processor_id(), i, 0);

	perf_irq = null_perf_irq;
}

#ifdef CONFIG_MIPS_MT_SMTC
unsigned long smtc_discarded_samples[NR_CPUS];

/* Try to get the task running on a specified TC */
struct task_struct *smtc_get_current(int tc)
{
	extern u32 kernelsp[NR_CPUS];
	unsigned int sp = kernelsp[tc];
	struct task_struct *task;

	sp |= THREAD_MASK;
	sp ^= THREAD_MASK;

	task = ((struct thread_info *)sp)->task;

	/* Task no longer running..not interesting */
	if (!task_curr(task)) {
		smtc_discarded_samples[tc]++;
		return NULL;
	}

	return task;
}
#endif

#define M_CONFIG1_PC	(1 << 4)

static inline int __n_counters(void)
{
	if (read_c0_config7() & M_CONFIG7_PCT)
		pr_debug("oprofile : 34K MR7 Counter Topology detected\n");
	if (!(read_c0_config1() & M_CONFIG1_PC))
		return 0;
	if (!(r_c0_perfctrl(0, 0) & M_PERFCTL_MORE))
		return 1;
	if (!(r_c0_perfctrl(0, 1) & M_PERFCTL_MORE))
		return 2;
	if (!(r_c0_perfctrl(0, 2) & M_PERFCTL_MORE))
		return 3;

	return 4;
}

static inline int n_counters(void)
{
	int counters = __n_counters();
	pr_debug ("oprofile : Arch has %d physical counters/CPU\n", counters);
	return counters;
}

static inline void reset_counters(int counters)
{
	unsigned int self = smp_processor_id();
	int i;

	PRINT_FUNC_NAME();

	for (i = 0; i < counters; i++) {
		write_perfctrl(self, i, 0);
		write_perfcntr(self, i, 0);
	}
}

static int __init mips_34K_init(void)
{
	int counters, i;

	PRINT_FUNC_NAME();

	phys_cntrs_per_tc = n_counters();
	if (phys_cntrs_per_tc == 0) {
		printk(KERN_ERR "oprofile: CPU has no performance counters\n");
		return -ENODEV;
	}

	counters = counter_layout();
	op_model_34K_mr7_ops.num_counters = counters;
	pr_debug ("oprofile : System has %d oprofile counters\n", counters);

	for_each_online_cpu(i)
		reset_counters(counters);

	op_model_34K_mr7_ops.cpu_type = "mips/34K_mr7";
	perf_irq = mips_34K_perfcount_handler;

	return 0;
}

static void mips_34K_exit(void)
{
	int counters = op_model_34K_mr7_ops.num_counters;
	int i;

	PRINT_FUNC_NAME();

	for_each_online_cpu(i)
		reset_counters(counters);

	perf_irq = null_perf_irq;
}

struct op_mips_model op_model_34K_mr7_ops = {
	.reg_setup	= mips_34K_reg_setup,
	.cpu_setup	= mips_34K_cpu_setup,
	.init		= mips_34K_init,
	.exit		= mips_34K_exit,
	.cpu_start	= mips_34K_cpu_start,
	.cpu_stop	= mips_34K_cpu_stop,
};

#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/perf_event.h>

static void enable_pmccntr(void* data)
{
	pr_info("Enabling PMCCNTR on CPU %d\n", smp_processor_id());

	write_sysreg(BIT(31), pmcntenset_el0); /* Enable PMCCNTR */
	write_sysreg(ARMV8_PMU_USERENR_EN | ARMV8_PMU_USERENR_CR, pmuserenr_el0);
	write_sysreg(ARMV8_PMU_PMCR_E | ARMV8_PMU_PMCR_C, pmcr_el0);
	write_sysreg(ARMV8_PMU_INCLUDE_EL2, pmccfiltr_el0);
}

static void disable_pmccntr(void* data)
{
	pr_info("Disabling PMCCNTR on CPU %d\n", smp_processor_id());

	write_sysreg(0, pmcntenset_el0);
	write_sysreg(0, pmuserenr_el0);
	write_sysreg(0, pmcr_el0);
	write_sysreg(0, pmccfiltr_el0);
}


static int __init init(void)
{
	on_each_cpu(enable_pmccntr, NULL, 1);
	return 0;
}

static void __exit fini(void)
{
	on_each_cpu(disable_pmccntr, NULL, 1);
}

MODULE_DESCRIPTION("Enables user-mode access to PMCCNTR_EL0");
MODULE_LICENSE("GPL");
module_init(init);
module_exit(fini);

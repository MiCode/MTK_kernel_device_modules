#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/cpufreq.h>

#define PROC_NAME "cpumaxfreq"
#define PROC_MODE 0444

static unsigned int cpumax_freq=0;
static struct proc_dir_entry *cpumax_ret;

static int cpumaxfreq_show(struct seq_file *m,void *v)
{
	int freq;

	pr_debug("cpumaxfreq: %u khz\n",cpumax_freq);
	freq = cpumax_freq/1000 + 5;
	/* P16 code for BUGHQ-7600 by p-dongfeiju1 at 2025/06/05 start */
	seq_printf(m, "%u.%01u\n", freq/1000, freq%1000/100);
	/* P16 code for BUGHQ-7600 by p-dongfeiju1 at 2025/06/05 end */

	return 0;
}

static int cpumaxfreq_open(struct inode *inode,struct file *filp)
{
	return single_open(filp,cpumaxfreq_show,NULL);
}

static const struct proc_ops cpumaxfreq_fops = {
	.proc_open		= cpumaxfreq_open,
	.proc_lseek		= seq_lseek,
	.proc_release		= seq_release,
	.proc_read		= seq_read,
};

static int __init proc_cpumaxfreq_init(void)
{
	int cpu;
	struct cpufreq_policy policy;

	cpumax_ret = proc_create(PROC_NAME,PROC_MODE,NULL,&cpumaxfreq_fops);
	if(!cpumax_ret){
		pr_err("could not create /proc/cpumaxfreq\n");
		return 0;
	}

	for_each_possible_cpu(cpu){

		if(cpufreq_get_policy(&policy,cpu))
			continue;
		if(policy.cpuinfo.max_freq > cpumax_freq)
			cpumax_freq = policy.cpuinfo.max_freq;
	}

	return 0;
}

static void __exit proc_cpumaxfreq_exit(void)
{
	proc_remove(cpumax_ret);
}

module_init(proc_cpumaxfreq_init)
module_exit(proc_cpumaxfreq_exit)
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Show MAX frequency supported by cpu");

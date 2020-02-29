/*
 * krp.c
 *
 * The profiler export the profiling information to user space
 *
 * usage:
 *
 * insmod krp.ko func=<func1_name>,<func2_name>
 * cat /sys/kernel/qeexo_krp/stats
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/init.h>

#define MAX_TRACE	5

#ifdef SYSFS_CLASS
struct class *qeexo_class;
#endif
static struct kobject *profile_kobj;

static unsigned int ntrace;

static char *func[MAX_TRACE];
module_param_array(func, charp, NULL, S_IRUGO);

/* per-instance private data */
struct my_data {
	ktime_t entry_stamp;
};

struct my_trace {
	const char *name;
	struct kretprobe krp;
	raw_spinlock_t lock;
	s64	duration;
	u64	avg_duration;
	u64	invocation;
} tracer[MAX_TRACE];

static void tracer_counter(struct my_trace *tc);

/*
 * The "stats" file where a static variable is read from and written to.
 */
static ssize_t stats_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	int i;
	ssize_t len = 0;
	struct my_trace *tc = &tracer[0];
	char *head =
		"<symbol name>:<average time per call in ns>:<invocations>";

	len += sprintf(buf + len, "%s\n", head);
	/*
	len += sprintf(buf + len, "%s:%lld:%lld\n", tracer[0].name,
			tracer[0].avg_duration, tracer[0].invocation);
	len += sprintf(buf + len, "%s:%lld:%lld\n", tracer[1].name,
			tracer[1].avg_duration, tracer[1].invocation);
	*/
	for (i = 0; i < ntrace; i++, tc++) {
		/*
		tracer_counter(&tracer[i]);
		len += sprintf(buf + len, "%s:%lld:%lld\n", tracer[i].name,
				tracer[i].avg_duration, tracer[i].invocation);
		*/
		tracer_counter(tc);
		len += sprintf(buf + len, "%s:%lld:%lld\n", tc->name,
				tc->avg_duration, tc->invocation);
	}

	return len;
}

/* Sysfs attributes cannot be world-writable. */
static struct kobj_attribute stats_attribute =
	__ATTR(stats, 0444, stats_show, NULL);
/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *attrs[] = {
	&stats_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory.  If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
static struct attribute_group attr_group = {
	.attrs = attrs,
};


/* Here we use the entry_hanlder to timestamp function entry */
static int entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct my_data *data;

	if (!current->mm)
		return 1;	/* Skip kernel threads */

	data = (struct my_data *)ri->data;
	data->entry_stamp = ktime_get();
	return 0;
}

/*
 * Return-probe handler: Log the return value and duration. Duration may turn
 * out to be zero consistently, depending upon the granularity of time
 * accounting on the platform.
 */
static int ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	int retval = regs_return_value(regs);
	struct kretprobe *krp = ri->rp;
	struct my_trace *tc = container_of(krp, struct my_trace, krp);
	struct my_data *data = (struct my_data *)ri->data;
	s64 delta;
	ktime_t now;

	now = ktime_get();
	delta = ktime_to_ns(ktime_sub(now, data->entry_stamp));
	printk(KERN_DEBUG "%s returned %d and took %lld ns to execute\n",
			krp->kp.symbol_name, retval, (long long)delta);
	raw_spin_lock(&tc->lock);
	tc->duration += delta;
	tc->invocation++;
	raw_spin_unlock(&tc->lock);

	return 0;
}

static void tracer_counter(struct my_trace *tc)
{
	raw_spin_lock(&tc->lock);
	if (tc->invocation == 0)
		tc->avg_duration = 0;
	else
		tc->avg_duration = tc->duration / tc->invocation;
	raw_spin_unlock(&tc->lock);
	printk(KERN_DEBUG "tracer: %s, avg_duration=%lld ns, invocation=%lld\n",
			tc->name, tc->avg_duration, tc->invocation);
}

static unsigned int get_func_num(void)
{
	int i = 0;
	while (func[i] && strlen(func[i]) > 0 && i < MAX_TRACE)
		i++;
	return i;
}

static int __init kretprobe_init(void)
{
	int ret;
	int i;

	printk(KERN_DEBUG "\n KRP INIT\n");
	ntrace = get_func_num();
	if (ntrace == 0) {
		printk(KERN_ERR "No function be traced\n");
		return -EINVAL;
	}
#ifdef SYSFS_CLASS
	struct kobject *kparent;
	qeexo_class = class_create(THIS_MODULE, "qeexo");
	if (IS_ERR(qeexo_class)) {
		ret = PTR_ERR(qeexo_class);
		printk(KERN_ERR "cannot create /sys/class/qeexo\n");
		goto out;
	}
	kparent = qeexo_class->dev_kobj;
	profile_kobj = kobject_create_and_add("qeexo_krp", kparent);
#else
	profile_kobj = kobject_create_and_add("qeexo_krp", kernel_kobj);
#endif
	if (IS_ERR(profile_kobj)) {
		ret = PTR_ERR(profile_kobj);
		printk(KERN_ERR "cannot create sysfs kobject/\n");
		goto out_class;
	}
	/* Create the files associated with this kobject */
	ret = sysfs_create_group(profile_kobj, &attr_group);
	if (ret)
		goto out_kobj;

	for (i = 0; i < ntrace; i++) {
		tracer[i].name = func[i];
		raw_spin_lock_init(&tracer[i].lock);
		tracer[i].krp.handler = ret_handler,
		tracer[i].krp.entry_handler = entry_handler,
		tracer[i].krp.data_size	= sizeof(struct my_data),
		/* Probe up to 20 instances concurrently. */
		tracer[i].krp.maxactive	= 20,
		tracer[i].krp.kp.symbol_name = func[i];
		tracer[i].duration = 0;
		tracer[i].avg_duration = 0;
		tracer[i].invocation = 0;
		ret = register_kretprobe(&tracer[i].krp);
		if (ret < 0) {
			printk(KERN_INFO "regist tracer[%d].krp failed,\
					returned %d\n", i, ret);
			while (--i >= 0) 
				unregister_kretprobe(&tracer[i].krp);
			goto out_kobj;
		}
	}

	return 0;

out_kobj:
	kobject_put(profile_kobj);
out_class:
#ifdef SYSFS_CLASS
	class_destroy(qeexo_class);
out:
#endif
	return ret;
}

static void __exit kretprobe_exit(void)
{
	int i;
	for (i = 0; i < ntrace; i++) {
		unregister_kretprobe(&tracer[i].krp);
		printk(KERN_INFO "tracer[%d] %s at %p unregistered\n",
				i, tracer[i].name, tracer[i].krp.kp.addr);
		/* nmissed > 0 suggests that maxactive was set too low. */
		printk(KERN_INFO "Missed probing %d instances of %s\n",
			tracer[i].krp.nmissed, tracer[i].name);
		tracer_counter(&tracer[i]);
	}
	kobject_put(profile_kobj);
#ifdef SYSFS_CLASS
	class_destroy(qeexo_class);
#endif
	printk(KERN_DEBUG "KRP EXIT\n\n");
}

module_init(kretprobe_init)
module_exit(kretprobe_exit)
MODULE_LICENSE("GPL");
MODULE_AUTHOR("He Ye <hbomb_ustc@hotmail.com>");

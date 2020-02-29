/*
 * kmp module
 *
 * It used to export information about memory leaks related with
 * memory (de)allocated by vmalloc() and vfree() 
 *
 * usage: 
 * 	$insmod kmp.ko
 * 	$echo <saddr> > /sys/kernel/qeexo_kmp/scan_start_addr
 * 	$echo <eaddr> > /sys/kernel/qeexo_kmp/scan_end_addr
 * 	$cat /sys/kernel/qeexo_kmp/memleaks
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
#include <linux/slab.h>
#include <linux/list.h>

static u32 saddr = 0x08004000;
static u32 eaddr = 0x08008000;

/* per-instance private data */
struct my_data {
	ktime_t entry_stamp;
};

#define NOT_FREE	0
#define	POSSIBLE_LEAK	1

struct _MEM_INFO {
	const void 		*addr;
	size_t			size;
	int			flag;
	struct list_head	list;
};
typedef struct _MEM_INFO MEM_INFO;

LIST_HEAD(vmlist);

static ssize_t saddr_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	sscanf(buf, "%x\n", &saddr);
	return count;
}

static struct kobj_attribute saddr_attribute =
	__ATTR(scan_start_addr, 0200, NULL, saddr_store);

static ssize_t eaddr_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	sscanf(buf, "%x\n", &eaddr);
	return count;
}

static struct kobj_attribute eaddr_attribute =
	__ATTR(scan_end_addr, 0200, NULL, eaddr_store);


static void scan_vm(u32 start, u32 end)
{
	MEM_INFO *minfo;
	void *p;
	void *s_addr = (void *)start;
	void *e_addr = (void *)end;

	if (start > end)
		return;
	if (start == 0 || end == 0)
		return;

	printk(KERN_INFO "scan vm from %p to %p\n", s_addr, e_addr);
	list_for_each_entry(minfo, &vmlist, list) {
		for (p = s_addr; p <= e_addr; p++) {
			if (p == minfo->addr) {
				minfo->flag = POSSIBLE_LEAK;
				break;
			}
		}
	}
	printk(KERN_INFO "scan vm finished\n");
}

#define M_STATUS(flag) (flag ? "possible leak" : "not free yet") 

static ssize_t memleaks_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	MEM_INFO *minfo;
	ssize_t len = 0;
	char *head =
		"<block star address>:" \
		"<\"possible leak\" or \"not freed yet\">:" \
		"<instruction pointer where it was allocated>";

	scan_vm(saddr, eaddr);
	len += sprintf(buf + len, "%s\n", head);
	list_for_each_entry(minfo, &vmlist, list) {
		len += sprintf(buf + len, "0x%p:%s\n",
				minfo->addr,
				M_STATUS(minfo->flag));
	}

	return len;
}

static struct kobj_attribute memleaks_attribute =
	__ATTR(memleaks, 0444, memleaks_show, NULL);

static struct attribute *attrs[] = {
	&saddr_attribute.attr,
	&eaddr_attribute.attr,
	&memleaks_attribute.attr,
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


static int vmalloc_entry(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return 0;
}

static void vmlist_add(void *addr, unsigned long size)
{
	MEM_INFO *minfo;

	minfo = (MEM_INFO *)kmalloc(sizeof(MEM_INFO), GFP_KERNEL);	
	if (minfo == NULL)
		printk(KERN_INFO "kmalloc minfo failed\n");

	minfo->addr = addr;
	minfo->size = size;
	minfo->flag = NOT_FREE;

	list_add(&minfo->list, &vmlist);
}

static void vmlist_remove(void *addr) 
{
	MEM_INFO *minfo = NULL;

	list_for_each_entry(minfo, &vmlist, list) {
		if (minfo->addr == addr) {
			list_del(&minfo->list);
			kfree(minfo);
			break;
		}
	}
}

static int vmalloc_return(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	void *addr = (void *)regs_return_value(regs);
	unsigned long *sp = (unsigned long *)kernel_stack_pointer(regs);
	
	unsigned int size = *(sp + 8); /* arg1 of vmalloc */
	//size = PAGE_ALIGN(size);

	if (addr)
		vmlist_add(addr, size);

	return 0;
}

static struct kretprobe vmalloc_krp = {
	.handler		= vmalloc_return,
	.entry_handler		= vmalloc_entry,
	.data_size		= sizeof(struct my_data),
	/* Probe up to 20 instances concurrently. */
	.maxactive		= 20,
	.kp.symbol_name		= "vmalloc"
};

static void jvfree(void *addr)
{
	vmlist_remove(addr);
	jprobe_return();
}

static struct jprobe vfree_jp = {
	.entry			= jvfree,
	.kp.symbol_name		= "vfree"
};

static struct kobject *kmp_kobj;

static int __init kmp_init(void)
{
	int ret;

	kmp_kobj = kobject_create_and_add("qeexo_kmp", kernel_kobj);
	if (!kmp_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(kmp_kobj, &attr_group);
	if (ret) {
		printk(KERN_INFO "create KMP kobject failed\n");
		goto sysfs_failed;
	}

	ret = register_kretprobe(&vmalloc_krp);
	if (ret < 0) {
		printk(KERN_INFO "register vmalloc_krp failed\n");
		goto krp_failed;
	}

	ret = register_jprobe(&vfree_jp);
	if (ret < 0) {
		printk(KERN_INFO "register vfree_jp failed\n");
		goto jp_failed;
	}

	printk(KERN_INFO "KMP registered\n");
	return 0;

jp_failed:
	unregister_kretprobe(&vmalloc_krp);

krp_failed:
sysfs_failed:
	kobject_put(kmp_kobj);

	return -1;
}

static void __exit kmp_exit(void)
{
	unregister_jprobe(&vfree_jp);
	unregister_kretprobe(&vmalloc_krp);
	kobject_put(kmp_kobj);
	printk(KERN_INFO "KMP unregistered\n");
}

module_init(kmp_init)
module_exit(kmp_exit)
MODULE_LICENSE("GPL");
MODULE_AUTHOR("He Ye <hbomb_ustc@hotmail.com>");

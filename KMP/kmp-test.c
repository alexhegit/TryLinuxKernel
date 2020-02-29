/*
 * mm/kmemleak-test.c
 *
 * Copyright (C) 2008 ARM Limited
 * Written by Catalin Marinas <catalin.marinas@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/percpu.h>
#include <linux/fdtable.h>

#include <linux/kmemleak.h>

static void *addr[4];

/*
 * Some very simple testing. This function needs to be extended for
 * proper testing.
 */
static int __init kmp_test_init(void)
{
	printk(KERN_INFO "Kmemleak testing\n");

	addr[0] = vmalloc(64);
	addr[1] = vmalloc(128);
	addr[2] = vmalloc(256);
	addr[3] = vmalloc(512);

	/* make some orphan objects */
	pr_info("kmemleak: vmalloc(64) = %p\n", addr[0]);
	pr_info("kmemleak: vmalloc(128) = %p\n", addr[1]);
	pr_info("kmemleak: vmalloc(256) = %p\n", addr[2]);
	pr_info("kmemleak: vmalloc(512) = %p\n", addr[3]);

	return 0;
}
module_init(kmp_test_init);

static void __exit kmp_test_exit(void)
{
	int i;
	for (i = 0; i < 4; i++) {
		vfree(addr[i]);
	}
}
module_exit(kmp_test_exit);

MODULE_LICENSE("GPL");

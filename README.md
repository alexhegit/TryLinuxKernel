# TryLinuxKernel

## Two tasks

Your task is to develop a basic LKM (Linux Kernel Module) profiler. This kernel module should have the following two main 
components: 
1. Kernel routine profiler (KRP): Provides a way to dynamically measure the average duration and number of invocations of
user-specified kernel functions.  
2. Kernel memory profiler (KMP): Enables the detection of possible memory leaks in the kernel space  


### KRP REQUIREMENTS
The kernel module should have a parameter called ‘funcs’ that specifies the kernel symbols to profile separated with commas.
For instance, if the user wants to profile the getpid() and clone() system calls, she will install the module using the
following command: 
```
    insmod profiler.ko funcs=sys_getpid,sys_clone 
``` 
The module should export the profiling information to user space applications via sysfs. This means that once the kernel module
is launched it will create a read-only sysfs file named */sys/class/qeexo/profiler/stats* so that the user can read the current 
profiling status. 
 
The file format should be: 
```
    <symbol name>:<average time per call in ns>:<invocations>*
``` 
Output example: 
```
    $ cat /sys/class/qeexo/profiler/stats
    sys_getpid:1421:13
    sys_clone:30475:795 
```

### KMP REQUIREMENTS
The KMP should export information about memory leaks to user space applications via sysfs. The KMP should only trace memory 
(de)allocations performed via vmalloc() and vfree(). An allocated block will be considered a leak if no pointer to its start
address can be found by scanning the memory. 

The memory region to scan should be specified by the following two write-only sysfs files: 
  1. /sys/class/qeexo/profiler/scan_start_addr: Start virtual address 
  2. /sys/class/qeexo/profiler/scan_end_addr: End virtual address 
  
You should assume that those two addresses are valid for reading. 

The module should export the information about memory leaks to user space applications via sysfs. Once the module is launched
it will create a read-only sysfs file named /sys/class/qeexo/profiler/memleaks so that the user can see the current memory leaks. 
The file format should be: 
```
<block start address>:<“possible leak” or “not freed yet”>:<instruction pointer where it was allocated> 
```

• “possible leak” means that the allocated block may be leaked.  
• “not freed yet” means that there was found a pointer to that block (so it is not a leak) but it was not freed yet.  

The memory will only be scanned when /sys/class/qeexo/profiler/memleaks is read. 

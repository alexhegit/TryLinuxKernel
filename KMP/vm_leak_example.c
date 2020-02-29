// Linux Kernel headers
#include    <linux/module.h>
#include    <linux/kernel.h>
#include    <linux/init.h>
#include    <linux/config.h>
#include    <linux/vmalloc.h>

#define  FILE_NAME_LENGTH          256
#define  vmalloc(size)              profile_malloc (block_size, __FILE__, __LINE__)
#define  vfree(mem_ref)                 profile_free(mem_ref)

struct _MEM_INFO
{
    const void          *address;
    size_t              size;
    char                file_name[FILE_NAME_LENGTH];
    size_t              length;
};
typedef struct _MEM_INFO MEM_INFO;

struct _MEM_LEAK {
    MEM_INFO mem_info;
    struct _MEM_LEAK * next;
};
typedef struct _MEM_LEAK MEM_LEAK;



#undef      vmalloc
#undef      vfree


static MEM_PROFILER_LIST * ptr_start = NULL;
static MEM_PROFILER_LIST * ptr_next =  NULL;

/*
 * adds allocated memory info. into the list
 *
 */
void add(MEM_INFO alloc_info)
{

    MEM_PROFILER_LIST * mem_leak_info = NULL;
    mem_leak_info = (MEM_PROFILER_LIST *) malloc (sizeof(MEM_PROFILER_LIST));
    mem_leak_info->mem_info.address = alloc_info.address;
    mem_leak_info->mem_info.size = alloc_info.size;
    strcpy(mem_leak_info->mem_info.file_name, alloc_info.file_name); 
    mem_leak_info->mem_info.line = alloc_info.line;
    mem_leak_info->next = NULL;

    if (ptr_start == NULL)  
    {
        ptr_start = mem_leak_info;
        ptr_next = ptr_start;
    }
    else {
        ptr_next->next = mem_leak_info;
        ptr_next = ptr_next->next;              
    }

}

/*
 * remove memory info. from the list
 *
 */
void erase(unsigned pos)
{

    unsigned int i = 0;
    MEM_PROFILER_LIST * alloc_info, * temp;

    if(pos == 0)
    {
        MEM_PROFILER_LIST * temp = ptr_start;
        ptr_start = ptr_start->next;
        free(temp);
    }
    else 
    {
        for(i = 0, alloc_info = ptr_start; i < pos; 
            alloc_info = alloc_info->next, ++i)
        {
            if(pos == i + 1)
            {
                temp = alloc_info->next;
                alloc_info->next =  temp->next;
                free(temp);
                break;
            }
        }
    }
}

/*
 * deletes all the elements from the list
 */
void clear()
{
    MEM_PROFILER_LIST * temp = ptr_start;
    MEM_PROFILER_LIST * alloc_info = ptr_start;

    while(alloc_info != NULL) 
    {
        alloc_info = alloc_info->next;
        free(temp);
        temp = alloc_info;
    }
}

/*
 * profile of vmalloc
 */
void * profile_vmalloc (unsigned int size, const char * file, unsigned int line)
{
    void * ptr = vmalloc (size);
    if (ptr != NULL) 
    {
        add_mem_info(ptr, size, file, line);
    }
    return ptr;
}


/*
 * profile of free
 */
void profile_free(void * mem_ref)
{
    remove_mem_info(mem_ref);
    free(mem_ref);
}

/*
 * gets the allocated memory info and adds it to a list
 *
 */
void add_mem_info (void * mem_ref, unsigned int size,  const char * file, unsigned int line)
{
    MEM_INFO mem_alloc_info;

    /* fill up the structure with all info */
    memset( &mem_alloc_info, 0, sizeof ( mem_alloc_info ) );
    mem_alloc_info.address  = mem_ref;
    mem_alloc_info.size = size;
    strncpy(mem_alloc_info.file_name, file, FILE_NAME_LENGTH);
    mem_alloc_info.line = line;

    /* add the above info to a list */
    add(mem_alloc_info);
}

/*
 * if the allocated memory info is part of the list, removes it
 *
 */
void remove_mem_info (void * mem_ref)
{
    unsigned int i;
    MEM_PROFILER_LIST  * leak_info = ptr_start;

    /* check if allocate memory is in our list */
    for(i = 0; leak_info != NULL; ++i, leak_info = leak_info->next)
    {
        if ( leak_info->mem_info.address == mem_ref )
        {
            erase ( i );
            break;
        }
    }
}

/*
 * writes a memory leak summary to a file
 */
void mem_leak_summary(void)
{
    unsigned int i;
    MEM_PROFILER_LIST * mem_output;

    FILE * fp_write = fopen (SUMMARY_FILE, "wt");
    char info[1024];

    if(fp_write != NULL)
    {

        fwrite(info, (strlen(info) + 1) , 1, fp_write);
        sprintf(info, "%s\n", "-----------------------------------");   
        fwrite(info, (strlen(info) + 1) , 1, fp_write);

        for(mem_output= ptr_start; mem_output!= NULL; mem_output= mem_output->next)
        {
            sprintf(info, "address : %d\n", leak_info->mem_output.address);
            fwrite(info, (strlen(info) + 1) , 1, fp_write);
            sprintf(info, "size    : %d bytes\n", leak_info->mem_output.size);          
            fwrite(info, (strlen(info) + 1) , 1, fp_write);
            sprintf(info, "line    : %d\n", leak_info->mem_output.line);
            fwrite(info, (strlen(info) + 1) , 1, fp_write);
            sprintf(info, "%s\n", "-----------------------------------");   
            fwrite(info, (strlen(info) + 1) , 1, fp_write);
        }
    }   
    clear();
}

static int __init profiler_init(void)
{
    return 0;
}

static void __exit profiler_cleanup(void)
{
    printk("profiler module uninstalled\n");
}

module_init(profiler_init);
module_exit(profiler_cleanup);
MODULE_LICENSE("GPL");

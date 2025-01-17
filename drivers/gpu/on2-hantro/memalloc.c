/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Google Finland Oy.                              --
--                                                                            --
--                   (C) COPYRIGHT 2012 GOOGLE FINLAND OY                     --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : Allocate memory blocks
--
------------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/module.h>
/* needed for __init,__exit directives */
#include <linux/init.h>
/* needed for remap_page_range */
#include <linux/mm.h>
/* obviously, for kmalloc */
#include <linux/slab.h>
/* for struct file_operations, register_chrdev() */
#include <linux/fs.h>
/* standard error codes */
#include <linux/errno.h>
/* this header files wraps some common module-space operations ...
   here we use mem_map_reserve() macro */

#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/list.h>
/* for current pid */
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
/* Our header */
#include "memalloc.h"

/* module description */
//MODULE_LICENSE("Proprietary");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Google Finland Oy");
MODULE_DESCRIPTION("RAM allocation");
/*
#ifndef HLINA_START_ADDRESS
#define HLINA_START_ADDRESS 0x02000000
#endif
*/
#define MAX_OPEN 32
#define ID_UNUSED 0xFF
/* the size of chunk in MEMALLOC_DYNAMIC */
#define CHUNK_SIZE 1
#define MEMALLOC_BASIC 0
#define MEMALLOC_MAX_OUTPUT 1
#define MEMALLOC_BASIC_X2 2
#define MEMALLOC_BASIC_AND_16K_STILL_OUTPUT 3
#define MEMALLOC_BASIC_AND_MVC_DBP 4
#define MEMALLOC_BASIC_AND_4K_OUTPUT 5
#define MEMALLOC_DYNAMIC 6

struct mm_private{
	struct platform_device	*pdev;
	struct cdev cdev;
	dev_t		dev;
	u32			start;
	u32			size;
	unsigned int alloc_method;
	int memalloc_major;
	struct class *class;
};

static struct mm_private mm_priv;
static const char memalloc_dev_name[] = "memalloc";

/* selects the memory allocation method, i.e. which allocation scheme table is used */
//unsigned int alloc_method = MEMALLOC_BASIC;
/* memory size in MBs for MEMALLOC_DYNAMIC */
//unsigned int alloc_size = 96;

//static int memalloc_major = 0;  /* dynamic */

int id[MAX_OPEN] = { ID_UNUSED };

/* module_param(name, type, perm) */
//module_param(alloc_method, uint, 0);
//module_param(alloc_size, uint, 0);

/* here's all the must remember stuff */
struct allocation
{
    struct list_head list;
    void *buffer;
    unsigned int order;
    int fid;
};

struct list_head heap_list;

static DEFINE_SPINLOCK(mem_lock);

typedef struct hlinc
{
    unsigned int bus_address;
    unsigned int used;
    unsigned int size;
    /* how many chunks are reserved so we can free right amount chunks */
    unsigned int chunks_reserved;
    int file_id;
} hlina_chunk;

static unsigned int *size_table = NULL;
static size_t chunks = 0;

unsigned int size_table_0[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    4, 4, 4, 4, 4, 4, 4, 4,
    10, 10, 10, 10,
    22, 22, 22, 22,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    50, 50, 50, 50, 50, 50, 50,
    75, 75, 75, 75, 75,
    86, 86, 86, 86, 86,
    113, 113,
    152, 152,
    162, 162, 162,
    270, 270, 270,
    403, 403, 403, 403,
    403, 403,
    450, 450,
    893, 893,
    893, 893,
    1999,
    3997,
    4096,
    8192
};

unsigned int size_table_1[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0,
    0, 0,
    0, 0, 0,
    0, 0, 0,
    0, 0,
    0, 64,
    64, 128,
    512,
    3072,
    8448
};

unsigned int size_table_2[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    10, 10, 10, 10, 10, 10, 10, 10,
    22, 22, 22, 22, 22, 22, 22, 22,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
    75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
    86, 86, 86, 86, 86, 86, 86, 86, 86, 86,
    113, 113, 113, 113,
    152, 152, 152, 152,
    162, 162, 162, 162, 162, 162,
    270, 270, 270, 270, 270, 270,
    403, 403, 403, 403, 403, 403, 403, 403,
    403, 403, 403, 403,
    450, 450, 450, 450,
    893, 893, 893, 893,
    893, 893, 893, 893,
    1999, 1999,
    3997, 3997,
    4096, 4096,
    8192, 8192
};

unsigned int size_table_3[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    4, 4, 4, 4, 4, 4, 4, 4,
    10, 10, 10, 10,
    22, 22, 22, 22,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    50, 50, 50, 50, 50, 50, 50,
    75, 75, 75, 75, 75,
    86, 86, 86, 86, 86,
    113, 113,
    152, 152,
    162, 162, 162,
    270, 270, 270,
    403, 403, 403, 403,
    403, 403,
    450, 450,
    893, 893,
    893, 893,
    1999,
    3997,
    4096,
    8192,
    19000
};

unsigned int size_table_4[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    4, 4, 4, 4, 4, 4, 4, 4,
    10, 10, 10, 10,
    22, 22, 22, 22,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    50, 50, 50, 50, 50, 50, 50,
    75, 75, 75, 75, 75,
    86, 86, 86, 86, 86,
    113, 113,
    152, 152,
    162, 162, 162,
    270, 270, 270,
    403, 403, 403, 403,
    403, 403,
    450, 450,
    893, 893,
    893, 893,
    1999,
    3997,
    3997,3997,3997,3997,3997,
    4096,
    8192,
};

unsigned int size_table_5[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    4, 4, 4, 4, 4, 4, 4, 4,
    10, 10, 10, 10,
    22, 22, 22, 22,
    38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
    50, 50, 50, 50, 50, 50, 50,
    75, 75, 75, 75, 75,
    86, 86, 86, 86, 86,
    113, 113,
    152, 152,
    162, 162, 162,
    270, 270, 270,
    403, 403, 403, 403,403, 403,
    450, 450,
    893, 893,893, 893,
    1999,
    3997,
    4096,
    7200, 7200, 7200, 7200,
    14000,
    17400, 17400
};

/*static hlina_chunk hlina_chunks[256];*/
/* maximum for 383 MB with 1*4096 sized chunks*/
static hlina_chunk hlina_chunks[98048]; 

static int AllocMemory(unsigned *busaddr, unsigned int size, struct file *filp);
static int FreeMemory(unsigned long busaddr);
static void ResetMems(void);

static long memalloc_ioctl(struct file *filp, unsigned int cmd,
	    unsigned long arg)
{
    int err = 0;
    int ret;

    PDEBUG("ioctl cmd 0x%08x\n", cmd);

    if (filp == NULL || arg == 0)
	return -EFAULT;

    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if(_IOC_TYPE(cmd) != MEMALLOC_IOC_MAGIC)
        return -ENOTTY;
    if(_IOC_NR(cmd) > MEMALLOC_IOC_MAXNR)
        return -ENOTTY;

    if(_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void *) arg, _IOC_SIZE(cmd));
    else if(_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void *) arg, _IOC_SIZE(cmd));
    if(err)
        return -EFAULT;

    switch (cmd)
    {
    case MEMALLOC_IOCHARDRESET:

        PDEBUG("HARDRESET\n");
        ResetMems();

        break;

    case MEMALLOC_IOCXGETBUFFER:
        {
            int result;
            MemallocParams memparams;

            PDEBUG("GETBUFFER\n");
            spin_lock(&mem_lock);

	    if (__copy_from_user(&memparams, (const void *) arg,
			sizeof(memparams)))
		return -EFAULT;

            result = AllocMemory(&memparams.busAddress, memparams.size, filp);

	    if (__copy_to_user((void *) arg, &memparams,
			sizeof(memparams)))
		return -EFAULT;

            spin_unlock(&mem_lock);

            return result;
        }
    case MEMALLOC_IOCSFREEBUFFER:
        {

            unsigned long busaddr;

            PDEBUG("FREEBUFFER\n");
            spin_lock(&mem_lock);
            __get_user(busaddr, (unsigned long *) arg);
            ret = FreeMemory(busaddr);

            spin_unlock(&mem_lock);
            return ret;
        }
    }
    return 0;
}

static int memalloc_open(struct inode *inode, struct file *filp)
{
    int i = 0;

    for(i = 0; i < MAX_OPEN + 1; i++)
    {

        if(i == MAX_OPEN)
            return -1;
        if(id[i] == ID_UNUSED)
        {
            id[i] = i;
            filp->private_data = id + i;
            break;
        }
    }
    PDEBUG("dev opened\n");
    return 0;

}

static int memalloc_release(struct inode *inode, struct file *filp)
{

    int i = 0;

    for(i = 0; i < chunks; i++)
    {
        if(hlina_chunks[i].file_id == *((int *) (filp->private_data)))
        {
            hlina_chunks[i].used = 0;
            hlina_chunks[i].file_id = ID_UNUSED;
            hlina_chunks[i].chunks_reserved = 0;
        }
    }
    *((int *) filp->private_data) = ID_UNUSED;
    PDEBUG("dev closed\n");
    return 0;
}

/* VFS methods */
static const struct file_operations memalloc_fops = {
    .open		= memalloc_open,
    .release	= memalloc_release,
    .unlocked_ioctl	= memalloc_ioctl,
};


int memalloc_sysfs_register(struct mm_private *mml_priv, dev_t dev,
	    const char *memalloc_dev_name)
{
    int err = 0;
    struct device *mdev;

    mml_priv->class = class_create(THIS_MODULE, memalloc_dev_name);
    if (IS_ERR(mml_priv->class)) {
		err = PTR_ERR(mml_priv->class);
		goto init_class_err;
    }
    mdev = device_create(mml_priv->class, NULL, dev, NULL,
	    memalloc_dev_name);
    if (IS_ERR(mdev))	{
		err = PTR_ERR(mdev);
		goto init_mdev_err;
    }

    /* Success! */
    return 0;

    /* Error handling */
init_mdev_err:
    class_destroy(mml_priv->class);
init_class_err:

    return err;
}



int memalloc_init(void)
{
    int result = -EFAULT;
    int i = 0;

    PDEBUG("module init\n");
    printk("memalloc: 8190 Linear Memory Allocator, %s \n", "$Revision: 1.14 $");
    printk("memalloc: linear memory base = 0x%08x \n", mm_priv.start);

    switch (mm_priv.alloc_method)
    {

    case MEMALLOC_MAX_OUTPUT:
        size_table = size_table_1;
        chunks = (sizeof(size_table_1) / sizeof(*size_table_1));
        printk(KERN_INFO "memalloc: allocation method: MEMALLOC_MAX_OUTPUT\n");
        break;
    case MEMALLOC_BASIC_X2:
        size_table = size_table_2;
        chunks = (sizeof(size_table_2) / sizeof(*size_table_2));
        printk(KERN_INFO "memalloc: allocation method: MEMALLOC_BASIC x 2\n");
        break;
    case MEMALLOC_BASIC_AND_16K_STILL_OUTPUT:
        size_table = size_table_3;
        chunks = (sizeof(size_table_3) / sizeof(*size_table_3));
        printk(KERN_INFO "memalloc: allocation method: MEMALLOC_BASIC_AND_16K_STILL_OUTPUT\n");
        break;
    case MEMALLOC_BASIC_AND_MVC_DBP:
        size_table = size_table_4;
        chunks = (sizeof(size_table_4) / sizeof(*size_table_4));
        printk(KERN_INFO "memalloc: allocation method: MEMALLOC_BASIC_AND_MVC_DBP\n");
        break;
    case MEMALLOC_BASIC_AND_4K_OUTPUT:
        size_table = size_table_5;
        chunks = (sizeof(size_table_5) / sizeof(*size_table_5));
        printk(KERN_INFO "memalloc: allocation method: MEMALLOC_BASIC_AND_4K_OUTPUT\n");
        break;
    case MEMALLOC_DYNAMIC:
//        chunks = (mm_priv.size*1024*1024)/(4096*CHUNK_SIZE);
        chunks = (mm_priv.size)/(4096*CHUNK_SIZE);
        printk(KERN_INFO "memalloc: allocation method: MEMALLOC_DYNAMIC; size %d MB; chunks %d of size %d\n", mm_priv.size/1024/1024, chunks, CHUNK_SIZE);
        size_table = (unsigned int *) kmalloc(chunks*sizeof(unsigned int), GFP_USER);
        if (size_table == NULL)
        {
            printk(KERN_ERR "memalloc: cannot allocate size_table for MEMALLOC_DYNAMIC\n");
            goto err;
        }
        for (i = 0; i < chunks; ++i)
        {
            size_table[i] = CHUNK_SIZE;
        }
        break;
    default:
        size_table = size_table_0;
        chunks = (sizeof(size_table_0) / sizeof(*size_table_0));
        printk(KERN_INFO "memalloc: allocation method: MEMALLOC_BASIC\n");
        break;
    }

	mm_priv.dev = 0;
    if (0 == mm_priv.memalloc_major) {
	/* auto select a major */
		result = alloc_chrdev_region(&mm_priv.dev, 0, 1, memalloc_dev_name);
		mm_priv.memalloc_major = MAJOR(mm_priv.dev);
    } else {
	/* use load time defined major number */
		mm_priv.dev = MKDEV(mm_priv.memalloc_major, 0);
		result = register_chrdev_region(mm_priv.dev, 1, memalloc_dev_name);
    }

    if (result)
		goto init_chrdev_err;

    memset(&mm_priv.cdev, 0, sizeof(mm_priv.cdev));

    /* initialize our char dev data */
    cdev_init(&mm_priv.cdev, &memalloc_fops);
    mm_priv.cdev.owner = THIS_MODULE;
    mm_priv.cdev.ops = &memalloc_fops;

    /* register char dev with the kernel */
    result = cdev_add(&mm_priv.cdev, mm_priv.dev, 1/*count*/);
    if (result)
	goto init_cdev_err;

    result = memalloc_sysfs_register(&mm_priv, mm_priv.dev, memalloc_dev_name);
    if (result)
	goto init_sysfs_err;

    ResetMems();

    /* We keep a register of out customers, reset it */
    for(i = 0; i < MAX_OPEN; i++)
    {
        id[i] = ID_UNUSED;
    }

    return 0;

init_sysfs_err:
    cdev_del(&mm_priv.cdev);
init_cdev_err:
    unregister_chrdev_region(mm_priv.dev, 1/*count*/);
init_chrdev_err:
  err:
    PDEBUG("memalloc: module not inserted\n");
    return result;
}

void memalloc_cleanup(void)
{

    PDEBUG("clenup called\n");

    if (mm_priv.alloc_method == MEMALLOC_DYNAMIC && size_table != NULL)
        kfree(size_table);

	device_destroy(mm_priv.class, mm_priv.dev);
    class_destroy(mm_priv.class);
    cdev_del(&mm_priv.cdev);
    unregister_chrdev_region(mm_priv.dev, 1/*count*/);

    PDEBUG("memalloc: module removed\n");
    return;
}
/*
module_init(memalloc_init);
module_exit(memalloc_cleanup);
*/
/* Cycle through the buffers we have, give the first free one */
static int AllocMemory(unsigned *busaddr, unsigned int size, struct file *filp)
{

    int i = 0;
    int j = 0;
    unsigned int skip_chunks = 0;
    unsigned int size_reserved = 0;
    *busaddr = 0;

    if (mm_priv.alloc_method == MEMALLOC_DYNAMIC)
    {
        /* calculate how many chunks we need */
        unsigned int alloc_chunks = size/(CHUNK_SIZE*4096) + 1;
        
        /* run through the chunk table */
        for(i = 0; i < chunks; i++)
        {
            skip_chunks = 0;
            /* if this chunk is available */
            if(!hlina_chunks[i].used)
            {
                /* check that there is enough memory left */
                if (i + alloc_chunks > chunks)
                    break;
                
                /* check that there is enough consecutive chunks available */
#if 0                
                if (hlina_chunks[i + alloc_chunks - 1].used)
                {
                    skip_chunks = 1;
                    /* continue from the next chunk after used */
                    i = i + alloc_chunks;
                    break;
                }
#endif
#if 1            
                for (j = i; j < i + alloc_chunks; j++)
                {
                    if (hlina_chunks[j].used)
                    {
                        skip_chunks = 1;
                        /* skip the used chunks */
                        i = j + hlina_chunks[j].chunks_reserved - 1;
		                break;
                    }
                }
#endif
                
                /* if enough free memory found */
                if (!skip_chunks)
                {
#if 0
                    /* mark the chunks used and return bus address to first chunk */
                    for (j = i; j < i + alloc_chunks; j++)
                    {
                        hlina_chunks[j].used = 1;
                        hlina_chunks[j].file_id = *((int *) (filp->private_data));
                    }
#endif
                    *busaddr = hlina_chunks[i].bus_address;
                    hlina_chunks[i].used = 1;
                    hlina_chunks[i].file_id = *((int *) (filp->private_data));
                    hlina_chunks[i].chunks_reserved = alloc_chunks;
                    size_reserved = hlina_chunks[i].chunks_reserved*CHUNK_SIZE*4096;
                    break;
                }
            }
            else
            {
                /* skip the used chunks */
                i += hlina_chunks[i].chunks_reserved - 1;
            }
        }
    }
    else
    {
        for(i = 0; i < chunks; i++)
        {

            if(!hlina_chunks[i].used && (hlina_chunks[i].size >= size))
            {
                *busaddr = hlina_chunks[i].bus_address;
                hlina_chunks[i].used = 1;
                hlina_chunks[i].file_id = *((int *) (filp->private_data));
                size_reserved = hlina_chunks[i].size;
                break;
            }
        }
    }

    if(*busaddr == 0)
    {
        printk("memalloc: Allocation FAILED: size = %d\n", size);
    }
    else
    {
        PDEBUG("MEMALLOC OK: size: %d, size reserved: %d\n", size,
               size_reserved);
    }

    return 0;
}

/* Free a buffer based on bus address */
static int FreeMemory(unsigned long busaddr)
{
    int i = 0;
    int j = 0;

    if (mm_priv.alloc_method == MEMALLOC_DYNAMIC)
    {
        for(i = 0; i < chunks; i++)
        {
            if(hlina_chunks[i].bus_address == busaddr)
            {
#if 0
                for (j = i; j < i + hlina_chunks[i].chunks_reserved; j++)
                {
                    hlina_chunks[j].used = 0;
                    hlina_chunks[j].file_id = ID_UNUSED;
                }
#endif
                hlina_chunks[i].used = 0;
                hlina_chunks[i].file_id = ID_UNUSED;
                hlina_chunks[i].chunks_reserved = 0;
                break;      
            }
        }
    }
    else
    {
        for(i = 0; i < chunks; i++)
        {
            if(hlina_chunks[i].bus_address == busaddr)
            {
               hlina_chunks[i].used = 0;
               hlina_chunks[i].file_id = ID_UNUSED;
               break;
            }
        }
    }

    return 0;
}

/* Reset "used" status */
void ResetMems(void)
{
    int i = 0;
    unsigned int ba = mm_priv.start;

    for(i = 0; i < chunks; i++)
    {

        hlina_chunks[i].bus_address = ba;
        hlina_chunks[i].used = 0;
        hlina_chunks[i].file_id = ID_UNUSED;
        hlina_chunks[i].size = 4096 * size_table[i];
        hlina_chunks[i].chunks_reserved = 0;

        ba += hlina_chunks[i].size;
    }

    printk("memalloc: %d bytes (%dMB) configured. Check RAM size!\n",
           ba - (unsigned int)(mm_priv.start),
          (ba - (unsigned int)(mm_priv.start)) / (1024 * 1024));

    if(ba - (unsigned int)(mm_priv.start) > 96 * 1024 * 1024)
    {
        PDEBUG("MEMALLOC ERROR: MEMORY ALLOC BUG\n");
    }

}

static int __devinit memalloc_probe (struct platform_device *pdev)
{
	struct resource *pr, *mr;
	int ret = 0;

	mm_priv.pdev = pdev;

	pr = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (pr == NULL) {
		dev_err(&pdev->dev, "no memory resource defined\n");
		ret = -ENODEV;
		goto err;
	}

	mm_priv.start = pr->start;	
	mm_priv.size = resource_size(pr);
	mm_priv.alloc_method = MEMALLOC_DYNAMIC;

	mr = request_mem_region(pr->start, mm_priv.size, pdev->name);
	if(mr == NULL) {
		dev_err(&pdev->dev, "failed to request memory region\n");
		ret = -EBUSY;
		goto err;
	}

	if(!memalloc_init()){
		goto err_release;
	}
	printk("Mem allocate for buffer %d Mb", mm_priv.size / (1024 * 1024));
//    platform_set_drvdata(pdev, ud);
	return 0;


err_release:
	release_mem_region(pr->start, resource_size(pr));
err:
	return ret;
}

static int __devexit memalloc_remove(struct platform_device *pdev)
{

	memalloc_cleanup();
    release_mem_region(mm_priv.start, mm_priv.size);
    return 0;
}

static struct platform_driver memalloc_driver = {
	.probe		= memalloc_probe,
	.remove		= __devexit_p(memalloc_remove),
	.driver		= {
		.name	= "vpu_mem",
		.owner	= THIS_MODULE,
	},
};

static int __init memalloc_module_init(void)
{
    return platform_driver_register(&memalloc_driver);
}

static void __exit memalloc_module_exit(void)
{
    platform_driver_unregister(&memalloc_driver);
}

module_init(memalloc_module_init);
module_exit(memalloc_module_exit);

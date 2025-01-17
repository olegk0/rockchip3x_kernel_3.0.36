/*
 * simple memory allocator integrated with UMP
 * Copyright (C) 2015 olegvedi@gmail.com
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include "ump_kernel_interface_ref_drv.h"
#include <linux/rk_fb.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include "rk_ump.h"

#define DBG_LVL 2

#if DBG_LVL
#define DDEBUG(lvl, fmt, args...) {if(lvl <= DBG_LVL) printk("%s - " fmt "\n",__func__, ##args);}
#else
#define DDEBUG(...)
#endif

#define USI_MAX_CONNECTIONS 20

#define USI_MAX_RES 7

typedef enum
{
	RES_FREE=0,
	RES_USED,
} usi_mbstat;

struct usi_res_s {
	int    res_ids[USI_MAX_RES];
    int    ref_id;
};

struct usi_mbs_s {
	struct list_head	node;
	usi_mbstat	stat;
	struct usi_ump_mbs	uum;
	void    *umh;
	void    *virt;
	int     ref_id;
};

struct usi_private_s {
	struct platform_device  *pdev;
	u32			begin;
	u32			end;
	u32			full_size;
	u32			used;
	int         con_ids[USI_MAX_CONNECTIONS];
    struct usi_res_s    usi_res;
};

static DEFINE_MUTEX(usi_lock);
static LIST_HEAD(usi_list);

static struct usi_private_s usi_priv;

static struct usi_ump_mbs *usi_ump_alloc_mb(u32 size, int ref_id)
{
	struct usi_mbs_s *umbs, *new_umbs=NULL;
	ump_dd_physical_block upb;
	ump_dd_handle *umh;
	ump_secure_id secure_id;
	int find=0;

	DDEBUG(3, "Request on %d bytes", size);
	size = UMP_SIZE_ALIGN(size);
	mutex_lock(&usi_lock);
	list_for_each_entry(umbs, &usi_list, node) {
		if(umbs->stat == RES_FREE && umbs->uum.size >= size){
			find = 1;
			break;
		}
	}
//    mutex_unlock(&usi_lock);

    if(!find){
		DDEBUG(1, "Free buf Not found");
		goto err;
	}
	
	if(umbs->uum.size > size){
		new_umbs = kzalloc(sizeof(*new_umbs), GFP_KERNEL);
		if(!new_umbs){
			DDEBUG(1,"Don`t allocate mem");
			goto err;
		}
	}

	upb.addr = umbs->uum.addr;
	upb.size = size;

	umh = ump_dd_handle_create_from_phys_blocks(&upb, 1);
	if(umh == UMP_DD_HANDLE_INVALID){
	    DDEBUG(1,"Error get ump handle");
	    goto err_free;
	}
	secure_id = ump_dd_secure_id_get(umh);//UMP_INVALID_SECURE_ID

//	mutex_lock(&usi_lock);
	if(new_umbs){// need resize block
		new_umbs->uum.addr = upb.addr;
		new_umbs->uum.size = size;
		umbs->uum.addr += size;
		umbs->uum.size -= size;
//		mutex_lock(&usi_lock);
		list_add_tail(&new_umbs->node, &umbs->node);
//		mutex_unlock(&usi_lock);
	}else{//alloc exist block
		new_umbs = umbs;
	}
	new_umbs->stat = RES_USED;
	new_umbs->umh = umh;
	new_umbs->uum.secure_id = secure_id;
    new_umbs->ref_id = ref_id;
	
	usi_priv.used += size;
	mutex_unlock(&usi_lock);
	
    DDEBUG(3, "Secure id:%d ref_id:%d used:%d paddr:%X",secure_id, ref_id, usi_priv.used, new_umbs->uum.addr);
    return &new_umbs->uum;

err_free:
	if(new_umbs)
		kfree(new_umbs);
err:
	mutex_unlock(&usi_lock);
	return NULL;
}

static int usi_ump_free_mb(int ref_id, ump_secure_id secure_id)
{
    struct usi_mbs_s *umbs, *tumbs, *umbs_prev=NULL;
    int find=0,ret=0, i;

    if(list_empty(&usi_list))
		return -EINVAL;

    DDEBUG(3,"Release mem block ref_id:%d secure_id:%d", ref_id, secure_id);
    
    mutex_lock(&usi_lock);
    list_for_each_entry(umbs, &usi_list, node) {
		if(umbs->stat == RES_USED && ((secure_id && umbs->uum.secure_id == secure_id ) ||//del by secure id
                ( ref_id && umbs->ref_id == ref_id )) ){ //del by ref id
            i=100;
            if(umbs->umh)
                while(ump_dd_reference_release(umbs->umh) && i--); //WARNING  it requires change ump kernel driver (ump_dd_reference_release func)
            umbs->umh = NULL;
            umbs->stat = RES_FREE;
            usi_priv.used -= umbs->uum.size;
        
			find = 1;
            if(secure_id)
                break;
		}
    }
//    mutex_unlock(&usi_lock);

    if(find){
        DDEBUG(3, "Mem block released");
//defrag
//		mutex_lock(&usi_lock);
		list_for_each_entry_safe(umbs, tumbs, &usi_list, node) {
			if(umbs_prev && umbs_prev->stat == RES_FREE && umbs->stat == RES_FREE){
				umbs->uum.size += umbs_prev->uum.size;
				umbs->uum.addr -= umbs_prev->uum.size;
				list_del(&umbs_prev->node);
				kfree(umbs_prev);
				umbs_prev = NULL;
			}
			umbs_prev = umbs;
		}
//		mutex_unlock(&usi_lock);
    }else{
		DDEBUG(1, "Mem block don`t found");
		ret = -ENODEV;
    }
	mutex_unlock(&usi_lock);
    DDEBUG(2, "--------used:%d ref_res:%d", usi_priv.used, usi_priv.usi_res.ref_id);
    return ret;
}

static long usi_ump_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned long __user *puser = (unsigned long __user *) arg;
    u32 param[2];
    unsigned int res;
    int ret = -EFAULT;
	struct usi_ump_mbs	*uum, luum;
	struct usi_ump_mbs_info	uumi;
    struct usi_res_s *ur = file->private_data;

    BUG_ON(ur == NULL);
    
    DDEBUG(3,"ref_id:%d cmd:%ul",ur->ref_id, cmd);
    switch (cmd) {
    case USI_ALLOC_MEM_BLK:
		if (copy_from_user(&luum, puser, sizeof(luum)))
		    break;
		uum = usi_ump_alloc_mb(luum.size, ur->ref_id);
//	put_user((unsigned int), puser);
		if(!uum)
			break;
		if(copy_to_user(puser, uum, sizeof(*uum)))
			break;
		ret = 0;
        break;
    case USI_FREE_MEM_BLK:
		if (copy_from_user(param, puser, sizeof(u32)))
		    break;
		ret = usi_ump_free_mb( 0, param[0]);
        break;
    case USI_GET_INFO:
		uumi.size_full = usi_priv.full_size;
		uumi.size_used = usi_priv.used;
		if(copy_to_user(puser, &uumi, sizeof(uumi)))
			break;
		ret = 0;
        break;
	case USI_FREE_ALL_BLKS:
		ret = usi_ump_free_mb( ur->ref_id, 0);
        break;
//----------------
    case USI_GET_RES_REFS:
		ret = usi_priv.usi_res.ref_id;
        break;
    case USI_ALLOC_RES:
        if (copy_from_user(&res, puser, sizeof(res)) || res >= USI_MAX_RES){
            DDEBUG(1,"Error res req:%d", res);
		    break;
        }
        DDEBUG(3,"Request res:%d", res);
        mutex_lock(&usi_lock);
        if(usi_priv.usi_res.res_ids[res] == RES_FREE){
            ur->res_ids[res]  = RES_USED;
            usi_priv.usi_res.res_ids[res] = RES_USED;
            usi_priv.usi_res.ref_id++;
            res = usi_priv.usi_res.ref_id;
            ret = 0;
        }
        else{
            DDEBUG(3,"Res used:%d",res);
            ret = -EBUSY;
        }
        mutex_unlock(&usi_lock);
        break;
    case USI_FREE_RES:
        if (copy_from_user(&res, puser, sizeof(res)) || res >= USI_MAX_RES ){
            DDEBUG(1,"Error res free:%d", res);
		    break;
        }
        DDEBUG(3,"Free res:%d", res);
        mutex_lock(&usi_lock);
        if(usi_priv.usi_res.res_ids[res] == RES_USED){
            ur->res_ids[res]  = RES_FREE;
            usi_priv.usi_res.res_ids[res] = RES_FREE;
            usi_priv.usi_res.ref_id--;
            res = usi_priv.usi_res.ref_id;
            ret = 0;
        }
        else{
            DDEBUG(1,"Error: res:%d is not used", res);
            ret = -EINVAL;
        }
        mutex_unlock(&usi_lock);
        DDEBUG(2,"Res used:%d", usi_priv.usi_res.ref_id);
        break;
    }
    
    return ret;
}

int usi_ump_release(struct inode *inode, struct file *file)
{
    struct usi_res_s *ur = file->private_data;
    int i;
    
    BUG_ON(ur == NULL);
    BUG_ON(ur->ref_id < 1);
    usi_ump_free_mb(ur->ref_id, 0);
    mutex_lock(&usi_lock);    
    usi_priv.con_ids[ur->ref_id-1] = 0;
    
    if(usi_priv.usi_res.ref_id){
        for(i=0;i<USI_MAX_RES;i++){
            if(ur->res_ids[i] == RES_USED){
//                ur->res_ids[i] = 0;
            	usi_priv.usi_res.res_ids[i] = RES_FREE;
                usi_priv.usi_res.ref_id--;
            }
        }
    }
     
    kfree(ur);       
    mutex_unlock(&usi_lock);
    DDEBUG(3, "id:%d ref_res:%d", ur->ref_id, usi_priv.usi_res.ref_id);
    return 0;
}

int usi_ump_open(struct inode *inode, struct file *file)
{
    int cnt, ret=0;
    struct usi_res_s *ur;
    
    mutex_lock(&usi_lock);
    file->private_data = NULL;
    for(cnt=0;cnt<USI_MAX_CONNECTIONS;cnt++)
        if(usi_priv.con_ids[cnt] == RES_FREE) break;
    if(cnt == USI_MAX_CONNECTIONS)
        ret = -EAGAIN;
    else{
        ur = kzalloc(sizeof(*ur), GFP_KERNEL);
		if(!ur){
			DDEBUG(1,"Don`t allocate mem");
        }
        else{
            usi_priv.con_ids[cnt] = RES_USED;
            cnt++;
            ur->ref_id = cnt;
            file->private_data = ur;
        }
    }
    mutex_unlock(&usi_lock);
    DDEBUG(3, "id:%d", cnt);
    return ret;
}

static const struct file_operations usi_fops = {
    .owner  = THIS_MODULE,
    .unlocked_ioctl = usi_ump_ioctl,
    .open = usi_ump_open,
    .release = usi_ump_release,
};

static struct miscdevice usi_dev = {
    MISC_DYNAMIC_MINOR,
    USI_UMP_DRV_NAME,
//    .minor
    &usi_fops
};

static int usi_ump_init_list(void)
{
	struct usi_mbs_s *umbs;

	umbs = kzalloc(sizeof(*umbs), GFP_KERNEL);
	if(! umbs){
		DDEBUG(1, "Don`t allocate mem for first block");
		return -ENOMEM;
	}
	
	umbs->stat = RES_FREE;
	umbs->uum.addr = UMP_SIZE_ALIGN(usi_priv.begin);
	umbs->uum.size = usi_priv.full_size - (umbs->uum.addr - usi_priv.begin);
	umbs->umh = NULL;
	list_add(&umbs->node, &usi_list);
	
	return 0;
}

static void usi_ump_free_all(void)
{
	int i;
    struct usi_mbs_s *umbs, *tumbs;

    DDEBUG(3, "Release all mem blocks");
	mutex_lock(&usi_lock);
    list_for_each_entry_safe(umbs, tumbs, &usi_list, node) {
//		if(!list_empty(&usi_list)){
			if(umbs){
				if(umbs->umh){
					i = 100;
					while(ump_dd_reference_release(umbs->umh) && i--); //WARNING  it requires change ump kernel driver (ump_dd_reference_release func)
				}
				list_del(&umbs->node);
				kfree(umbs);
				umbs = NULL;
			}
//		}
    }
    usi_priv.used = 0;
	mutex_unlock(&usi_lock);
}

static int __devinit usi_ump_probe (struct platform_device *pdev)
{
	struct resource *pr, *mr;
	int ret = 0, i;

	usi_priv.pdev = pdev;

	pr = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (pr == NULL) {
		dev_err(&pdev->dev, "no memory resource defined\n");
		ret = -ENODEV;
		goto err_free;
	}

	usi_priv.begin = pr->start;
	usi_priv.end = pr->end;
	usi_priv.full_size = resource_size(pr);
	usi_priv.used = 0;

	mr = request_mem_region(pr->start, usi_priv.full_size, pdev->name);
	if(mr == NULL) {
		dev_err(&pdev->dev, "failed to request memory region\n");
		ret = -EBUSY;
		goto err_free;
	}
    
	if(usi_ump_init_list()){
		goto err_free;
	}

	for(i=0;i<USI_MAX_CONNECTIONS;i++)
        	usi_priv.con_ids[i] = RES_FREE;

	for(i=0;i<USI_MAX_RES;i++)
        	usi_priv.usi_res.res_ids[i] = RES_FREE;
            
    usi_priv.usi_res.ref_id = 0;
	
	ret = misc_register(&usi_dev);
	if(ret){
		dev_err(&pdev->dev, "Unable to register usi_ump\n" );
		goto err_free_mem2;
	}
    mutex_init(&usi_lock);
	printk("RK_UMP: Mem allocate for buffer %d Mb from:%X\n", usi_priv.full_size / (1024 * 1024), usi_priv.begin);
//    platform_set_drvdata(pdev, ud);
	return 0;

err_free_mem2:
	usi_ump_free_all();
//err_free_mem:
	release_mem_region(pr->start, resource_size(pr));
err_free:
//	kfree();
	return ret;
}

static int __devexit usi_ump_remove(struct platform_device *pdev)
{
//    struct usi_device *ud;

//    ud = platform_get_drvdata(pdev);
	usi_ump_free_all();
    release_mem_region(usi_priv.begin, usi_priv.full_size);
    misc_deregister(&usi_dev);
    return 0;
}

static struct platform_driver usi_ump_driver = {
	.probe		= usi_ump_probe,
	.remove		= __devexit_p(usi_ump_remove),
	.driver		= {
		.name	= "rk-ump",
		.owner	= THIS_MODULE,
	},
};

static int __init usi_ump_init(void)
{
    return platform_driver_register(&usi_ump_driver);
}

static void __exit usi_ump_exit(void)
{
    platform_driver_unregister(&usi_ump_driver);
}

module_init(usi_ump_init);
module_exit(usi_ump_exit);
MODULE_AUTHOR("olegvedi@gmail.com");
MODULE_LICENSE("GPL");

/*
 * Decoder device driver (kernel module headers)
 *
 * Copyright (C) 2011  Hantro Products Oy.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
------------------------------------------------------------------------------*/

#ifndef _HX170DEC_H_
#define _HX170DEC_H_

#ifdef __KERNEL__
#include <linux/cdev.h>     /* character device definitions */
#endif
#include <linux/ioctl.h>    /* needed for the _IOW etc stuff used later */

/*
 * Macros to help debugging
 */
#undef PDEBUG   /* undef it, just in case */
#if 0
//CONFIG_VIDEO_SPEAR_VIDEODEC_DEBUG
#  ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#define PDEBUG(fmt, args...) printk(KERN_INFO "hx170dec: " fmt, ## args)
#else
/* This one for user space */
#define PDEBUG(fmt, args...) printf(__FILE__ ":%d: " fmt, __LINE__ , ## args)
#endif
#else
#define PDEBUG(fmt, args...)  /* not debugging: nothing */
#endif

typedef enum VPU_FREQ {
    VPU_FREQ_NC=0,
    VPU_FREQ_200M,
    VPU_FREQ_266M,
    VPU_FREQ_300M,
    VPU_FREQ_400M,
    VPU_FREQ_DEFAULT,
    VPU_FREQ_BUT,
} VPU_FREQ;

/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define HX170DEC_IOC_MAGIC  'k'
/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */

/* decode/pp time for HW performance */
#define HX170DEC_HW_PERFORMANCE    _IO(HX170DEC_IOC_MAGIC, 2)
#define HX170DEC_IOCGHWOFFSET      _IOR(HX170DEC_IOC_MAGIC,  3, unsigned long *)
#define HX170DEC_IOCGHWIOSIZE      _IOR(HX170DEC_IOC_MAGIC,  4, unsigned int *)

#define HX170DEC_IOC_CLI           _IO(HX170DEC_IOC_MAGIC,  5)
#define HX170DEC_IOC_STI           _IO(HX170DEC_IOC_MAGIC,  6)

#ifdef ENABLE_FASYNC
/* the client is pp instance */
#define HX170DEC_PP_INSTANCE       _IO(HX170DEC_IOC_MAGIC, 1)
#else
#define HX170DEC_IOC_WAIT_DEC      _IOW(HX170DEC_IOC_MAGIC,  7, unsigned int *)
#define HX170DEC_IOC_WAIT_PP       _IOW(HX170DEC_IOC_MAGIC,  8, unsigned int *)
#endif
#define HX170DEC_GET_VPU_FREQ      _IOR(HX170DEC_IOC_MAGIC,  9, VPU_FREQ *)
#define HX170DEC_SET_VPU_FREQ      _IOW(HX170DEC_IOC_MAGIC,  10, VPU_FREQ *)

#define HX170DEC_IOC_MAXNR 10

#ifdef __KERNEL__
struct hx170dec_dev {
    struct cdev cdev;
    struct class *hx170dec_class;
};
#endif

#endif /* !_HX170DEC_H_ */

/*
 * Copyright (C) 2010, 2012-2013, 2015 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _MALI_DRV_H_
#define _MALI_DRV_H_

#define DRIVER_AUTHOR       "ARM"
#define DRIVER_NAME     "mali_drm"
#define DRIVER_DESC     "DRM module for Mali-200, Mali-400"
#define DRIVER_DATE     "20100520"
#define DRIVER_MAJOR        0
#define DRIVER_MINOR        1
#define DRIVER_PATCHLEVEL   0

#include "drm/drm_sman.h"

typedef struct drm_mali_private
{
	drm_local_map_t *mmio;
	unsigned int idle_fault;
	struct drm_sman sman;
	int vram_initialized;
	unsigned long vram_offset;
} drm_mali_private_t;

int mali_drm_init(struct platform_device *dev);
void mali_drm_exit(struct platform_device *dev);

extern int mali_idle(struct drm_device *dev);
extern void mali_reclaim_buffers_locked(struct drm_device *dev, struct drm_file *file_priv);
extern void mali_lastclose(struct drm_device *dev);
extern struct drm_ioctl_desc mali_ioctls[];
extern int mali_max_ioctl;

#define DRM_MEM_DRIVER 2
#define DRM_IOCTL_DEF(ioctl, _func, _flags) \
         [DRM_IOCTL_NR(ioctl)] = {.cmd = ioctl, .func = _func, .flags = _flags, .cmd_drv = 0}

#endif /* _MALI_DRV_H_ */

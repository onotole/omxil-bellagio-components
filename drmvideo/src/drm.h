/**
 * src/drm.h
 *
 * DRM functions header file.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#ifndef _DRM_H

#include "xf86drm.h"
#include "xf86drmMode.h"
#include <drm/exynos_drm.h>

#include <drm/drm.h>

/*
 * Mode setting with the kernel interfaces is a bit of a chore.
 * First you have to find the connector in question and make sure the
 * requested mode is available.
 * Then you need to find the encoder attached to that connector so you
 * can bind it with a free crtc.
 */
struct connector {
	uint32_t id;
	char mode_str[64];
	drmModeModeInfo *mode;
	drmModeEncoder *encoder;
	int crtc;
	unsigned int fb_id[2], current_fb_id;
	struct timeval start;

	int swap_count;
};

/* Create a gem buffer */
int exynos_gem_create(int fd, struct drm_exynos_gem_create *gem);
/* mmap the created buffer */
int exynos_gem_mmap(int fd, struct drm_exynos_gem_mmap *in_mmap);
/* Close the gem buffer */
int exynos_gem_close(int fd, struct drm_gem_close *gem_close);
/* Find the connector and mode */
void connector_find_mode(int drm_fd, drmModeRes *resources, struct connector *c);
/* Print a list of available connectors */
void dump_connectors(int drm_fd, drmModeRes *resources);

/* Handler for the page_flip event */
void
page_flip_handler(int fd, unsigned int frame,
                  unsigned int sec, unsigned int usec, void *data);

#endif /* _DRM_H */

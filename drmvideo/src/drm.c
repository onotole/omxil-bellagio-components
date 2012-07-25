/**
 * src/drm.c
 *
 * DRM functions.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

#include <poll.h>

#include <semaphore.h>
#include <pthread.h>

#include <sys/time.h>

#include <sys/mman.h>
#include <math.h>

#include <bellagio/omxcore.h>

#include "xf86drm.h"
#include "xf86drmMode.h"
#include <drm/exynos_drm.h>
#include "drm.h"

#include <drm/drm.h>

void connector_find_mode(int drm_fd, drmModeRes *resources, struct connector *c)
{
	drmModeConnector *connector;
	int i, j;

	/* First, find the connector & mode */
	c->mode = NULL;
	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(drm_fd, resources->connectors[i]);

		if (!connector) {
			DEBUG(DEB_LEV_ERR, "could not get connector %i: %s\n",
				resources->connectors[i], strerror(errno));
			drmModeFreeConnector(connector);
			continue;
		}

		if (!connector->count_modes) {
			drmModeFreeConnector(connector);
			continue;
		}

		if (connector->connector_id != c->id) {
			drmModeFreeConnector(connector);
			continue;
		}

		for (j = 0; j < connector->count_modes; j++) {
			c->mode = &connector->modes[j];
			if (!strcmp(c->mode->name, c->mode_str))
				break;
		}

		/* Found it, break out */
		if (c->mode)
			break;

		drmModeFreeConnector(connector);
	}

	if (!c->mode) {
		DEBUG(DEB_LEV_ERR, "failed to find mode \"%s\"\n", c->mode_str);
		return;
	}

	/* Now get the encoder */
	for (i = 0; i < resources->count_encoders; i++) {
		c->encoder = drmModeGetEncoder(drm_fd, resources->encoders[i]);

		if (!c->encoder) {
			DEBUG(DEB_LEV_ERR, "could not get encoder %i: %s\n",
				resources->encoders[i], strerror(errno));
			drmModeFreeEncoder(c->encoder);
			continue;
		}

		if (c->encoder->encoder_id  == connector->encoder_id)
			break;

		drmModeFreeEncoder(c->encoder);
	}

	if (c->crtc == -1)
		c->crtc = c->encoder->crtc_id;
}

int exynos_gem_create(int fd, struct drm_exynos_gem_create *gem)
{
	int ret;

	if (!gem) {
		DEBUG(DEB_LEV_ERR, "gem object is null.\n");
		return -EFAULT;
	}

	ret = ioctl(fd, DRM_IOCTL_EXYNOS_GEM_CREATE, gem);
	if (ret < 0) {
		DEBUG(DEB_LEV_ERR, "failed to create gem buffer: %s\n",
				strerror(-ret));
		return ret;
	}

	return 0;
}

int exynos_gem_mmap(int fd, struct drm_exynos_gem_mmap *in_mmap)
{
	int ret;

	ret = ioctl(fd, DRM_IOCTL_EXYNOS_GEM_MMAP, in_mmap);
	if (ret < 0) {
		DEBUG(DEB_LEV_ERR, "failed to get buffer offset: %s\n",
				strerror(-ret));
		return ret;
	}

	return 0;
}

int exynos_gem_close(int fd, struct drm_gem_close *gem_close)
{
	int ret;

	ret = ioctl(fd, DRM_IOCTL_GEM_CLOSE, gem_close);
	if (ret < 0) {
		 DEBUG(DEB_LEV_ERR, "failed to close gem buffer: %s\n",
					strerror(-ret));
		return ret;
	}

	return 0;
}

void page_flip_handler(int fd, unsigned int frame,
                  unsigned int sec, unsigned int usec, void *data)
{
        DEBUG(DEB_LEV_FULL_SEQ, "Handled page flip %d %d %d\n", frame, sec, usec);
}

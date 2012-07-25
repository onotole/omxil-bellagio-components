/**
  @file src/components/colorconv/omx__colorconv_component.h

  This component implements a color converter using V4L2 S5P FIMC driver.

  Copyright (C) 2011  Samsung Electronics Co., Ltd.

  This library is free software; you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License as published by the Free
  Software Foundation; either version 2.1 of the License, or (at your option)
  any later version.

  This library is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
  details.

  You should have received a copy of the GNU Lesser General Public License
  along with this library; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St, Fifth Floor, Boston, MA
  02110-1301  USA

  $Date: 2008-06-27 12:00:23 +0200 (Fri, 27 Jun 2008) $
  Revision $Rev: 554 $
  Author $Author: pankaj_sen $
*/

#ifndef _OMX_FIMC_COLORCONV_COMPONENT_H_
#define _OMX_FIMC_COLORCONV_COMPONENT_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <linux/videodev2.h>
#include <bellagio/omx_base_filter.h>
#include <bellagio/omx_base_video_port.h>

#define COLOR_CONV_BASE_NAME "OMX.st.video_colorconv"
#define COLOR_CONV_FIMC_NAME "OMX.st.video_colorconv.fimc"
#define COLOR_CONV_FIMC_ROLE "video_colorconv.fimc"

#define MIN(a,b)  (((a) < (b)) ? (a) : (b))
#define MAX(a,b)  (((a) > (b)) ? (a) : (b))

struct buffer {
	char				*addr[VIDEO_MAX_PLANES];
	unsigned long		size[VIDEO_MAX_PLANES];
	struct v4l2_plane	planes[VIDEO_MAX_PLANES];
	unsigned int		num_planes;
	unsigned int		index;
	unsigned int		width;
	unsigned int		height;
	unsigned int		stride;
};

/** V4L FIMC color converter component port structure.
  */
DERIVEDCLASS(omx_v4l_colorconv_PortType, omx_base_video_PortType)
#define omx_v4l_colorconv_PortType_FIELDS omx_base_video_PortType_FIELDS \
  /** @param omxConfigCrop Crop rectangle of image */ \
  OMX_CONFIG_RECTTYPE omxConfigCrop; \
  /** @param omxConfigRotate Set rotation angle of image */ \
  OMX_CONFIG_ROTATIONTYPE omxConfigRotate; \
  /** @param omxConfigMirror Set mirroring of image */ \
  OMX_CONFIG_MIRRORTYPE omxConfigMirror; \
  /** @param omxConfigScale Set scale factors */ \
  OMX_CONFIG_SCALEFACTORTYPE omxConfigScale; \
  /** @param nBuffersRequested Number of buffers requested at v4l2 devicec */ \
  int nBuffersRequested; \
  /** @param omxConfigOutputPosition Top-Left offset from intermediate buffer to output buffer */ \
  OMX_CONFIG_POINTTYPE omxConfigOutputPosition; \
  /** @param pixelformat The V4L2 pixel format FOURCC */ \
  unsigned int pixelformat;
ENDCLASS(omx_v4l_colorconv_PortType)

/** V4L FIMC color converter component private structure.
  */
DERIVEDCLASS(omx_v4l_colorconv_PrivateType, omx_base_filter_PrivateType)
#define omx_v4l_colorconv_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  /** @param V4L2 mem-to-mem device file name */ \
  char devnode_name[32]; \
  /** @param vid_fd V4L2 mem-to-mem device file descriptor */ \
  int vid_fd; \
  OMX_BOOL samsungProprietaryCommunication;
ENDCLASS(omx_v4l_colorconv_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_v4l_colorconv_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,
					    OMX_STRING cComponentName);
OMX_ERRORTYPE omx_v4l_colorconv_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_v4l_colorconv_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_v4l_colorconv_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_v4l_colorconv_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,
					       internalRequestMessageType *message);

OMX_ERRORTYPE omx_v4l_colorconv_AllocateBuffer(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE** pBuffer,
  OMX_U32 nPortIndex,
  OMX_PTR pAppPrivate,
  OMX_U32 nSizeBytes);


OMX_ERRORTYPE omx_v4l_colorconv_AllocateTunnelBuffer(
  omx_base_PortType *openmaxStandPort,
  OMX_U32 nPortIndex,
  OMX_U32 nSizeBytes);

OMX_ERRORTYPE omx_v4l_colorconv_UseBuffer(
  OMX_HANDLETYPE hComponent,
  OMX_BUFFERHEADERTYPE** ppBufferHdr,
  OMX_U32 nPortIndex,
  OMX_PTR pAppPrivate,
  OMX_U32 nSizeBytes,
  OMX_U8* pBuffer);

void omx_v4l_colorconv_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* inputbuffer,
  OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_v4l_colorconv_SetConfig(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nIndex,
  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_v4l_colorconv_GetParameter(
   OMX_HANDLETYPE hComponent,
   OMX_INDEXTYPE nParamIndex,
   OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_v4l_colorconv_SetParameter(
   OMX_HANDLETYPE hComponent,
   OMX_INDEXTYPE nParamIndex,
   OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_v4l_colorconv_GetConfig(
   OMX_HANDLETYPE hComponent,
   OMX_INDEXTYPE nIndex,
   OMX_PTR pComponentConfigStructure);

/** finds pixel format */
OMX_U32 /* enum PixelFormat */ find_ffmpeg_pxlfmt(OMX_COLOR_FORMATTYPE omx_pxlfmt);

/** stride calculation */
OMX_S32 calcStride(OMX_U32 width, OMX_COLOR_FORMATTYPE omx_pxlfmt);

OMX_ERRORTYPE omx_video_colorconv_UseEGLImage (
        OMX_HANDLETYPE hComponent,
        OMX_BUFFERHEADERTYPE** ppBufferHdr,
        OMX_U32 nPortIndex,
        OMX_PTR pAppPrivate,
	void* eglImage);

OMX_ERRORTYPE fimc_input_port_ComponentTunnelRequest(
		omx_base_PortType* openmaxStandPort,
		OMX_HANDLETYPE hTunneledComp,
		OMX_U32 nTunneledPort,
		OMX_TUNNELSETUPTYPE* pTunnelSetup);

#endif

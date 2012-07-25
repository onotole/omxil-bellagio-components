/**
 * src/omx_drmdev_sink_component.c
 *
 * This OpenMAX component implements a video sink that uses the DRM to display
 * the incoming buffers. It has been based in the OpenMAX FBDEV sink component,
 * which has originally been developed by Peter Littlefield
 *
 * Copyright (C) 2007-2009  STMicroelectronics and Agere Systems
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

#include <errno.h>
#include <bellagio/omxcore.h>
#include <omx_drmdev_sink_component.h>
#include <config.h>
#include <unistd.h>

#include "xf86drm.h"
#include "xf86drmMode.h"
#include <drm/exynos_drm.h>
#include <drm/drm.h>
#include "drm.h"

/** height offset - reqd adjust the display position - at the centre of upper half of screen */
#define HEIGHT_OFFSET 0

#define DRMDEV_SINK_COMP_ROLE "drmdev.drmdev_sink"

/** we assume, frame rate = 25 fps ; so one frame processing time = 40000 us */
static OMX_U32 nFrameProcessTime = 40000; // in micro second

/** Counter of sink component instance*/
static OMX_U32 nodrmdev_sinkInstance=0;

/** Maximum number of sink component instances */
#define MAX_COMPONENT_DRMDEVSINK 2

/** Returns a time value in milliseconds based on a clock starting at
 *  some arbitrary base. Given a call to GetTime that returns a value
 *  of n a subsequent call to GetTime made m milliseconds later should
 *  return a value of (approximately) (n+m). This method is used, for
 *  instance, to compute the duration of call. */
long GetTime() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return ((long)now.tv_sec) * 1000 + ((long)now.tv_usec) / 1000;
}

/** The Constructor
 *
 * @param openmaxStandComp is the handle to be constructed
 * @param cComponentName is the name of the constructed component
 *
 */
OMX_ERRORTYPE omx_drmdev_sink_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  omx_drmdev_sink_component_PortType *pPort;
  omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n", __func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_drmdev_sink_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  } else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", __func__, (int)openmaxStandComp->pComponentPrivate);
  }

  omx_drmdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_drmdev_sink_component_Private->ports = NULL;
  omx_drmdev_sink_component_Private->bDoubleBuffering = OMX_TRUE;

  /** we could create our own port structures here
    * fixme maybe the base class could use a "port factory" function pointer?
    */
  err = omx_base_sink_Constructor(openmaxStandComp, cComponentName);

  omx_drmdev_sink_component_Private->sPortTypesParam[OMX_PortDomainVideo].nStartPortNumber = 0;
  omx_drmdev_sink_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts = 1;

  /** Allocate Ports and call port constructor. */
  if ((omx_drmdev_sink_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
       omx_drmdev_sink_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts)  && !omx_drmdev_sink_component_Private->ports) {
    omx_drmdev_sink_component_Private->ports = calloc((omx_drmdev_sink_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                                                      omx_drmdev_sink_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts), sizeof(omx_base_PortType *));
    if (!omx_drmdev_sink_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    omx_drmdev_sink_component_Private->ports[0] = calloc(1, sizeof(omx_drmdev_sink_component_PortType));
    if (!omx_drmdev_sink_component_Private->ports[0]) {
      return OMX_ErrorInsufficientResources;
    }
    base_video_port_Constructor(openmaxStandComp, &omx_drmdev_sink_component_Private->ports[0], 0, OMX_TRUE);
  }

  pPort = (omx_drmdev_sink_component_PortType *) omx_drmdev_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];

  /** Domain specific section for the allocated port. */

  pPort->sPortParam.format.video.nFrameWidth = 100;
  pPort->sPortParam.format.video.nFrameHeight = 100;
  pPort->sPortParam.format.video.nBitrate = 0;
  pPort->sPortParam.format.video.xFramerate = 25;
  pPort->sPortParam.format.video.eColorFormat = OMX_COLOR_Format24bitRGB888;

  //  Figure out stride, slice height, min buffer size
  pPort->sPortParam.format.video.nStride = calcStride(pPort->sPortParam.format.video.nFrameWidth, pPort->sPortParam.format.video.eColorFormat);
  pPort->sPortParam.format.video.nSliceHeight = pPort->sPortParam.format.video.nFrameHeight;  //  No support for slices yet
  pPort->sPortParam.nBufferSize = (OMX_U32) abs(pPort->sPortParam.format.video.nStride) * pPort->sPortParam.format.video.nSliceHeight;

  pPort->sVideoParam.eColorFormat = OMX_COLOR_Format24bitRGB888;
  pPort->sVideoParam.xFramerate = 25;

  DEBUG(DEB_LEV_PARAMS, "In %s, bSize=%d stride=%d\n", __func__,(int)pPort->sPortParam.nBufferSize,(int)pPort->sPortParam.format.video.nStride);

  /** Set configs */
  setHeader(&pPort->omxConfigCrop, sizeof(OMX_CONFIG_RECTTYPE));
  pPort->omxConfigCrop.nPortIndex = OMX_BASE_SINK_INPUTPORT_INDEX;
  pPort->omxConfigCrop.nLeft = pPort->omxConfigCrop.nTop = 0;
  pPort->omxConfigCrop.nWidth = pPort->omxConfigCrop.nHeight = 0;

  setHeader(&pPort->omxConfigRotate, sizeof(OMX_CONFIG_ROTATIONTYPE));
  pPort->omxConfigRotate.nPortIndex = OMX_BASE_SINK_INPUTPORT_INDEX;
  pPort->omxConfigRotate.nRotation = 0;  //Default: No rotation (0 degrees)

  setHeader(&pPort->omxConfigMirror, sizeof(OMX_CONFIG_MIRRORTYPE));
  pPort->omxConfigMirror.nPortIndex = OMX_BASE_SINK_INPUTPORT_INDEX;
  pPort->omxConfigMirror.eMirror = OMX_MirrorNone;  //Default: No mirroring

  setHeader(&pPort->omxConfigScale, sizeof(OMX_CONFIG_SCALEFACTORTYPE));
  pPort->omxConfigScale.nPortIndex = OMX_BASE_SINK_INPUTPORT_INDEX;
  pPort->omxConfigScale.xWidth = pPort->omxConfigScale.xHeight = 0x10000;  //Default: No scaling (scale factor = 1)

  setHeader(&pPort->omxConfigOutputPosition, sizeof(OMX_CONFIG_POINTTYPE));
  pPort->omxConfigOutputPosition.nPortIndex = OMX_BASE_SINK_INPUTPORT_INDEX;
  pPort->omxConfigOutputPosition.nX = pPort->omxConfigOutputPosition.nY = 0; //Default: No shift in output position (0,0)

  /** set the function pointers */
  omx_drmdev_sink_component_Private->destructor = omx_drmdev_sink_component_Destructor;
  omx_drmdev_sink_component_Private->BufferMgmtFunction = omx_drmdev_sink_BufferMgmtFunction;
  pPort->Port_SendBufferFunction = omx_drmdev_sink_component_port_SendBufferFunction;
  openmaxStandComp->SetParameter = omx_drmdev_sink_component_SetParameter;
  openmaxStandComp->GetParameter = omx_drmdev_sink_component_GetParameter;
  omx_drmdev_sink_component_Private->messageHandler = omx_drmdev_sink_component_MessageHandler;

  pPort->Port_AllocateTunnelBuffer = drm_input_port_AllocateTunnelBuffer;
  pPort->Port_FreeTunnelBuffer = drm_input_port_FreeTunnelBuffer;
  pPort->ComponentTunnelRequest = base_port_ComponentTunnelRequest;

  nodrmdev_sinkInstance++;
  if(nodrmdev_sinkInstance > MAX_COMPONENT_DRMDEVSINK) {
    DEBUG(DEB_LEV_ERR, "Reached Max Instances %d\n",(int)nodrmdev_sinkInstance);
    return OMX_ErrorInsufficientResources;
  }

  return err;
}

/** The Destructor
 */
OMX_ERRORTYPE omx_drmdev_sink_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  /* frees port/s */
  if (omx_drmdev_sink_component_Private->ports) {
    for (i=0; i < (omx_drmdev_sink_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                   omx_drmdev_sink_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts); i++) {
      if(omx_drmdev_sink_component_Private->ports[i])
        omx_drmdev_sink_component_Private->ports[i]->PortDestructor(omx_drmdev_sink_component_Private->ports[i]);
    }
    free(omx_drmdev_sink_component_Private->ports);
    omx_drmdev_sink_component_Private->ports=NULL;
  }

  omx_base_sink_Destructor(openmaxStandComp);
  nodrmdev_sinkInstance--;

  return OMX_ErrorNone;
}


OMX_S32 calcStride2(omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private) {
  OMX_U32 stride;

  if(omx_drmdev_sink_component_Private->vscr_info.bits_per_pixel == 32){
    stride = omx_drmdev_sink_component_Private->fscr_info.line_length;
  } else if(omx_drmdev_sink_component_Private->vscr_info.bits_per_pixel == 8){
    stride = omx_drmdev_sink_component_Private->fscr_info.line_length*4;
  } else{
    stride = omx_drmdev_sink_component_Private->fscr_info.line_length*
           omx_drmdev_sink_component_Private->vscr_info.bits_per_pixel/8;
  }
  return stride;
}

OMX_ERRORTYPE omx_drmdev_sink_component_open(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private = openmaxStandComp->pComponentPrivate;

  omx_drmdev_sink_component_Private->drm_fd = drmOpen(DRM_MODULE, NULL);

  if (omx_drmdev_sink_component_Private->drm_fd < 0) {
	  DEBUG(DEB_LEV_ERR, "Failed to open DRM device \"%s\"\n", DRM_MODULE);
	  return OMX_ErrorHardware;
  }

  DEBUG(DEB_LEV_ERR, "Successfully opened DRM device \"%s\"\n", DRM_MODULE);

  return OMX_ErrorNone;
}


OMX_ERRORTYPE omx_drmdev_sink_component_close(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private = openmaxStandComp->pComponentPrivate;

  drmClose(omx_drmdev_sink_component_Private->drm_fd);

  DEBUG(DEB_LEV_ERR, "Successfully closed DRM device \"%s\"\n", DRM_MODULE);

  return OMX_ErrorNone;
}

/** The initialization function
  * This function opens the frame buffer device and allocates memory for display
  * also it finds the frame buffer supported display formats
  */
OMX_ERRORTYPE omx_drmdev_sink_component_Init(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_drmdev_sink_component_PortType* pPort = (omx_drmdev_sink_component_PortType *) omx_drmdev_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
  struct connector con;
  int ret;
  int i;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s (%s:%d)\n", __func__, __FILE__, __LINE__);

  omx_drmdev_sink_component_Private->drm_resources = drmModeGetResources(omx_drmdev_sink_component_Private->drm_fd);

  for (i = 0; i < pPort->sPortParam.nBufferCountActual; i++) {

	  ret = drmModeAddFB(omx_drmdev_sink_component_Private->drm_fd,
			  pPort->sPortParam.format.video.nFrameWidth,
			  pPort->sPortParam.format.video.nFrameHeight,
			  32, 32,
			  pPort->sPortParam.format.video.nFrameWidth * 4, // ??
			  omx_drmdev_sink_component_Private->drm_gem[i].handle,
			  &omx_drmdev_sink_component_Private->drm_fb[i]);

	  if (ret) {
			  DEBUG(DEB_LEV_ERR, "Failed to Add FB (num=%d)\n", i);
			  return OMX_ErrorInsufficientResources;
		  }
  }

  con.id = DRM_CON_ID;
  con.crtc = DRM_CON_CRTC;
  memset(con.mode_str, 0, sizeof(con.mode_str));

  connector_find_mode(omx_drmdev_sink_component_Private->drm_fd,
					  omx_drmdev_sink_component_Private->drm_resources,
					  &con);
  for (i = 0; i < pPort->sPortParam.nBufferCountActual; i++)
	  omx_drmdev_sink_component_Private->drm_crtc[i] = con.crtc;

  ret = drmModeSetCrtc(omx_drmdev_sink_component_Private->drm_fd,
		  con.crtc, omx_drmdev_sink_component_Private->drm_fb[0], 0, 0, &con.id, 1, con.mode);

  if (ret) {
	  DEBUG(DEB_LEV_ERR, "Failed to SetCrtc\n");
	  return OMX_ErrorInsufficientResources;
  }

  DEBUG(DEB_LEV_ERR, "Successfully mapped DRM\n");

  omx_drmdev_sink_component_Private->drm_current_buf = 0;
  omx_drmdev_sink_component_Private->fbpxlfmt = OMX_COLOR_Format32bitARGB8888;
  omx_drmdev_sink_component_Private->fbwidth = pPort->sPortParam.format.video.nFrameWidth;
  omx_drmdev_sink_component_Private->fbheight = pPort->sPortParam.format.video.nFrameHeight;
  omx_drmdev_sink_component_Private->fbbpp = 32;
  omx_drmdev_sink_component_Private->fbstride = pPort->sPortParam.format.video.nFrameWidth * 4;
  omx_drmdev_sink_component_Private->lastDrmBuffer = NULL;

  return OMX_ErrorNone;
}

/** The deinitialization function
  * It deallocates the frame buffer memory, and closes frame buffer
  */
OMX_ERRORTYPE omx_drmdev_sink_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  return OMX_ErrorNone;
}

/**  This function takes two inputs -
  * @param width is the input picture width
  * @param omx_pxlfmt is the input openmax standard pixel format
  * It calculates the byte per pixel needed to display the picture with the input omx_pxlfmt
  * The output stride for display is thus omx_drmdev_sink_component_Private->product of input width and byte per pixel
  */
OMX_S32 calcStride(OMX_U32 width, OMX_COLOR_FORMATTYPE omx_pxlfmt) {
  OMX_U32 stride;
  OMX_U32 bpp; // bit per pixel

  switch(omx_pxlfmt) {
    case OMX_COLOR_FormatMonochrome:
      bpp = 1;
      break;
    case OMX_COLOR_FormatL2:
      bpp = 2;
    case OMX_COLOR_FormatL4:
      bpp = 4;
      break;
    case OMX_COLOR_FormatL8:
    case OMX_COLOR_Format8bitRGB332:
    case OMX_COLOR_FormatRawBayer8bit:
    case OMX_COLOR_FormatRawBayer8bitcompressed:
      bpp = 8;
      break;
    case OMX_COLOR_FormatRawBayer10bit:
      bpp = 10;
      break;
    case OMX_COLOR_FormatYUV411Planar:
    case OMX_COLOR_FormatYUV411PackedPlanar:
    case OMX_COLOR_Format12bitRGB444:
    case OMX_COLOR_FormatYUV420Planar:
    case OMX_COLOR_FormatYUV420PackedPlanar:
    case OMX_COLOR_FormatYUV420SemiPlanar:
    case OMX_COLOR_FormatYUV444Interleaved:
      bpp = 12;
      break;
    case OMX_COLOR_FormatL16:
    case OMX_COLOR_Format16bitARGB4444:
    case OMX_COLOR_Format16bitARGB1555:
    case OMX_COLOR_Format16bitRGB565:
    case OMX_COLOR_Format16bitBGR565:
    case OMX_COLOR_FormatYUV422Planar:
    case OMX_COLOR_FormatYUV422PackedPlanar:
    case OMX_COLOR_FormatYUV422SemiPlanar:
    case OMX_COLOR_FormatYCbYCr:
    case OMX_COLOR_FormatYCrYCb:
    case OMX_COLOR_FormatCbYCrY:
    case OMX_COLOR_FormatCrYCbY:
      bpp = 16;
      break;
    case OMX_COLOR_Format18bitRGB666:
    case OMX_COLOR_Format18bitARGB1665:
      bpp = 18;
      break;
    case OMX_COLOR_Format19bitARGB1666:
      bpp = 19;
      break;
    case OMX_COLOR_FormatL24:
    case OMX_COLOR_Format24bitRGB888:
    case OMX_COLOR_Format24bitBGR888:
    case OMX_COLOR_Format24bitARGB1887:
      bpp = 24;
      break;
    case OMX_COLOR_Format25bitARGB1888:
      bpp = 25;
      break;
    case OMX_COLOR_FormatL32:
    case OMX_COLOR_Format32bitBGRA8888:
    case OMX_COLOR_Format32bitARGB8888:
      bpp = 32;
      break;
    default:
      bpp = 0;
      break;
  }
  stride = (width * bpp) >> 3; // division by 8 to get byte per pixel value
  return (OMX_S32) stride;
}

/**  Returns the OpenMAX color format type corresponding to an drmdev fb_var_screeninfo structure
  * @param vscr_info contains the frame buffer display settings
  * We extract the rgba configuration of the frame buffer from this structure and thereby
  * apply the appropriate OpenMAX standard color format equivalent to this configuration
  * the r,g,b,a content length is needed to get the bit per pixel values
  * r,g,b,a content offsets determine their respective positions
  */
OMX_COLOR_FORMATTYPE find_omx_pxlfmt(struct fb_var_screeninfo *vscr_info) {
  OMX_COLOR_FORMATTYPE omx_pxlfmt = OMX_COLOR_FormatUnused;

  /** check if gray scale -  if so then, switch according to bit per pixel criteria */
  if (vscr_info->grayscale) {
    switch (vscr_info->bits_per_pixel) {
      case 2:
        omx_pxlfmt = OMX_COLOR_FormatL2;
        break;
      case 4:
        omx_pxlfmt = OMX_COLOR_FormatL4;
        break;
      case 8:
        omx_pxlfmt = OMX_COLOR_FormatL8;
        break;
      case 16:
        omx_pxlfmt = OMX_COLOR_FormatL16;
        break;
      case 24:
        omx_pxlfmt = OMX_COLOR_FormatL24;
        break;
      case 32:
        omx_pxlfmt = OMX_COLOR_FormatL32;
        break;
      default:
        omx_pxlfmt = OMX_COLOR_FormatUnused;
        break;
    }
  } else {
    /** now check the rgba components and set the corresponding color formats */
    if(vscr_info->bits_per_pixel == 1) {
      omx_pxlfmt = OMX_COLOR_FormatMonochrome;
    } else if ( vscr_info->red.length == 3 && vscr_info->red.offset == 5 &&
                vscr_info->green.length == 3 && vscr_info->green.offset == 2 &&
                vscr_info->blue.length == 2 && vscr_info->blue.offset == 0 &&
                vscr_info->transp.length == 0) {
      omx_pxlfmt = OMX_COLOR_Format8bitRGB332;
    } else if ( vscr_info->red.length == 4 && vscr_info->red.offset == 8 &&
                vscr_info->green.length == 4 && vscr_info->green.offset == 4 &&
                vscr_info->blue.length == 4 && vscr_info->blue.offset == 0 &&
                vscr_info->transp.length == 0) {
      omx_pxlfmt = OMX_COLOR_Format12bitRGB444;
    } else if ( vscr_info->transp.length == 4 && vscr_info->transp.offset == 12 &&
                vscr_info->red.length == 4 && vscr_info->red.offset == 8 &&
                vscr_info->green.length == 4 && vscr_info->green.offset == 4 &&
                vscr_info->blue.length == 4 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format16bitARGB4444;
    } else if ( vscr_info->red.length == 5 && vscr_info->blue.length == 5 &&
                vscr_info->transp.length == 1 && vscr_info->transp.offset == 15 &&
                vscr_info->green.length == 5 && vscr_info->green.offset == 5 &&
                vscr_info->red.offset == 10 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format16bitARGB1555;
    } else if ( vscr_info->red.length == 5 && vscr_info->blue.length == 5 &&
                vscr_info->green.length == 6 && vscr_info->green.offset == 5 &&
                vscr_info->red.offset == 11 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format16bitRGB565;
    } else if ( vscr_info->red.length == 5 && vscr_info->blue.length == 5 &&
                vscr_info->green.length == 6 && vscr_info->green.offset == 5 &&
                vscr_info->red.offset == 0 && vscr_info->blue.offset == 11) {
      omx_pxlfmt = OMX_COLOR_Format16bitBGR565;
    } else if ( vscr_info->red.length == 6 && vscr_info->green.length == 6 &&
                vscr_info->transp.length == 0 && vscr_info->red.offset == 12 && vscr_info->green.offset == 6 &&
                vscr_info->blue.length == 6 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format18bitRGB666;
    } else if ( vscr_info->red.length == 6 && vscr_info->green.length == 6 &&
                vscr_info->transp.length == 1 && vscr_info->transp.offset == 17 &&
                vscr_info->red.offset == 11 && vscr_info->green.offset == 5 &&
                vscr_info->blue.length == 5 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format18bitARGB1665;
    } else if ( vscr_info->transp.length == 1 && vscr_info->transp.offset == 18 &&
                vscr_info->red.length == 6 && vscr_info->red.offset ==  12 &&
                vscr_info->green.length == 6 && vscr_info->green.offset == 6 &&
                vscr_info->blue.length == 6 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format19bitARGB1666;
    } else if ( vscr_info->transp.length == 0 && vscr_info->red.length == 8 &&
                vscr_info->green.length == 8 && vscr_info->blue.length == 8 &&
                vscr_info->green.offset == 8 && vscr_info->red.offset == 16 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format24bitRGB888;
    } else if ( vscr_info->transp.length == 0 && vscr_info->red.length == 8 &&
                vscr_info->green.length == 8 && vscr_info->blue.length == 8 &&
                vscr_info->green.offset == 8 && vscr_info->red.offset == 0 && vscr_info->blue.offset == 16) {
      omx_pxlfmt = OMX_COLOR_Format24bitBGR888;
    } else if ( vscr_info->transp.length == 1 && vscr_info->transp.offset == 23 &&
                vscr_info->red.length == 8 && vscr_info->red.offset == 15 &&
                vscr_info->green.length == 8 && vscr_info->green.offset == 7 &&
                vscr_info->blue.length == 7 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format24bitARGB1887;
    } else if ( vscr_info->transp.length == 1 && vscr_info->transp.offset == 24 &&
                vscr_info->red.length == 8 && vscr_info->red.offset == 16 &&
                vscr_info->green.length == 8 && vscr_info->green.offset == 8 &&
                vscr_info->blue.length == 8 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format25bitARGB1888;
    } else if ( vscr_info->transp.length == 8 && vscr_info->red.length == 8 &&
                vscr_info->green.length == 8 && vscr_info->blue.length == 8 &&
                vscr_info->transp.offset == 24 && vscr_info->red.offset == 16 &&
                vscr_info->green.offset == 8 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format32bitARGB8888;
    } else if ( vscr_info->transp.length == 8 && vscr_info->red.length == 8 &&
                vscr_info->green.length == 8 && vscr_info->blue.length == 8 &&
                vscr_info->transp.offset == 0 && vscr_info->red.offset == 8 &&
                vscr_info->green.offset == 16 && vscr_info->blue.offset == 24) {
      omx_pxlfmt = OMX_COLOR_Format32bitBGRA8888;
    } else if ( vscr_info->transp.length == 8 && vscr_info->red.length == 8 &&
                vscr_info->green.length == 8 && vscr_info->blue.length == 8 &&
                vscr_info->transp.offset == 0 && vscr_info->red.offset == 0 &&
                vscr_info->green.offset == 0 && vscr_info->blue.offset == 0) {
      omx_pxlfmt = OMX_COLOR_Format8bitRGB332;
    } else {
      omx_pxlfmt = OMX_COLOR_FormatUnused;
    }
  }

  return omx_pxlfmt;
}

/**  This function copies source image to destination image of required dimension and color formats
  * @param src_ptr is the source image string pointer
  * @param src_stride is the source image stride (src_width * byte_per_pixel)
  * @param src_width is source image width
  * @param src_height is source image height
  * @param src_offset_x is x offset value (if any) from starting pointer
  * @param src_offset_y is y offset value (if any) from starting pointer
  * @param dest_ptr is the destination image string pointer
  * @param dest_stride is the destination image stride (dest_width * byte_per_pixel)
  * @param dest_width is destination image width
  * @param dest_height is destination image height
  * @param dest_offset_x is x offset value (if any) from ending pointer
  * @param dest_offset_y is y offset value (if any) from ending pointer
  * @param cpy_width  is the source image copy width - it determines the portion of source image to be copied from source to destination image
  * @param cpy_height is the source image copy height - it determines the portion of source image to be copied from source to destination image
  * @param colorformat is the source image color format
  * @param fbpxlfmt undocumented
  */
void omx_img_copy(OMX_U8* src_ptr, OMX_S32 src_stride, OMX_U32 src_width, OMX_U32 src_height,
                  OMX_S32 src_offset_x, OMX_S32 src_offset_y,
                  OMX_U8* dest_ptr, OMX_S32 dest_stride, OMX_U32 dest_width,  OMX_U32 dest_height,
                  OMX_S32 dest_offset_x, OMX_S32 dest_offset_y,
                  OMX_S32 cpy_width, OMX_U32 cpy_height, OMX_COLOR_FORMATTYPE colorformat,OMX_COLOR_FORMATTYPE fbpxlfmt) {

  OMX_U32 i,j;
  OMX_U32 cp_byte; //equal to source image byte per pixel value
  OMX_U8 r,g,b,a;
  OMX_U8* org_src_cpy_ptr;
  OMX_U8* org_dst_cpy_ptr;

  DEBUG(DEB_LEV_FUNCTION_NAME, "%s: memcpy  %p ---> %p\n",__func__, src_ptr, dest_ptr);

  /**  CAUTION: We don't do any checking of boundaries! (FIXME - see omx_ffmpeg_colorconv_component_BufferMgmtCallback)
    * Input frame is planar, not interleaved
    * Feel free to add more formats if implementing them
    */
  if (colorformat == OMX_COLOR_FormatYUV411Planar ||
      colorformat == OMX_COLOR_FormatYUV411PackedPlanar ||
      colorformat == OMX_COLOR_FormatYUV420Planar ||
      colorformat == OMX_COLOR_FormatYUV420PackedPlanar ||
      colorformat == OMX_COLOR_FormatYUV422Planar ||
      colorformat == OMX_COLOR_FormatYUV422PackedPlanar ) {

    OMX_U32 src_luma_width;      //  Width (in columns) of the source Y plane
    OMX_U32 src_luma_height;     //  Height (in rows) of source Y plane
    OMX_S32 src_luma_stride;     //  Stride in bytes of each source Y row
    OMX_U32 src_luma_offset_x;   //  Horizontal byte offset
    OMX_U32 src_luma_offset_y;   //  Vertical offset in rows from top of plane
    OMX_U32 src_luma_offset;     //  Total byte offset to rectangle

    OMX_U32 src_chroma_width;    //  Width (in columns) of source chroma planes
    OMX_U32 src_chroma_height;   //  Height (in rows) of source chroma planes
    OMX_S32 src_chroma_stride;   //  Stride in bytes of each source chroma row
    OMX_U32 src_chroma_offset_x; //  Horizontal byte offset
    OMX_U32 src_chroma_offset_y; //  Vertical offset in rows from top of plane
    OMX_U32 src_chroma_offset;   //  Bytes to crop rectangle from start of chroma plane

    OMX_U32 dest_luma_width;     //  Width (in columns) of the destination Y plane
    OMX_U32 dest_luma_height;    //  Height (in rows) of destination Y plane
    OMX_S32 dest_luma_stride;    //  Stride in bytes of each destination Y row
    OMX_U32 dest_luma_offset_x;  //  Horizontal byte offset
    OMX_U32 dest_luma_offset_y;  //  Vertical offset in rows from top of plane
    OMX_U32 dest_luma_offset;    //  Bytes to crop rectangle from start of Y plane

    OMX_U32 dest_chroma_width;   //  Width (in columns) of destination chroma planes
    OMX_U32 dest_chroma_height;  //  Height (in rows) of destination chroma planes
    OMX_S32 dest_chroma_stride;  //  Stride in bytes of each destination chroma row
    OMX_U32 dest_chroma_offset_x;//  Horizontal byte offset
    OMX_U32 dest_chroma_offset_y;//  Vertical offset in rows from top of plane
    OMX_U32 dest_chroma_offset;  //  Bytes to crop rectangle from start of chroma plane

    OMX_U32 luma_crop_width;     //  Width in bytes of a luma row in the crop rectangle
    OMX_U32 luma_crop_height;    //  Number of luma rows in the crop rectangle
    OMX_U32 chroma_crop_width;   //  Width in bytes of a chroma row in the crop rectangle
    OMX_U32 chroma_crop_height;  //  Number of chroma rows in crop rectangle

    switch (colorformat) {
      /**  Watch out for odd or non-multiple-of-4 (4:1:1) luma resolutions (I don't check)  */
      /**  Planar vs. PackedPlanar will have to be handled differently if/when slicing is implemented */
      case OMX_COLOR_FormatYUV411Planar:
      case OMX_COLOR_FormatYUV411PackedPlanar:
        /**  OpenMAX IL spec says chroma channels are subsampled by 4x horizontally AND vertically in YUV 4:1:1.
          *  Conventional wisdom (wiki) tells us that it is only subsampled horizontally.
          *    Following OpenMAX spec anyway.  Technically I guess this would be YUV 4:1:0.
          */
        src_luma_width = src_width;
        src_luma_height = src_height;
        src_luma_stride = (OMX_S32) src_luma_width;
        src_luma_offset_x = src_offset_x;
        src_luma_offset_y = src_offset_y;

        src_chroma_width = src_luma_width  >> 2;
        src_chroma_height = src_luma_height;
        src_chroma_stride = (OMX_S32) src_chroma_width;
        src_chroma_offset_x = src_luma_offset_x  >> 2;
        src_chroma_offset_y = src_luma_offset_y;

        dest_luma_width = dest_width;
        dest_luma_height = dest_height;
        dest_luma_stride = (OMX_S32) dest_luma_width;
        dest_luma_offset_x = dest_offset_x;
        dest_luma_offset_y = dest_offset_y;

        dest_chroma_width = dest_luma_width  >> 2;
        dest_chroma_height = dest_luma_height;
        dest_chroma_stride = (OMX_S32) dest_chroma_width;
        dest_chroma_offset_x = dest_luma_offset_x  >> 2;
        dest_chroma_offset_y = dest_luma_offset_y;

        luma_crop_width = (OMX_U32) abs(cpy_width);
        luma_crop_height = cpy_height;
        chroma_crop_width = luma_crop_width  >> 2;
        chroma_crop_height = luma_crop_height;
        break;

      /**  Planar vs. PackedPlanar will have to be handled differently if/when slicing is implemented */
      case OMX_COLOR_FormatYUV420Planar:
      case OMX_COLOR_FormatYUV420PackedPlanar:
        src_luma_width = src_width;
        src_luma_height = src_height;
        src_luma_stride = (OMX_S32) src_luma_width;
        src_luma_offset_x = src_offset_x;
        src_luma_offset_y = src_offset_y;

        src_chroma_width = src_luma_width >> 1;
        src_chroma_height = src_luma_height >> 1;
        src_chroma_stride = (OMX_S32) src_chroma_width;
        src_chroma_offset_x = src_luma_offset_x >> 1;
        src_chroma_offset_y = src_luma_offset_y >> 1;

        dest_luma_width = dest_width;
        dest_luma_height = dest_height;
        dest_luma_stride = (OMX_S32) dest_luma_width;
        dest_luma_offset_x = dest_offset_x;
        dest_luma_offset_y = dest_offset_y;

        dest_chroma_width = dest_luma_width >> 1;
        dest_chroma_height = dest_luma_height >> 1;
        dest_chroma_stride = (OMX_S32) dest_chroma_width;
        dest_chroma_offset_x = dest_luma_offset_x >> 1;
        dest_chroma_offset_y = dest_luma_offset_y >> 1;

        luma_crop_width = cpy_width;
        luma_crop_height = cpy_height;
        chroma_crop_width = luma_crop_width >> 1;
        chroma_crop_height = luma_crop_height >> 1;
        break;
      /**  Planar vs. PackedPlanar will have to be handled differently if/when slicing is implemented */
      case OMX_COLOR_FormatYUV422Planar:
      case OMX_COLOR_FormatYUV422PackedPlanar:
        src_luma_width = src_width;
        src_luma_height = src_height;
        src_luma_stride = (OMX_S32) src_luma_width;
        src_luma_offset_x = src_offset_x;
        src_luma_offset_y = src_offset_y;

        src_chroma_width = src_luma_width >> 1;
        src_chroma_height = src_luma_height;
        src_chroma_stride = (OMX_S32) src_chroma_width;
        src_chroma_offset_x = src_luma_offset_x >> 1;
        src_chroma_offset_y = src_luma_offset_y;

        dest_luma_width = dest_width;
        dest_luma_height = dest_height;
        dest_luma_stride = (OMX_S32) dest_luma_width;
        dest_luma_offset_x = dest_offset_x;
        dest_luma_offset_y = dest_offset_y;

        dest_chroma_width = dest_luma_width >> 1;
        dest_chroma_height = dest_luma_height;
        dest_chroma_stride = (OMX_S32) dest_chroma_width;
        dest_chroma_offset_x = dest_luma_offset_x >> 1;
        dest_chroma_offset_y = dest_luma_offset_y;

        luma_crop_width = (OMX_U32) abs(cpy_width);
        luma_crop_height = cpy_height;
        chroma_crop_width = luma_crop_width >> 1;
        chroma_crop_height = luma_crop_height;
        break;
      default:
        DEBUG(DEB_LEV_ERR,"\n color format not supported --error \n");
        return;
    }

    /**  Pointers to the start of each plane to make things easier */
    OMX_U8* Y_input_ptr = src_ptr;
    OMX_U8* U_input_ptr = Y_input_ptr + ((OMX_U32) abs(src_luma_stride) * src_luma_height);
    OMX_U8* V_input_ptr = U_input_ptr + ((OMX_U32) abs(src_chroma_stride) * src_chroma_height);

    /**  Figure out total offsets */
    src_luma_offset = (src_luma_offset_y * (OMX_U32) abs(src_luma_stride)) + src_luma_offset_x;
    src_chroma_offset = (src_chroma_offset_y * (OMX_U32) abs(src_chroma_stride)) + src_chroma_offset_x;

    /**  If input stride is negative, reverse source row order */
    if (src_stride < 0) {
      src_luma_offset += ((OMX_U32) abs(src_luma_stride)) * (src_luma_height - 1);
      src_chroma_offset += ((OMX_U32) abs(src_chroma_stride)) * (src_chroma_height - 1);

      if (src_luma_stride > 0) {
        src_luma_stride *= -1;
      }

      if (src_chroma_stride > 0) {
        src_chroma_stride *= -1;
      }
    }

    /**  Pointers to use with memcpy */
    OMX_U8* src_Y_ptr = Y_input_ptr + src_luma_offset;
    OMX_U8* src_U_ptr = U_input_ptr + src_chroma_offset;
    OMX_U8*  src_V_ptr = V_input_ptr + src_chroma_offset;

    /**  Pointers to destination planes to make things easier */
    OMX_U8* Y_output_ptr = dest_ptr;
    OMX_U8* U_output_ptr = Y_output_ptr + ((OMX_U32) abs(dest_luma_stride) * dest_luma_height);
    OMX_U8* V_output_ptr = U_output_ptr + ((OMX_U32) abs(dest_chroma_stride) * dest_chroma_height);

    /**  Figure out total offsets */
    dest_luma_offset = (dest_luma_offset_y * (OMX_U32) abs(dest_luma_stride)) + dest_luma_offset_x;
    dest_chroma_offset = (dest_chroma_offset_y * (OMX_U32) abs(dest_chroma_stride)) + dest_chroma_offset_x;

    /**  If output stride is negative, reverse destination row order */
    if (dest_stride < 0) {
      dest_luma_offset += ((OMX_U32) abs(dest_luma_stride)) * (dest_luma_height - 1);
      dest_chroma_offset += ((OMX_U32) abs(dest_chroma_stride)) * (dest_chroma_height - 1);
      if (dest_luma_stride > 0) {
        dest_luma_stride *= -1;
      }
      if (dest_chroma_stride > 0) {
        dest_chroma_stride *= -1;
      }
    }

    /**  Pointers to use with memcpy */
    OMX_U8* dest_Y_ptr = Y_output_ptr + dest_luma_offset;
    OMX_U8* dest_U_ptr = U_output_ptr + dest_chroma_offset;
    OMX_U8*  dest_V_ptr = V_output_ptr + dest_chroma_offset;

    //  Y
    for (i = 0; i < luma_crop_height; ++i, src_Y_ptr += src_luma_stride, dest_Y_ptr += dest_luma_stride) {
      memcpy(dest_Y_ptr, src_Y_ptr, luma_crop_width);    //  Copy Y rows into in_buffer
    }
    //  U
    for (i = 0; i < chroma_crop_height; ++i, src_U_ptr += src_chroma_stride, dest_U_ptr += dest_chroma_stride) {
      memcpy(dest_U_ptr, src_U_ptr, chroma_crop_width);  //  Copy U rows into in_buffer
    }
    //  V
    for (i = 0; i < chroma_crop_height; ++i, src_V_ptr += src_chroma_stride, dest_V_ptr += dest_chroma_stride) {
      memcpy(dest_V_ptr, src_V_ptr, chroma_crop_width);  //  Copy V rows into in_buffer
    }
  } else {

    OMX_U32 cpy_byte_width = calcStride((OMX_U32) abs(cpy_width), colorformat);  //  Bytes width to copy
    OMX_U32 src_byte_offset_x = calcStride((OMX_U32) abs(src_offset_x), colorformat);
    OMX_U32 dest_byte_offset_x = calcStride((OMX_U32) abs(dest_offset_x), colorformat);
    OMX_U32 src_byte_offset_y = src_offset_y * (OMX_U32) abs(src_stride);
    OMX_U32 dest_byte_offset_y = dest_offset_y * (OMX_U32) abs(dest_stride);

    if (src_stride < 0)  {
      //  If input stride is negative, start from bottom
      src_byte_offset_y += cpy_height * (OMX_U32) abs(src_stride);
    }
    if (dest_stride < 0) {
      //  If output stride is negative, start from bottom
      dest_byte_offset_y += cpy_height * (OMX_U32) abs(dest_stride);
    }

    OMX_U8* src_cpy_ptr = src_ptr + src_byte_offset_y + src_byte_offset_x;
    OMX_U8* dest_cpy_ptr = dest_ptr + dest_byte_offset_y + dest_byte_offset_x;

    /** fbpxlfmt is the output (frame buffer supported) image color format
      * here fbpxlfmt is OMX_COLOR_Format32bitARGB8888 always because
      * the frame buffer has configuration of rgba 8/0 8/0 8/0 8/0 with pixel depth 8
      * if other configuration frame buffer is employed then appropriate conversion policy should be written
      */

    DEBUG(DEB_LEV_SIMPLE_SEQ, "height=%d,width=%d,dest_stride=%d\n",(int)cpy_height,(int)cpy_byte_width,(int)dest_stride);

    if(fbpxlfmt == OMX_COLOR_Format8bitRGB332 && colorformat == OMX_COLOR_Format24bitRGB888) {
      cp_byte = 3;
      for (i = 0; i < cpy_height; ++i) {
        // copy rows
        org_src_cpy_ptr = src_cpy_ptr;
        org_dst_cpy_ptr = dest_cpy_ptr;
        for(j = 0; j < cpy_byte_width; j += cp_byte) {
          //extract source rgba components
          r = *(src_cpy_ptr + 0);
          g = *(src_cpy_ptr + 1);
          b = *(src_cpy_ptr + 2);

          *(dest_cpy_ptr + 0) = b;
          *(dest_cpy_ptr + 1) = g;
          *(dest_cpy_ptr + 2) = r;
          //last byte - all 1
          *(dest_cpy_ptr + 3) = 0xff;
          src_cpy_ptr += cp_byte;
          dest_cpy_ptr += 4;
        }
        dest_cpy_ptr = org_dst_cpy_ptr + dest_stride;
        src_cpy_ptr =  org_src_cpy_ptr + src_stride;
      }
    } else if(fbpxlfmt == OMX_COLOR_Format16bitRGB565 && colorformat == OMX_COLOR_Format24bitRGB888) {
      cp_byte = 3;
      for (i = 0; i < cpy_height; ++i) {
        // copy rows
        org_src_cpy_ptr = src_cpy_ptr;
        org_dst_cpy_ptr = dest_cpy_ptr;
        for(j = 0; j < cpy_byte_width; j += cp_byte) {
          //extract source rgba components
          r = *(src_cpy_ptr + 0);
          g = *(src_cpy_ptr + 1);
          b = *(src_cpy_ptr + 2);
          *(dest_cpy_ptr + 0) = ((b>>3) & 0x1f) | ((g<<3) & 0xE0);
          *(dest_cpy_ptr + 1) = ((g>>5) & 0x07) | (r & 0xf8);
          //last byte - all 1
          src_cpy_ptr += cp_byte;
          dest_cpy_ptr += 2;
         /*
          *(dest_cpy_ptr + 0) = b;
          *(dest_cpy_ptr + 1) = g;
          *(dest_cpy_ptr + 2) = r;
          //last byte - all 1
          *(dest_cpy_ptr + 3) = 0xff;
          src_cpy_ptr += cp_byte;
          dest_cpy_ptr += 4;
        */
        }
        dest_cpy_ptr = org_dst_cpy_ptr + dest_stride;
        src_cpy_ptr =  org_src_cpy_ptr + src_stride;
      }
    } else if(fbpxlfmt == OMX_COLOR_Format24bitRGB888 && colorformat == OMX_COLOR_Format24bitRGB888) {
      cp_byte = 3;
      for (i = 0; i < cpy_height; ++i) {
        // copy rows
        org_src_cpy_ptr = src_cpy_ptr;
        org_dst_cpy_ptr = dest_cpy_ptr;
        for(j = 0; j < cpy_byte_width; j += cp_byte) {
          //extract source rgba components
          r = *(src_cpy_ptr + 0);
          g = *(src_cpy_ptr + 1);
          b = *(src_cpy_ptr + 2);
          //assign to destination
          *(dest_cpy_ptr + 0) = b;
          *(dest_cpy_ptr + 1) = g;
          *(dest_cpy_ptr + 2) = r;
          //last byte - all 1
          *(dest_cpy_ptr + 3) = 0xff;
          src_cpy_ptr += cp_byte;
          dest_cpy_ptr += 4;
        }
        dest_cpy_ptr = org_dst_cpy_ptr + dest_stride;
        src_cpy_ptr =  org_src_cpy_ptr + src_stride;
      }
    }else if(fbpxlfmt == OMX_COLOR_Format32bitARGB8888 && colorformat == OMX_COLOR_Format24bitRGB888) {
      cp_byte = 3;
      for (i = 0; i < cpy_height; ++i) {
        // copy rows
        org_src_cpy_ptr = src_cpy_ptr;
        org_dst_cpy_ptr = dest_cpy_ptr;
        for(j = 0; j < cpy_byte_width; j += cp_byte) {
          //extract source rgba components
          r = *(src_cpy_ptr + 0);
          g = *(src_cpy_ptr + 1);
          b = *(src_cpy_ptr + 2);
          //assign to detination
          *(dest_cpy_ptr + 0) = b;
          *(dest_cpy_ptr + 1) = g;
          *(dest_cpy_ptr + 2) = r;
          //last byte - all 1
          *(dest_cpy_ptr + 3) = 0xff;
          src_cpy_ptr += cp_byte;
          dest_cpy_ptr += 4;
        }
        dest_cpy_ptr = org_dst_cpy_ptr + dest_stride;
        src_cpy_ptr =  org_src_cpy_ptr + src_stride;
      }
    } else if(fbpxlfmt == OMX_COLOR_Format32bitARGB8888 && colorformat == OMX_COLOR_Format24bitBGR888) {
      cp_byte = 3;
      for (i = 0; i < cpy_height; ++i) {
        // copy rows
        org_src_cpy_ptr = src_cpy_ptr;
        org_dst_cpy_ptr = dest_cpy_ptr;
        for(j = 0; j < cpy_byte_width; j += cp_byte) {
          //extract source rgba components
          b = *(src_cpy_ptr + 0);
          g = *(src_cpy_ptr + 1);
          r = *(src_cpy_ptr + 2);
          //assign to detination
          *(dest_cpy_ptr + 0) = b;
          *(dest_cpy_ptr + 1) = g;
          *(dest_cpy_ptr + 2) = r;
          //last byte - all 1
          *(dest_cpy_ptr + 3) = 0xff;
          src_cpy_ptr += cp_byte;
          dest_cpy_ptr += 4;
        }
        dest_cpy_ptr = org_dst_cpy_ptr + dest_stride;
        src_cpy_ptr =  org_src_cpy_ptr + src_stride;
      }
    } else if(fbpxlfmt == OMX_COLOR_Format32bitARGB8888 && (colorformat == OMX_COLOR_Format32bitBGRA8888 || colorformat == OMX_COLOR_Format32bitARGB8888)) {
      for (i = 0; i < cpy_height; ++i, src_cpy_ptr += src_stride, dest_cpy_ptr += dest_stride ) {
        // same color format - so no extraction - only simple memcpy
        memcpy(dest_cpy_ptr, src_cpy_ptr, cpy_byte_width);  //  Copy rows
      }
    } else if(fbpxlfmt == OMX_COLOR_Format32bitARGB8888 && colorformat == OMX_COLOR_Format16bitARGB1555) {
      cp_byte = 2;
      for (i = 0; i < cpy_height; ++i) {
        // copy rows
        org_src_cpy_ptr = src_cpy_ptr;
        org_dst_cpy_ptr = dest_cpy_ptr;
        for(j = 0; j < cpy_byte_width; j += cp_byte) {
          // individual argb components are less than 1 byte
          OMX_U16 temp_old, temp, *temp1;
          temp1=(OMX_U16*)src_cpy_ptr;
          temp = *temp1;
          temp_old = temp;
          a = (OMX_U8) ((temp >> 15) && 0x0001); //getting the 1 bit of a and setting all other bits to 0
          temp = temp_old;
          r = (OMX_U8) ((temp >> 10) & 0x001f); //getting 5 bits of r and setting all other bits to 0
          temp = temp_old;
          g = (OMX_U8) ((temp >> 5) & 0x001f); //getting 5 bits of g and setting all other bits to 0
          temp = temp_old;
          b = (OMX_U8) (temp & 0x001f); //getting 5 bits of b and setting all other bits to 0
          temp = temp_old;
          // assign them in perfect order
          *(dest_cpy_ptr + 0) = b<<3;
          *(dest_cpy_ptr + 1) = g<<3;
          *(dest_cpy_ptr + 2) = r<<3;
          *(dest_cpy_ptr + 3) = a<<7;
          src_cpy_ptr += cp_byte;
          dest_cpy_ptr += 4;
        }
        dest_cpy_ptr = org_dst_cpy_ptr + dest_stride;
        src_cpy_ptr =  org_src_cpy_ptr + src_stride;
      }
    } else if(fbpxlfmt == OMX_COLOR_Format32bitARGB8888 && (colorformat == OMX_COLOR_Format16bitRGB565 || OMX_COLOR_Format16bitBGR565)) {
      cp_byte = 2;
      for (i = 0; i < cpy_height; ++i) {
        // copy rows
        org_src_cpy_ptr = src_cpy_ptr;
        org_dst_cpy_ptr = dest_cpy_ptr;
        for(j = 0; j < cpy_byte_width; j += cp_byte) {
          // individual rgb components are less than 1 byte
          OMX_U16 temp_old, temp,*temp1;
          temp1=(OMX_U16*)src_cpy_ptr;
          temp = *temp1;
          temp_old = temp;
          r = (OMX_U8) ((temp >> 11) & 0x001f); //getting 5 bits of r and setting all other bits to 0
          temp = temp_old;
          g = (OMX_U8) ((temp >> 5) & 0x003f); //getting 6 bits of g and setting all other bits to 0
          temp = temp_old;
          b = (OMX_U8) (temp & 0x001f); //getting 5 bits of b and setting all other bits to 0
          temp = temp_old;
          // assign them in perfect order
          *(dest_cpy_ptr + 0) = b<<3;
          *(dest_cpy_ptr + 1) = g<<2;
          *(dest_cpy_ptr + 2) = r<<3;
          // last byte  - all 1
          *(dest_cpy_ptr + 3) = 0xff;
          src_cpy_ptr += cp_byte;
          dest_cpy_ptr += 4;
        }
        dest_cpy_ptr = org_dst_cpy_ptr + dest_stride;
        src_cpy_ptr =  org_src_cpy_ptr + src_stride;
      }
    }
    else if(fbpxlfmt == OMX_COLOR_Format16bitARGB1555 && colorformat == OMX_COLOR_Format24bitRGB888)
    {
      cp_byte = 3;
      for (i = 0; i < cpy_height; ++i)
      {
        // copy rows
        org_src_cpy_ptr = src_cpy_ptr;
        org_dst_cpy_ptr = dest_cpy_ptr;
        for(j = 0; j < cpy_byte_width; j += cp_byte)
        {
          //extract source rgba components
          r = *(src_cpy_ptr + 0);
          g = *(src_cpy_ptr + 1);
          b = *(src_cpy_ptr + 2);

          *(dest_cpy_ptr + 0) = ((b>>3) & 0x1f) | ((g & 0x38)<<2);
          *(dest_cpy_ptr + 1) = ((g>>6) & 0x03) | ((r>>1) & 0x7c);

          src_cpy_ptr += cp_byte;
          dest_cpy_ptr += 2;
        }
        dest_cpy_ptr = org_dst_cpy_ptr + dest_stride;
        src_cpy_ptr =  org_src_cpy_ptr + src_stride;
      }
    } else {
      DEBUG(DEB_LEV_ERR, "the frame buffer pixel format %d and colorformat %d NOT supported\n",fbpxlfmt,colorformat);
      DEBUG(DEB_LEV_ERR, "or the input rgb format is not supported\n");
    }
  }
}

/** @brief the entry point for sending buffers to the drmdev sink port
 *
 * This function can be called by the EmptyThisBuffer or FillThisBuffer. It depends on
 * the nature of the port, that can be an input or output port.
 */
OMX_ERRORTYPE omx_drmdev_sink_component_port_SendBufferFunction(omx_base_PortType *openmaxStandPort, OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE                   err;
  OMX_U32                         portIndex;
  OMX_COMPONENTTYPE*              omxComponent = openmaxStandPort->standCompContainer;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
  int errQue;
#if NO_GST_OMX_PATCH
  unsigned int i;
#endif

  portIndex = (openmaxStandPort->sPortParam.eDir == OMX_DirInput)?pBuffer->nInputPortIndex:pBuffer->nOutputPortIndex;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s portIndex %lu\n", __func__, portIndex);

  if (portIndex != openmaxStandPort->sPortParam.nPortIndex) {
    DEBUG(DEB_LEV_ERR, "In %s: wrong port for this operation portIndex=%d port->portIndex=%d\n",
           __func__, (int)portIndex, (int)openmaxStandPort->sPortParam.nPortIndex);
    return OMX_ErrorBadPortIndex;
  }

  if(omx_base_component_Private->state == OMX_StateInvalid) {
    DEBUG(DEB_LEV_ERR, "In %s: we are in OMX_StateInvalid\n", __func__);
    return OMX_ErrorInvalidState;
  }

  if(omx_base_component_Private->state != OMX_StateExecuting &&
    omx_base_component_Private->state != OMX_StatePause &&
    omx_base_component_Private->state != OMX_StateIdle) {
    DEBUG(DEB_LEV_ERR, "In %s: we are not in executing/paused/idle state, but in %d\n", __func__, omx_base_component_Private->state);
    return OMX_ErrorIncorrectStateOperation;
  }
  if (!PORT_IS_ENABLED(openmaxStandPort) || (PORT_IS_BEING_DISABLED(openmaxStandPort) && !PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort)) ||
      (omx_base_component_Private->transientState == OMX_TransStateExecutingToIdle &&
      (PORT_IS_TUNNELED(openmaxStandPort) && !PORT_IS_BUFFER_SUPPLIER(openmaxStandPort)))) {
    DEBUG(DEB_LEV_ERR, "In %s: Port %d is disabled comp = %s \n", __func__, (int)portIndex,omx_base_component_Private->name);
    return OMX_ErrorIncorrectStateOperation;
  }

  /* Temporarily disable this check for gst-openmax */
#if NO_GST_OMX_PATCH
  {
  OMX_BOOL foundBuffer = OMX_FALSE;
  if(pBuffer!=NULL && pBuffer->pBuffer!=NULL) {
    for(i=0; i < openmaxStandPort->sPortParam.nBufferCountActual; i++){
    if (pBuffer->pBuffer == openmaxStandPort->pInternalBufferStorage[i]->pBuffer) {
      foundBuffer = OMX_TRUE;
      break;
    }
    }
  }
  if (!foundBuffer) {
    return OMX_ErrorBadParameter;
  }
  }
#endif

  if ((err = checkHeader(pBuffer, sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone) {
    DEBUG(DEB_LEV_ERR, "In %s: received wrong buffer header on input port\n", __func__);
    return err;
  }

  /* And notify the buffer management thread we have a fresh new buffer to manage */
  if(!PORT_IS_BEING_FLUSHED(openmaxStandPort) && !(PORT_IS_BEING_DISABLED(openmaxStandPort) && PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort))){
      errQue = queue(openmaxStandPort->pBufferQueue, pBuffer);
      if (errQue) {
	  return OMX_ErrorInsufficientResources;
      }
      tsem_up(openmaxStandPort->pBufferSem);
      DEBUG(DEB_LEV_FULL_SEQ, "In %s Signalling bMgmtSem Port Index=%d\n",__func__, (int)portIndex);
      tsem_up(omx_base_component_Private->bMgmtSem);
  }else if(PORT_IS_BUFFER_SUPPLIER(openmaxStandPort)){
      DEBUG(DEB_LEV_FULL_SEQ, "In %s: Comp %s received io:%d buffer\n", __func__,omx_base_component_Private->name,(int)openmaxStandPort->sPortParam.nPortIndex);
      errQue = queue(openmaxStandPort->pBufferQueue, pBuffer);
      if (errQue) {
	  return OMX_ErrorInsufficientResources;
      }
      tsem_up(openmaxStandPort->pBufferSem);
  } else { // If port being flushed and not tunneled then return error
    DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
    return OMX_ErrorIncorrectStateOperation;
  }
  return OMX_ErrorNone;
}

/** This is the central function for component processing. It
  * is executed in a separate thread, is synchronized with
  * semaphores at each port, those are released each time a new buffer
  * is available on the given port.
  */
void* omx_drmdev_sink_BufferMgmtFunction (void* param) {
  OMX_COMPONENTTYPE* openmaxStandComp = (OMX_COMPONENTTYPE*)param;
  omx_base_component_PrivateType* omx_base_component_Private  = (omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  omx_drmdev_sink_component_PrivateType*      omx_drm_sink_Private       = (omx_drmdev_sink_component_PrivateType*)omx_base_component_Private;
  omx_base_PortType               *pInPort                    = (omx_base_PortType *)omx_drm_sink_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
  tsem_t*                         pInputSem                   = pInPort->pBufferSem;
  queue_t*                        pInputQueue                 = pInPort->pBufferQueue;
  OMX_BUFFERHEADERTYPE*           pInputBuffer                = NULL;
  OMX_BUFFERHEADERTYPE*           bufferToReturn			  = NULL;
  OMX_COMPONENTTYPE*              target_component;
  OMX_BOOL                        isInputBufferNeeded         = OMX_TRUE;
  int                             inBufExchanged              = 0;
  static long                     old_time = 0, new_time = 0;
  long                            timediff = 0;

  omx_drm_sink_Private->bellagioThreads->nThreadBufferMngtID = (long int)syscall(__NR_gettid);
  DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s the thread ID is %i\n", __func__, (int)omx_drm_sink_Private->bellagioThreads->nThreadBufferMngtID);

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s \n", __func__);
  while(omx_base_component_Private->state == OMX_StateIdle || omx_base_component_Private->state == OMX_StateExecuting ||  omx_base_component_Private->state == OMX_StatePause ||
    omx_base_component_Private->transientState == OMX_TransStateLoadedToIdle){

    /*Wait till the ports are being flushed*/
    pthread_mutex_lock(&omx_drm_sink_Private->flush_mutex);
    while( PORT_IS_BEING_FLUSHED(pInPort)) {
      pthread_mutex_unlock(&omx_drm_sink_Private->flush_mutex);

      if(isInputBufferNeeded==OMX_FALSE) {
        pInPort->ReturnBufferFunction(pInPort,pInputBuffer);
        inBufExchanged--;
        pInputBuffer=NULL;
        isInputBufferNeeded=OMX_TRUE;
        DEBUG(DEB_LEV_FULL_SEQ, "Ports are flushing,so returning input buffer\n");
      }
      DEBUG(DEB_LEV_FULL_SEQ, "In %s signalling flush all condition \n", __func__);

      tsem_up(omx_drm_sink_Private->flush_all_condition);
      tsem_down(omx_drm_sink_Private->flush_condition);
      pthread_mutex_lock(&omx_drm_sink_Private->flush_mutex);
    }
    pthread_mutex_unlock(&omx_drm_sink_Private->flush_mutex);

    /*No buffer to process. So wait here*/
    if((pInputSem->semval==0 && isInputBufferNeeded==OMX_TRUE ) &&
      (omx_drm_sink_Private->state != OMX_StateLoaded && omx_drm_sink_Private->state != OMX_StateInvalid)) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for input buffer \n");
      tsem_down(omx_drm_sink_Private->bMgmtSem);
    }

    if(omx_drm_sink_Private->state == OMX_StateLoaded || omx_drm_sink_Private->state == OMX_StateInvalid) {
      DEBUG(DEB_LEV_FULL_SEQ, "In %s Buffer Management Thread is exiting\n",__func__);
      break;
    }

    DEBUG(DEB_LEV_FULL_SEQ, "Waiting for input buffer semval=%d in %s\n",pInputSem->semval, __func__);
    if(pInputSem->semval>0 && isInputBufferNeeded==OMX_TRUE ) {
      tsem_down(pInputSem);
      if(pInputQueue->nelem>0){
        inBufExchanged++;
        isInputBufferNeeded=OMX_FALSE;
        pInputBuffer = dequeue(pInputQueue);
        if(pInputBuffer == NULL){
          DEBUG(DEB_LEV_ERR, "Had NULL input buffer!!\n");
          break;
        }
      }
    }

    if(isInputBufferNeeded==OMX_FALSE) {
	if((pInputBuffer->nFlags & OMX_BUFFERFLAG_EOS) ==OMX_BUFFERFLAG_EOS) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "Detected EOS flags in input buffer\n");

		(*(omx_base_component_Private->callbacks->EventHandler))
		  (openmaxStandComp,
		  omx_base_component_Private->callbackData,
		  OMX_EventBufferFlag, /* The command was completed */
		  0, /* The commands was a OMX_CommandStateSet */
		  pInputBuffer->nFlags, /* The state has been changed in message->messageParam2 */
		  NULL);
		pInputBuffer->nFlags=0;
      }

      target_component=(OMX_COMPONENTTYPE*)pInputBuffer->hMarkTargetComponent;
      if(target_component==(OMX_COMPONENTTYPE *)openmaxStandComp) {
        /*Clear the mark and generate an event*/
        (*(omx_base_component_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_base_component_Private->callbackData,
          OMX_EventMark, /* The command was completed */
          1, /* The commands was a OMX_CommandStateSet */
          0, /* The state has been changed in message->messageParam2 */
          pInputBuffer->pMarkData);
      } else if(pInputBuffer->hMarkTargetComponent!=NULL){
        /*If this is not the target component then pass the mark*/
        DEBUG(DEB_LEV_FULL_SEQ, "Can't Pass Mark. This is a Sink!!\n");
      }

      if((omx_drm_sink_Private->state == OMX_StateExecuting) || (omx_drm_sink_Private->state == OMX_StateIdle)) {
	  int newFbNumber = 0;
	  int i;
		  struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
		  fd_set fds;
		  int ret;
		  drmEventContext evctx;

          if (omx_drm_sink_Private->tunneledOutput) {
			  DEBUG(DEB_LEV_FULL_SEQ, "Got the buffer: %p (drm)\n", pInputBuffer);
			      if (omx_drm_sink_Private->lastDrmBuffer != pInputBuffer) {
					  bufferToReturn = omx_drm_sink_Private->lastDrmBuffer;
					  omx_drm_sink_Private->lastDrmBuffer = pInputBuffer;
					  for (i = 0; i < pInPort->sPortParam.nBufferCountActual; i++) {
						  if ( pInputBuffer->pBuffer == (void*)((unsigned int)omx_drm_sink_Private->drm_mmap[i].mapped)) {
							  newFbNumber = i;
							  break;
						  }
					  }
			      }
				  isInputBufferNeeded = OMX_TRUE;
			  } else {

				  omx_drm_sink_Private->drm_current_buf = (omx_drm_sink_Private->drm_current_buf + 1) % omx_drm_sink_Private->drm_num_of_bufs;

				  newFbNumber = omx_drm_sink_Private->drm_current_buf;

				  memcpy((void*)((unsigned int)omx_drm_sink_Private->drm_mmap[newFbNumber].mapped),
						  pInputBuffer->pBuffer, pInputBuffer->nAllocLen);

				  isInputBufferNeeded = OMX_TRUE;
				  bufferToReturn = pInputBuffer;
			  }

			  drmModePageFlip(
					  omx_drm_sink_Private->drm_fd,
					  omx_drm_sink_Private->drm_crtc[newFbNumber],
					  omx_drm_sink_Private->drm_fb[newFbNumber],
					  DRM_MODE_PAGE_FLIP_EVENT,0
					  );

			  memset(&evctx, 0, sizeof evctx);
			  evctx.version = DRM_EVENT_CONTEXT_VERSION;
			  evctx.vblank_handler = NULL;
			  evctx.page_flip_handler = page_flip_handler;

			  FD_ZERO(&fds);
			  FD_SET(0, &fds);
			  FD_SET(omx_drm_sink_Private->drm_fd, &fds);
			  ret = select(omx_drm_sink_Private->drm_fd + 1, &fds, NULL, NULL, &timeout);

			  if (ret <= 0) {
					  fprintf(stderr, "-------->select timed out or error (ret %d)\n",
							  ret);
			  } else if (FD_ISSET(0, &fds)) {
					  printf("------>Break the select loop\n");
			  }

			  drmHandleEvent(omx_drm_sink_Private->drm_fd, &evctx);

			  new_time = GetTime();
			  if(old_time == 0) {
			    old_time = new_time;
			  } else {
			    timediff = nFrameProcessTime - ((new_time - old_time) * 1000);
			    if(timediff>0) {
			      usleep(timediff);
			    }
			    old_time = GetTime();
			  }
      } else {
        DEBUG(DEB_LEV_ERR, "In %s Received Buffer in non-Executing State(%s) TrState (%s)\n",
          __func__, stateName(omx_drm_sink_Private->state),
          transientStateName(omx_base_component_Private->transientState));
        if(OMX_TransStateExecutingToIdle == omx_base_component_Private->transientState ||
           OMX_TransStatePauseToIdle == omx_base_component_Private->transientState) {
          pInputBuffer->nFilledLen = 0;
          bufferToReturn = pInputBuffer;
        }
      }
      /*Input Buffer has been completely consumed. So, get new input buffer*/

      if(omx_drm_sink_Private->state==OMX_StatePause && !PORT_IS_BEING_FLUSHED(pInPort)) {
        /*Waiting at paused state*/
        tsem_wait(omx_drm_sink_Private->bStateSem);
      }

      /*Input Buffer has been completely consumed. So, return input buffer*/
      if (bufferToReturn != NULL) {
	bufferToReturn->nFilledLen = 0;
        pInPort->ReturnBufferFunction(pInPort, bufferToReturn);
        inBufExchanged--;
        bufferToReturn = NULL;
        isInputBufferNeeded = OMX_TRUE;
      }
    }
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ,"Exiting Buffer Management Thread\n");
  return NULL;
}

OMX_ERRORTYPE omx_drmdev_sink_component_SetConfig(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nIndex,
  OMX_PTR pComponentConfigStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_U32 portIndex;
  OMX_CONFIG_RECTTYPE *omxConfigCrop;
  OMX_CONFIG_ROTATIONTYPE *omxConfigRotate;
  OMX_CONFIG_MIRRORTYPE *omxConfigMirror;
  OMX_CONFIG_SCALEFACTORTYPE *omxConfigScale;
  OMX_CONFIG_POINTTYPE *omxConfigOutputPosition;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_drmdev_sink_component_PortType *pPort;
  if (pComponentConfigStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nIndex);

  switch (nIndex) {
    case OMX_IndexConfigCommonInputCrop:
      omxConfigCrop = (OMX_CONFIG_RECTTYPE*)pComponentConfigStructure;
      portIndex = omxConfigCrop->nPortIndex;
      if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_RECTTYPE))) != OMX_ErrorNone) {
        break;
      }
      if (portIndex == OMX_BASE_SINK_INPUTPORT_INDEX) {
        pPort = (omx_drmdev_sink_component_PortType *) omx_drmdev_sink_component_Private->ports[portIndex];
        pPort->omxConfigCrop.nLeft = omxConfigCrop->nLeft;
        pPort->omxConfigCrop.nTop = omxConfigCrop->nTop;
        pPort->omxConfigCrop.nWidth = omxConfigCrop->nWidth;
        pPort->omxConfigCrop.nHeight = omxConfigCrop->nHeight;
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    case OMX_IndexConfigCommonRotate:
      omxConfigRotate = (OMX_CONFIG_ROTATIONTYPE*)pComponentConfigStructure;
      portIndex = omxConfigRotate->nPortIndex;
      if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_ROTATIONTYPE))) != OMX_ErrorNone) {
        break;
      }
      if (portIndex == 0) {
        pPort = (omx_drmdev_sink_component_PortType *) omx_drmdev_sink_component_Private->ports[portIndex];
        if (omxConfigRotate->nRotation != 0) {
          //  Rotation not supported (yet)
          return OMX_ErrorUnsupportedSetting;
        }
        pPort->omxConfigRotate.nRotation = omxConfigRotate->nRotation;
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    case OMX_IndexConfigCommonMirror:
      omxConfigMirror = (OMX_CONFIG_MIRRORTYPE*)pComponentConfigStructure;
      portIndex = omxConfigMirror->nPortIndex;
      if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_MIRRORTYPE))) != OMX_ErrorNone) {
        break;
      }
      if (portIndex == 0) {
        if (omxConfigMirror->eMirror == OMX_MirrorBoth || omxConfigMirror->eMirror == OMX_MirrorHorizontal)  {
          //  Horizontal mirroring not yet supported
          return OMX_ErrorUnsupportedSetting;
        }
        pPort = (omx_drmdev_sink_component_PortType *) omx_drmdev_sink_component_Private->ports[portIndex];
        pPort->omxConfigMirror.eMirror = omxConfigMirror->eMirror;
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    case OMX_IndexConfigCommonScale:
      omxConfigScale = (OMX_CONFIG_SCALEFACTORTYPE*)pComponentConfigStructure;
      portIndex = omxConfigScale->nPortIndex;
      if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_SCALEFACTORTYPE))) != OMX_ErrorNone) {
        break;
      }
      if (portIndex == 0) {
        if (omxConfigScale->xWidth != 0x10000 || omxConfigScale->xHeight != 0x10000)  {
          //  Scaling not yet supported
          return OMX_ErrorUnsupportedSetting;
        }
        pPort = (omx_drmdev_sink_component_PortType *) omx_drmdev_sink_component_Private->ports[portIndex];
        pPort->omxConfigScale.xWidth = omxConfigScale->xWidth;
        pPort->omxConfigScale.xHeight = omxConfigScale->xHeight;
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    case OMX_IndexConfigCommonOutputPosition:
      omxConfigOutputPosition = (OMX_CONFIG_POINTTYPE*)pComponentConfigStructure;
      portIndex = omxConfigOutputPosition->nPortIndex;
      if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_POINTTYPE))) != OMX_ErrorNone) {
        break;
      }
      if (portIndex == 0) {
        pPort = (omx_drmdev_sink_component_PortType *) omx_drmdev_sink_component_Private->ports[portIndex];
        pPort->omxConfigOutputPosition.nX = omxConfigOutputPosition->nX;
        pPort->omxConfigOutputPosition.nY = omxConfigOutputPosition->nY;
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    default: // delegate to superclass
      return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_drmdev_sink_component_GetConfig(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nIndex,
  OMX_PTR pComponentConfigStructure) {

  OMX_CONFIG_RECTTYPE *omxConfigCrop;
  OMX_CONFIG_ROTATIONTYPE *omxConfigRotate;
  OMX_CONFIG_MIRRORTYPE *omxConfigMirror;
  OMX_CONFIG_SCALEFACTORTYPE *omxConfigScale;
  OMX_CONFIG_POINTTYPE *omxConfigOutputPosition;

  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_drmdev_sink_component_PortType *pPort;
  if (pComponentConfigStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting configuration %i\n", nIndex);
  /* Check which structure we are being fed and fill its header */
  switch (nIndex) {
    case OMX_IndexConfigCommonInputCrop:
      omxConfigCrop = (OMX_CONFIG_RECTTYPE*)pComponentConfigStructure;
      setHeader(omxConfigCrop, sizeof(OMX_CONFIG_RECTTYPE));
      if (omxConfigCrop->nPortIndex == 0) {
        pPort = (omx_drmdev_sink_component_PortType *)omx_drmdev_sink_component_Private->ports[omxConfigCrop->nPortIndex];
        memcpy(omxConfigCrop, &pPort->omxConfigCrop, sizeof(OMX_CONFIG_RECTTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
    break;
    case OMX_IndexConfigCommonRotate:
      omxConfigRotate = (OMX_CONFIG_ROTATIONTYPE*)pComponentConfigStructure;
      setHeader(omxConfigRotate, sizeof(OMX_CONFIG_ROTATIONTYPE));
      if (omxConfigRotate->nPortIndex == 0) {
        pPort = (omx_drmdev_sink_component_PortType *)omx_drmdev_sink_component_Private->ports[omxConfigRotate->nPortIndex];
        memcpy(omxConfigRotate, &pPort->omxConfigRotate, sizeof(OMX_CONFIG_ROTATIONTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    case OMX_IndexConfigCommonMirror:
      omxConfigMirror = (OMX_CONFIG_MIRRORTYPE*)pComponentConfigStructure;
      setHeader(omxConfigMirror, sizeof(OMX_CONFIG_MIRRORTYPE));
      if (omxConfigMirror->nPortIndex == 0) {
        pPort = (omx_drmdev_sink_component_PortType *)omx_drmdev_sink_component_Private->ports[omxConfigMirror->nPortIndex];
        memcpy(omxConfigMirror, &pPort->omxConfigMirror, sizeof(OMX_CONFIG_MIRRORTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    case OMX_IndexConfigCommonScale:
      omxConfigScale = (OMX_CONFIG_SCALEFACTORTYPE*)pComponentConfigStructure;
      setHeader(omxConfigScale, sizeof(OMX_CONFIG_SCALEFACTORTYPE));
      if (omxConfigScale->nPortIndex == 0) {
        pPort = (omx_drmdev_sink_component_PortType *)omx_drmdev_sink_component_Private->ports[omxConfigScale->nPortIndex];
        memcpy(omxConfigScale, &pPort->omxConfigScale, sizeof(OMX_CONFIG_SCALEFACTORTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    case OMX_IndexConfigCommonOutputPosition:
      omxConfigOutputPosition = (OMX_CONFIG_POINTTYPE*)pComponentConfigStructure;
      setHeader(omxConfigOutputPosition, sizeof(OMX_CONFIG_POINTTYPE));
      if (omxConfigOutputPosition->nPortIndex == 0) {
        pPort = (omx_drmdev_sink_component_PortType *)omx_drmdev_sink_component_Private->ports[omxConfigOutputPosition->nPortIndex];
        memcpy(omxConfigOutputPosition, &pPort->omxConfigOutputPosition, sizeof(OMX_CONFIG_POINTTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    default: // delegate to superclass
      return omx_base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_drmdev_sink_component_SetParameter(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nParamIndex,
  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
  OMX_PARAM_COMPONENTROLETYPE *pComponentRole;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_drmdev_sink_component_PortType *pPort;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  switch(nParamIndex) {
     case OMX_IndexParamPortDefinition:
      pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE*) ComponentParameterStructure;
      portIndex = pPortDef->nPortIndex;
      err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err);
        break;
      }

      if(portIndex > (omx_drmdev_sink_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts +
                      omx_drmdev_sink_component_Private->sPortTypesParam[OMX_PortDomainOther].nPorts)) {
        return OMX_ErrorBadPortIndex;
      }

      if(portIndex == 0) {

        pPort = (omx_drmdev_sink_component_PortType *) omx_drmdev_sink_component_Private->ports[portIndex];

        pPort->sPortParam.nBufferCountActual = pPortDef->nBufferCountActual;
        //  Copy stuff from OMX_VIDEO_PORTDEFINITIONTYPE structure
        if(pPortDef->format.video.cMIMEType != NULL) {
          strcpy(pPort->sPortParam.format.video.cMIMEType , pPortDef->format.video.cMIMEType);
        }
        pPort->sPortParam.format.video.nFrameWidth = pPortDef->format.video.nFrameWidth;
        pPort->sPortParam.format.video.nFrameHeight = pPortDef->format.video.nFrameHeight;
        pPort->sPortParam.format.video.nBitrate = pPortDef->format.video.nBitrate;
        pPort->sPortParam.format.video.xFramerate = pPortDef->format.video.xFramerate;
        if(pPortDef->format.video.xFramerate > 0) {
                nFrameProcessTime = 1000000 / pPortDef->format.video.xFramerate;
              }
        pPort->sPortParam.format.video.bFlagErrorConcealment = pPortDef->format.video.bFlagErrorConcealment;

        //  Figure out stride, slice height, min buffer size
        pPort->sPortParam.format.video.nStride = calcStride(pPort->sPortParam.format.video.nFrameWidth, pPort->sVideoParam.eColorFormat);
        pPort->sPortParam.format.video.nSliceHeight = pPort->sPortParam.format.video.nFrameHeight;  //  No support for slices yet
        // Read-only field by spec

        pPort->sPortParam.nBufferSize = (OMX_U32) abs(pPort->sPortParam.format.video.nStride) * pPort->sPortParam.format.video.nSliceHeight;
        pPort->omxConfigCrop.nWidth = pPort->sPortParam.format.video.nFrameWidth;
        pPort->omxConfigCrop.nHeight = pPort->sPortParam.format.video.nFrameHeight;
      }
      break;

    case OMX_IndexParamVideoPortFormat:
      //  FIXME: How do we handle the nIndex member?
      pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
      portIndex = pVideoPortFormat->nPortIndex;
      err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err);
        break;
      }
      pPort = (omx_drmdev_sink_component_PortType *) omx_drmdev_sink_component_Private->ports[portIndex];
      if(portIndex != 0) {
        return OMX_ErrorBadPortIndex;
      }
      if (pVideoPortFormat->eCompressionFormat != OMX_VIDEO_CodingUnused)  {
        //  No compression allowed
        return OMX_ErrorUnsupportedSetting;
      }

      if(pVideoPortFormat->xFramerate > 0) {
        nFrameProcessTime = 1000000 / pVideoPortFormat->xFramerate;
      }
      pPort->sVideoParam.xFramerate = pVideoPortFormat->xFramerate;
      pPort->sVideoParam.eCompressionFormat = pVideoPortFormat->eCompressionFormat;
      pPort->sVideoParam.eColorFormat = pVideoPortFormat->eColorFormat;
      //  Figure out stride, slice height, min buffer size
      pPort->sPortParam.format.video.nStride = calcStride(pPort->sPortParam.format.video.nFrameWidth, pPort->sVideoParam.eColorFormat);
      pPort->sPortParam.format.video.nSliceHeight = pPort->sPortParam.format.video.nFrameHeight;  //  No support for slices yet

      pPort->sPortParam.nBufferSize = (OMX_U32) abs(pPort->sPortParam.format.video.nStride) * pPort->sPortParam.format.video.nSliceHeight;
      break;
    case OMX_IndexParamStandardComponentRole:
      pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;

      if (omx_drmdev_sink_component_Private->state != OMX_StateLoaded && omx_drmdev_sink_component_Private->state != OMX_StateWaitForResources) {
        DEBUG(DEB_LEV_ERR, "In %s Incorrect State=%x lineno=%d\n",__func__,omx_drmdev_sink_component_Private->state,__LINE__);
        return OMX_ErrorIncorrectStateOperation;
      }

      if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) {
        break;
      }

      if (strcmp( (char*) pComponentRole->cRole, DRMDEV_SINK_COMP_ROLE)) {
        return OMX_ErrorBadParameter;
      }
      break;
    default: /*Call the base component function*/
      return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}


OMX_ERRORTYPE omx_drmdev_sink_component_GetParameter(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nParamIndex,
  OMX_PTR ComponentParameterStructure) {

  OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
  OMX_PARAM_COMPONENTROLETYPE *pComponentRole;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_COMPONENTTYPE *openmaxStandComp = (OMX_COMPONENTTYPE *)hComponent;
  omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private = openmaxStandComp->pComponentPrivate;
  omx_drmdev_sink_component_PortType *pPort = (omx_drmdev_sink_component_PortType *) omx_drmdev_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
    case OMX_IndexParamVideoInit:
      if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) {
        break;
      }
      memcpy(ComponentParameterStructure, &omx_drmdev_sink_component_Private->sPortTypesParam[OMX_PortDomainVideo], sizeof(OMX_PORT_PARAM_TYPE));
      break;
    case OMX_IndexParamOtherInit:
      if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) {
        break;
      }
      memcpy(ComponentParameterStructure, &omx_drmdev_sink_component_Private->sPortTypesParam[OMX_PortDomainOther], sizeof(OMX_PORT_PARAM_TYPE));
      break;
    case OMX_IndexParamVideoPortFormat:
      pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
      if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) {
        break;
      }
      if (pVideoPortFormat->nPortIndex < 1) {
        memcpy(pVideoPortFormat, &pPort->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
      } else {
        return OMX_ErrorBadPortIndex;
      }
      break;
    case OMX_IndexParamStandardComponentRole:
      pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;
      if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) {
        break;
      }
      strcpy( (char*) pComponentRole->cRole, DRMDEV_SINK_COMP_ROLE);
      break;
    default: /*Call the base component function*/
      return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return err;
}


OMX_ERRORTYPE omx_drmdev_sink_alloc_drm_buffers(OMX_COMPONENTTYPE* openmaxStandComp)
{
	omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private = (omx_drmdev_sink_component_PrivateType*)openmaxStandComp->pComponentPrivate;
	omx_base_PortType *openmaxStandPort = (omx_base_PortType *) omx_drmdev_sink_component_Private->ports[OMX_BASE_SINK_INPUTPORT_INDEX];
	int i;
	int ret;
	int nBufferSize;


	nBufferSize = openmaxStandPort->sPortParam.nBufferSize;

	for (i = 0; i < DRM_DEFAULT_NUMBER_OF_BUFFERS; i++) {
		DEBUG(DEB_LEV_FULL_SEQ, "Size of buffer to alloc: %d\n", nBufferSize);

		omx_drmdev_sink_component_Private->drm_gem[i].size = nBufferSize;

		ret = exynos_gem_create(omx_drmdev_sink_component_Private->drm_fd,
				&omx_drmdev_sink_component_Private->drm_gem[i]);

		if (ret < 0) {
			DEBUG(DEB_LEV_ERR, "Failed to allocate GEM buffer\n");
			return OMX_ErrorInsufficientResources;
		}

		omx_drmdev_sink_component_Private->drm_mmap[i].handle = omx_drmdev_sink_component_Private->drm_gem[i].handle;
		omx_drmdev_sink_component_Private->drm_mmap[i].size = omx_drmdev_sink_component_Private->drm_gem[i].size;

	  ret = exynos_gem_mmap(omx_drmdev_sink_component_Private->drm_fd,
								  &omx_drmdev_sink_component_Private->drm_mmap[i]);

		if (ret < 0) {
			struct drm_gem_close args;
			args.handle = omx_drmdev_sink_component_Private->drm_gem[i].handle;
			exynos_gem_close(omx_drmdev_sink_component_Private->drm_fd, &args);
			DEBUG(DEB_LEV_ERR, "Failed to mmap GEM buffer\n");
			return OMX_ErrorInsufficientResources;
		}
	}

	omx_drmdev_sink_component_Private->drm_num_of_bufs = DRM_DEFAULT_NUMBER_OF_BUFFERS;
	omx_drmdev_sink_component_Private->drm_current_buf = 0;

	return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_drmdev_sink_free_drm_buffers(OMX_COMPONENTTYPE* openmaxStandComp)
{
		omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private = (omx_drmdev_sink_component_PrivateType*)openmaxStandComp->pComponentPrivate;
		struct drm_gem_close args;
		int i;

		for (i = 0; i < omx_drmdev_sink_component_Private->drm_num_of_bufs; i++) {
					munmap((void*)&omx_drmdev_sink_component_Private->drm_mmap[i],
							omx_drmdev_sink_component_Private->drm_mmap[i].size);

				args.handle = omx_drmdev_sink_component_Private->drm_gem[i].handle;
				exynos_gem_close(omx_drmdev_sink_component_Private->drm_fd, &args);
		}

		return OMX_ErrorNone;
}


OMX_ERRORTYPE omx_drmdev_sink_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message) {

  omx_drmdev_sink_component_PrivateType* omx_drmdev_sink_component_Private = (omx_drmdev_sink_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err;
  OMX_STATETYPE eState;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  eState = omx_drmdev_sink_component_Private->state; //storing current state

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateExecuting ) && (omx_drmdev_sink_component_Private->state == OMX_StateIdle)) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s sink component from loaded to idle \n", __func__);

      if (!omx_drmdev_sink_component_Private->tunneledOutput) {
	  DEBUG(DEB_LEV_FULL_SEQ, "Not using tunneled communication - have to allocate buffers.");
	  omx_drmdev_sink_alloc_drm_buffers(openmaxStandComp);
      } else {
	  DEBUG(DEB_LEV_FULL_SEQ, "Using tunneled communication.");
      }

      err = omx_drmdev_sink_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Video Sink Init Failed Error=%x\n",__func__,err);
        return err;
      }
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s sink component from loaded to idle SUCCESS\n", __func__);
    }

    if ((message->messageParam == OMX_StateIdle ) && (omx_drmdev_sink_component_Private->state == OMX_StateLoaded)) {
    err = omx_drmdev_sink_component_open(openmaxStandComp);
    if (err != OMX_ErrorNone) {
	DEBUG(DEB_LEV_ERR, "In %s: failed to set state (loaded->idle)\n", __func__);
	omx_drmdev_sink_component_Private->state = OMX_StateInvalid;
	return err;
      }
    }
  }

  // Execute the base message handling
  err = omx_base_component_MessageHandler(openmaxStandComp,message);

  if (message->messageType == OMX_CommandStateSet){
		if ((message->messageParam == OMX_StateIdle ) && (omx_drmdev_sink_component_Private->state == OMX_StateLoaded)) {
			err = omx_drmdev_sink_component_open(openmaxStandComp);
			if (err != OMX_ErrorNone) {
				DEBUG(DEB_LEV_ERR, "In %s: failed to set state (loaded->idle)\n", __func__);
				omx_drmdev_sink_component_Private->state = OMX_StateInvalid;
				return err;
			}
		}

		if ((message->messageParam == OMX_StateIdle ) && (omx_drmdev_sink_component_Private->state == OMX_StateIdle) && eState == OMX_StateExecuting) {
			if (!omx_drmdev_sink_component_Private->tunneledOutput)
				omx_drmdev_sink_free_drm_buffers(openmaxStandComp);
			err = omx_drmdev_sink_component_Deinit(openmaxStandComp);
			if(err!=OMX_ErrorNone) {
				DEBUG(DEB_LEV_ERR, "In %s Video Sink Deinit Failed Error=%x\n",__func__,err);
				return err;
			}
		}
  }
  return err;
}

OMX_ERRORTYPE drm_input_port_AllocateTunnelBuffer(
		omx_base_PortType *openmaxStandPort,
		OMX_U32 nPortIndex)
{
  unsigned int i;
  OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
  omx_drmdev_sink_component_PrivateType* omx_base_component_Private = (omx_drmdev_sink_component_PrivateType*)omxComponent->pComponentPrivate;
  OMX_U8* pBuffer=NULL;
  OMX_ERRORTYPE eError=OMX_ErrorNone,err;
  int errQue;
  OMX_U32 numRetry=0,nBufferSize;
  OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
  OMX_U32 nLocalBufferCountActual;
  int ret;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for port %p\n", __func__, openmaxStandPort);

  if (nPortIndex != openmaxStandPort->sPortParam.nPortIndex) {
    DEBUG(DEB_LEV_ERR, "In %s: Bad Port Index\n", __func__);
    return OMX_ErrorBadPortIndex;
  }
  if (! PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort)) {
    DEBUG(DEB_LEV_ERR, "In %s: Port is not tunneled Flag=%x\n", __func__, (int)openmaxStandPort->nTunnelFlags);
    return OMX_ErrorBadPortIndex;
  }

  if (omx_base_component_Private->transientState != OMX_TransStateLoadedToIdle) {
    if (!openmaxStandPort->bIsTransientToEnabled) {
      DEBUG(DEB_LEV_ERR, "In %s: The port is not allowed to receive buffers\n", __func__);
      return OMX_ErrorIncorrectStateTransition;
    }
  }
  /*Get nBufferSize of the peer port and allocate which one is bigger*/
  nBufferSize = openmaxStandPort->sPortParam.nBufferSize;
  setHeader(&sPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  sPortDef.nPortIndex = openmaxStandPort->nTunneledPort;
  err = OMX_GetParameter(openmaxStandPort->hTunneledComponent, OMX_IndexParamPortDefinition, &sPortDef);
  if(err == OMX_ErrorNone) {
	  nBufferSize = (sPortDef.nBufferSize > openmaxStandPort->sPortParam.nBufferSize) ? sPortDef.nBufferSize: openmaxStandPort->sPortParam.nBufferSize;
  } else {
	  return OMX_ErrorPortsNotCompatible;
  }
  /* set the number of buffer needed getting the max nBufferCountActual of the two components
   * On the one with the minor nBufferCountActual a setParam should be called to normalize the value,
   * if possible.
   */
  nLocalBufferCountActual = openmaxStandPort->sPortParam.nBufferCountActual;

  DEBUG(DEB_LEV_FULL_SEQ, "%s:%d: nLocalBufferCountActual =  %d\n", __func__, __LINE__, (int)nLocalBufferCountActual);


  if (nLocalBufferCountActual < sPortDef.nBufferCountActual) {
	  nLocalBufferCountActual = sPortDef.nBufferCountActual;
	  openmaxStandPort->sPortParam.nBufferCountActual = nLocalBufferCountActual;
	  DEBUG(DEB_LEV_FULL_SEQ, "%s:%d: nLocalBufferCountActual =  %d\n", __func__, __LINE__, (int)nLocalBufferCountActual);
  } else if (sPortDef.nBufferCountActual < nLocalBufferCountActual){
	  sPortDef.nBufferCountActual = nLocalBufferCountActual;
	  DEBUG(DEB_LEV_FULL_SEQ, "%s:%d: nLocalBufferCountActual =  %d\n", __func__, __LINE__, (int)nLocalBufferCountActual);
	  err = OMX_SetParameter(openmaxStandPort->hTunneledComponent, OMX_IndexParamPortDefinition, &sPortDef);
	  if(err != OMX_ErrorNone) {
		  /* for some reasons undetected during negotiation the tunnel cannot be established.
		   */
		  return OMX_ErrorPortsNotCompatible;
	  }
  }
  DEBUG(DEB_LEV_ERR, "%s:%d: nLocalBufferCountActual =  %d\n", __func__, __LINE__, (int)nLocalBufferCountActual);
  if (openmaxStandPort->sPortParam.nBufferCountActual == 0) {
      openmaxStandPort->sPortParam.bPopulated = OMX_TRUE;
      openmaxStandPort->bIsFullOfBuffers = OMX_TRUE;
      DEBUG(DEB_LEV_FULL_SEQ, "In %s Allocated nothing\n",__func__);
      return OMX_ErrorNone;
  }
  DEBUG(DEB_LEV_FULL_SEQ, "%s:%d: nLocalBufferCountActual =  %d\n", __func__, __LINE__, (int)nLocalBufferCountActual);
  for(i=0; i < openmaxStandPort->sPortParam.nBufferCountActual; i++){
	DEBUG(DEB_LEV_FULL_SEQ, "The loop: i=%d state=%d\n", i, openmaxStandPort->bBufferStateAllocated[i]);

    if (openmaxStandPort->bBufferStateAllocated[i] == BUFFER_FREE) {
	  DEBUG(DEB_LEV_FULL_SEQ, "Size of buffer to alloc: %d\n", (unsigned int)nBufferSize);
	  omx_base_component_Private->drm_gem[i].size = nBufferSize;
	  ret = exynos_gem_create(omx_base_component_Private->drm_fd,
			&omx_base_component_Private->drm_gem[i]);

	  if (ret < 0) {
		  DEBUG(DEB_LEV_ERR, "Failed to allocate GEM buffer\n");
		  return OMX_ErrorInsufficientResources;
	  }
	  omx_base_component_Private->drm_mmap[i].handle = omx_base_component_Private->drm_gem[i].handle;
	  omx_base_component_Private->drm_mmap[i].size = omx_base_component_Private->drm_gem[i].size;

	  ret = exynos_gem_mmap(omx_base_component_Private->drm_fd,
							  &omx_base_component_Private->drm_mmap[i]);

	  if (ret < 0) {
		  struct drm_gem_close args;
		  args.handle = omx_base_component_Private->drm_gem[i].handle;
		  exynos_gem_close(omx_base_component_Private->drm_fd, &args);
		  DEBUG(DEB_LEV_ERR, "Failed to mmap GEM buffer\n");
		  return OMX_ErrorInsufficientResources;
	  }

	  pBuffer = (void*)((unsigned int)omx_base_component_Private->drm_mmap[i].mapped);

      if(pBuffer == NULL) {
        return OMX_ErrorInsufficientResources;
      }
      /*Retry more than once, if the tunneled component is not in Loaded->Idle State*/
      while(numRetry <TUNNEL_USE_BUFFER_RETRY) {
        eError=OMX_UseBuffer(openmaxStandPort->hTunneledComponent,&openmaxStandPort->pInternalBufferStorage[i],
                             openmaxStandPort->nTunneledPort,NULL,nBufferSize,pBuffer);
        if(eError!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_FULL_SEQ,"Tunneled Component Couldn't Use buffer %i From Comp=%s Retry=%d\n",
          i,omx_base_component_Private->name,(int)numRetry);

          if((eError ==  OMX_ErrorIncorrectStateTransition) && numRetry<TUNNEL_USE_BUFFER_RETRY) {
            DEBUG(DEB_LEV_FULL_SEQ,"Waiting for next try %i \n",(int)numRetry);
            usleep(TUNNEL_USE_BUFFER_RETRY_USLEEP_TIME);
            numRetry++;
            continue;
          }
          free(pBuffer);
          pBuffer = NULL;
          return eError;
        }
        else {
		if(openmaxStandPort->sPortParam.eDir == OMX_DirInput) {
			openmaxStandPort->pInternalBufferStorage[i]->nInputPortIndex  = openmaxStandPort->sPortParam.nPortIndex;
			openmaxStandPort->pInternalBufferStorage[i]->nOutputPortIndex = openmaxStandPort->nTunneledPort;
		} else {
			openmaxStandPort->pInternalBufferStorage[i]->nInputPortIndex  = openmaxStandPort->nTunneledPort;
			openmaxStandPort->pInternalBufferStorage[i]->nOutputPortIndex = openmaxStandPort->sPortParam.nPortIndex;
		}
		break;
        }
      }
      if(eError!=OMX_ErrorNone) {
        free(pBuffer);
        pBuffer = NULL;
        DEBUG(DEB_LEV_ERR,"In %s Tunneled Component Couldn't Use Buffer err = %x \n",__func__,(int)eError);
        return eError;
      }
      openmaxStandPort->bBufferStateAllocated[i] = BUFFER_ALLOCATED;
      openmaxStandPort->nNumAssignedBuffers++;
      DEBUG(DEB_LEV_PARAMS, "openmaxStandPort->nNumAssignedBuffers %i\n", (int)openmaxStandPort->nNumAssignedBuffers);

      if (openmaxStandPort->sPortParam.nBufferCountActual == openmaxStandPort->nNumAssignedBuffers) {
        openmaxStandPort->sPortParam.bPopulated = OMX_TRUE;
        openmaxStandPort->bIsFullOfBuffers = OMX_TRUE;
        DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s nPortIndex=%d\n",__func__, (int)nPortIndex);
      }
      errQue = queue(openmaxStandPort->pBufferQueue, openmaxStandPort->pInternalBufferStorage[i]);
      if (errQue) {
	  /* TODO the queue is full. This can be handled in a fine way with
	   * some retries, or other checking. For the moment this is a critical error
	   * and simply causes the failure of this call
	   */
	  return OMX_ErrorInsufficientResources;
      }
    }
  }

  omx_base_component_Private->tunneledOutput = OMX_TRUE;

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for port %p. Allocated all the buffers\n", __func__, openmaxStandPort);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE drm_input_port_FreeTunnelBuffer(omx_base_PortType *openmaxStandPort,OMX_U32 nPortIndex)
{
  unsigned int i;
  OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
  omx_drmdev_sink_component_PrivateType* omx_base_component_Private = (omx_drmdev_sink_component_PrivateType*)omxComponent->pComponentPrivate;
  OMX_ERRORTYPE eError=OMX_ErrorNone;
  OMX_U32 numRetry=0;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for port %p\n", __func__, openmaxStandPort);

  if (nPortIndex != openmaxStandPort->sPortParam.nPortIndex) {
    DEBUG(DEB_LEV_ERR, "In %s: Bad Port Index\n", __func__);
    return OMX_ErrorBadPortIndex;
  }
  if (! PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort)) {
    DEBUG(DEB_LEV_ERR, "In %s: Port is not tunneled\n", __func__);
    return OMX_ErrorBadPortIndex;
  }

  if (omx_base_component_Private->transientState != OMX_TransStateIdleToLoaded) {
    if (!openmaxStandPort->bIsTransientToDisabled) {
      DEBUG(DEB_LEV_FULL_SEQ, "In %s: The port is not allowed to free the buffers\n", __func__);
      (*(omx_base_component_Private->callbacks->EventHandler))
        (omxComponent,
        omx_base_component_Private->callbackData,
        OMX_EventError, /* The command was completed */
        OMX_ErrorPortUnpopulated, /* The commands was a OMX_CommandStateSet */
        nPortIndex, /* The state has been changed in message->messageParam2 */
        NULL);
    }
  }

  for(i=0; i < openmaxStandPort->sPortParam.nBufferCountActual; i++){
    if (openmaxStandPort->bBufferStateAllocated[i] & (BUFFER_ASSIGNED | BUFFER_ALLOCATED)) {

      openmaxStandPort->bIsFullOfBuffers = OMX_FALSE;
      if (openmaxStandPort->bBufferStateAllocated[i] & BUFFER_ALLOCATED) {
	struct drm_gem_close args;
	//free(openmaxStandPort->pInternalBufferStorage[i]->pBuffer);

	munmap((void*)&omx_base_component_Private->drm_mmap[i],
			omx_base_component_Private->drm_mmap[i].size);

	args.handle = omx_base_component_Private->drm_gem[i].handle;
	exynos_gem_close(omx_base_component_Private->drm_fd, &args);

        openmaxStandPort->pInternalBufferStorage[i]->pBuffer = NULL;
      }
      /*Retry more than once, if the tunneled component is not in Idle->Loaded State*/
      while(numRetry <TUNNEL_USE_BUFFER_RETRY) {
        eError=OMX_FreeBuffer(openmaxStandPort->hTunneledComponent,openmaxStandPort->nTunneledPort,openmaxStandPort->pInternalBufferStorage[i]);
        if(eError!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR,"Tunneled Component Couldn't free buffer %i \n",i);
          if((eError ==  OMX_ErrorIncorrectStateTransition) && numRetry<TUNNEL_USE_BUFFER_RETRY) {
            DEBUG(DEB_LEV_ERR,"Waiting for next try %i \n",(int)numRetry);
            usleep(TUNNEL_USE_BUFFER_RETRY_USLEEP_TIME);
            numRetry++;
            continue;
          }
          return eError;
        } else {
          break;
        }
      }
      openmaxStandPort->bBufferStateAllocated[i] = BUFFER_FREE;

      openmaxStandPort->nNumAssignedBuffers--;
      DEBUG(DEB_LEV_PARAMS, "openmaxStandPort->nNumAssignedBuffers %i\n", (int)openmaxStandPort->nNumAssignedBuffers);

      if (openmaxStandPort->nNumAssignedBuffers == 0) {
        openmaxStandPort->sPortParam.bPopulated = OMX_FALSE;
        openmaxStandPort->bIsEmptyOfBuffers = OMX_TRUE;
      }
    }
  }
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for port %p Qelem=%d BSem=%d\n", __func__, openmaxStandPort,
							  openmaxStandPort->pBufferQueue->nelem, openmaxStandPort->pBufferSem->semval);
  return OMX_ErrorNone;
}

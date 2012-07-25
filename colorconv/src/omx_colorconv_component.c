/**
  @file src/components/colorconv/omx_colorconv_component.c

  This component implements a color converter using the FFmpeg
  software library.

  Copyright (C) 2011 Samsung Electronics Co., Ltd.

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
*/

#include <bellagio/omxcore.h>
#include "omx_colorconv_component.h"
#include "../samsung-proprietary.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#define VIDEO_DEV_NAME	"/dev/video"

#define OMX_COLORCONV_DEF_PORT_REQ_BUFFERS 2

/** Maximum Number of Video Color Converter Component Instance */
#define MAX_COMPONENT_VIDEOCOLORCONV 2

/** Counter of Video Component Instance*/
static OMX_U32 noVideoColorConvInstance = 0;

#define DEFAULT_WIDTH  480
#define DEFAULT_HEIGHT 480

/** define the max input buffer size */
#define DEFAULT_VIDEO_INPUT_BUF_SIZE (DEFAULT_WIDTH * DEFAULT_HEIGHT * 3/2)

struct fimc_pixel_format {
	OMX_U32 fourcc;
	OMX_COLOR_FORMATTYPE omx_pixfmt;
};

#define CLEAR(x) memset(&x, 0, sizeof(x))
#define ALIGN(x, y) (((x) + ((y) - 1)) / (y) * (y))

static const struct fimc_pixel_format fimc_pixel_formats[] = {
	{ V4L2_PIX_FMT_RGB565X, OMX_COLOR_Format16bitRGB565 },
	{ V4L2_PIX_FMT_BGR666, OMX_COLOR_Format18bitRGB666 },
	{ V4L2_PIX_FMT_RGB32, OMX_COLOR_Format32bitARGB8888 },
	{ V4L2_PIX_FMT_YUYV, OMX_COLOR_FormatYCbYCr },
	{ V4L2_PIX_FMT_UYVY, OMX_COLOR_FormatCbYCrY },
	{ V4L2_PIX_FMT_VYUY, OMX_COLOR_FormatCrYCbY },
	{ V4L2_PIX_FMT_YVYU, OMX_COLOR_FormatYCrYCb },
	{ V4L2_PIX_FMT_YUV420, OMX_COLOR_FormatYUV420Planar },
	/*{ V4L2_PIX_FMT_NV16, */
	/*{ V4L2_PIX_FMT_NV61, */
	/*{ V4L2_PIX_FMT_YUV420, */
	/*{ V4L2_PIX_FMT_NV12, */
	/*{ V4L2_PIX_FMT_NV12M, */
	/*{ V4L2_PIX_FMT_YUV420M, */
	/*{ V4L2_PIX_FMT_NV12MT, */
	{ 0, 0 }, /* sentinel */
};

/** Figure out equivalent pixel format (fourcc) based on OMX_COLOR_FORMATTYPE
  * @param omx_pixfmt is the input OpenMAX standard pixel format
  * output is the fourcc (Four Character Code) corresponding to this input pixel format
  */
OMX_U32 omx_pixfmt_to_fourcc(OMX_COLOR_FORMATTYPE omx_pixfmt)
{
	int i;

	for (i = 0; fimc_pixel_formats[i].fourcc; i++) {
		if (fimc_pixel_formats[i].omx_pixfmt == omx_pixfmt)
			return fimc_pixel_formats[i].fourcc;
	}

	DEBUG(DEB_LEV_ERR, "%s: Fourcc for OMX color format %d not found\n",
	      __func__, (unsigned int) omx_pixfmt);

	return 0;
}

OMX_COLOR_FORMATTYPE fourcc_to_omx_pixfmt(OMX_U32 fourcc)
{
	int i;

	for (i = 0; fimc_pixel_formats[i].omx_pixfmt; i++) {
		if (fimc_pixel_formats[i].fourcc == fourcc)
			return fimc_pixel_formats[i].omx_pixfmt;
	}

	DEBUG(DEB_LEV_ERR, "%s: OMX color format for FOURCC 0x%x not found\n",
	      __func__, (unsigned int)fourcc);

	return 0;
}

/**
  * @param width is the input picture width
  * @param omx_pxlfmt is the input openmax standard pixel format
  *
  * This fucntions calculates the byte per pixel needed to display the picture
  * with the input omx_pxlfmt. The output stride for display is thus product
  * of input width and byte per pixel.
  */
OMX_S32 calcStride(OMX_U32 width, OMX_COLOR_FORMATTYPE omx_pxlfmt)
{
	OMX_U32 stride;
	OMX_U32 bpp; /* bit per pixel */

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
	stride = (width * bpp) >> 3;
	return (OMX_S32) stride;
}

/** The Constructor
  * @param omxStandComp the component handle to be constructed
  * @param cComponentName is the name of the constructed component
  */
OMX_ERRORTYPE omx_v4l_colorconv_Constructor(OMX_COMPONENTTYPE *omxStandComp, OMX_STRING cComponentName)
{
	omx_v4l_colorconv_PrivateType *colorconv_Priv;
	omx_v4l_colorconv_PortType *inPort, *outPort;
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_U32 i;

	if (!omxStandComp->pComponentPrivate) {
		DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n", __func__);
		omxStandComp->pComponentPrivate = calloc(1, sizeof(omx_v4l_colorconv_PrivateType));
		if (omxStandComp->pComponentPrivate == NULL) {
			return OMX_ErrorInsufficientResources;
		}
	} else {
		DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n",
		      __func__, (int)omxStandComp->pComponentPrivate);
	}

	DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d  %p\n", __func__, __LINE__, omxStandComp->pComponentPrivate);

	colorconv_Priv = omxStandComp->pComponentPrivate;
	colorconv_Priv->ports = NULL;
	colorconv_Priv->vid_fd = -1;

	DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d  %d %p\n", __func__, __LINE__,
	      (int)colorconv_Priv->sPortTypesParam[OMX_PortDomainVideo].nPorts, colorconv_Priv->ports);

	/* we could create our own port structures here */

	err = omx_base_filter_Constructor(omxStandComp, cComponentName);

	DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d  %p\n", __func__, __LINE__, omxStandComp->pComponentPrivate);
	DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d  %d %p\n", __func__, __LINE__,
	      (int)colorconv_Priv->sPortTypesParam[OMX_PortDomainVideo].nPorts, colorconv_Priv->ports);

	colorconv_Priv->sPortTypesParam[OMX_PortDomainVideo].nStartPortNumber = 0;
	colorconv_Priv->sPortTypesParam[OMX_PortDomainVideo].nPorts = 2;

	DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d  %d %p\n", __func__, __LINE__,
	      (int)colorconv_Priv->sPortTypesParam[OMX_PortDomainVideo].nPorts, colorconv_Priv->ports);

	/** Allocate Ports and call port constructor. */
	if (colorconv_Priv->sPortTypesParam[OMX_PortDomainVideo].nPorts && !colorconv_Priv->ports) {

		DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d\n", __func__, __LINE__);

		colorconv_Priv->ports = calloc(colorconv_Priv->sPortTypesParam[OMX_PortDomainVideo].nPorts,
					       sizeof(omx_base_PortType *));
		if (!colorconv_Priv->ports) {
			return OMX_ErrorInsufficientResources;
		}
		for (i=0; i < colorconv_Priv->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
			colorconv_Priv->ports[i] = calloc(1, sizeof(omx_v4l_colorconv_PortType));
			if (!colorconv_Priv->ports[i]) {
				return OMX_ErrorInsufficientResources;
			}
		}
	}

	DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d\n", __func__, __LINE__);

	base_video_port_Constructor(omxStandComp, &colorconv_Priv->ports[0], 0, OMX_TRUE);
	base_video_port_Constructor(omxStandComp, &colorconv_Priv->ports[1], 1, OMX_FALSE);

	inPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	outPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];

	inPort->nBuffersRequested = OMX_COLORCONV_DEF_PORT_REQ_BUFFERS;
	outPort->nBuffersRequested = OMX_COLORCONV_DEF_PORT_REQ_BUFFERS;

	/** Domain specific section for the ports. */

	inPort->sVideoParam.eColorFormat = OMX_COLOR_FormatYCrYCb;
	outPort->sVideoParam.eColorFormat = OMX_COLOR_Format16bitRGB565;

	/* input port parameter settings */
	inPort->sPortParam.format.video.nFrameWidth = DEFAULT_WIDTH;
	inPort->sPortParam.format.video.nFrameHeight = DEFAULT_HEIGHT;
	inPort->sPortParam.format.video.nStride = calcStride(inPort->sPortParam.format.video.nFrameWidth,
							     inPort->sVideoParam.eColorFormat);
	/* No support for slices yet */
	inPort->sPortParam.format.video.nSliceHeight = inPort->sPortParam.format.video.nFrameHeight;

	inPort->sPortParam.format.video.xFramerate = 25; /* How to calculate this ??? */

	inPort->sPortParam.nBufferSize = DEFAULT_VIDEO_INPUT_BUF_SIZE;
	inPort->sPortParam.format.video.eColorFormat = inPort->sVideoParam.eColorFormat;
	inPort->sPortParam.format.video.pNativeWindow = NULL;

	/* output port parameter settings */
	outPort->sPortParam.format.video.nFrameWidth = DEFAULT_WIDTH;
	outPort->sPortParam.format.video.nFrameHeight = DEFAULT_HEIGHT;
	outPort->sPortParam.format.video.nStride = calcStride(outPort->sPortParam.format.video.nFrameWidth,
							      outPort->sVideoParam.eColorFormat);
	/* No support for slices yet */
	outPort->sPortParam.format.video.nSliceHeight = outPort->sPortParam.format.video.nFrameHeight;
	outPort->sPortParam.format.video.xFramerate = 25;
	outPort->sPortParam.format.video.eColorFormat = inPort->sVideoParam.eColorFormat;

	outPort->sPortParam.nBufferSize = DEFAULT_VIDEO_INPUT_BUF_SIZE * 2;

	setHeader(&inPort->omxConfigCrop, sizeof(OMX_CONFIG_RECTTYPE));
	inPort->omxConfigCrop.nPortIndex = OMX_BASE_FILTER_INPUTPORT_INDEX;
	inPort->omxConfigCrop.nLeft = inPort->omxConfigCrop.nTop = 0;
	inPort->omxConfigCrop.nWidth = DEFAULT_WIDTH;
	inPort->omxConfigCrop.nHeight = DEFAULT_HEIGHT;

	setHeader(&inPort->omxConfigRotate, sizeof(OMX_CONFIG_ROTATIONTYPE));
	inPort->omxConfigRotate.nPortIndex = OMX_BASE_FILTER_INPUTPORT_INDEX;
	inPort->omxConfigRotate.nRotation = 0;

	setHeader(&inPort->omxConfigMirror, sizeof(OMX_CONFIG_MIRRORTYPE));
	inPort->omxConfigMirror.nPortIndex = OMX_BASE_FILTER_INPUTPORT_INDEX;
	inPort->omxConfigMirror.eMirror = OMX_MirrorNone;

	setHeader(&inPort->omxConfigScale, sizeof(OMX_CONFIG_SCALEFACTORTYPE));
	inPort->omxConfigScale.nPortIndex = OMX_BASE_FILTER_INPUTPORT_INDEX;
	inPort->omxConfigScale.xWidth = inPort->omxConfigScale.xHeight = 0x10000;

	setHeader(&inPort->omxConfigOutputPosition, sizeof(OMX_CONFIG_POINTTYPE));
	inPort->omxConfigOutputPosition.nPortIndex = OMX_BASE_FILTER_INPUTPORT_INDEX;
	inPort->omxConfigOutputPosition.nX = inPort->omxConfigOutputPosition.nY = 0;

	setHeader(&outPort->omxConfigCrop, sizeof(OMX_CONFIG_RECTTYPE));
	outPort->omxConfigCrop.nPortIndex = OMX_BASE_FILTER_OUTPUTPORT_INDEX;
	outPort->omxConfigCrop.nLeft = outPort->omxConfigCrop.nTop = 0;
	outPort->omxConfigCrop.nWidth = DEFAULT_WIDTH;
	outPort->omxConfigCrop.nHeight = DEFAULT_HEIGHT;

	setHeader(&outPort->omxConfigRotate, sizeof(OMX_CONFIG_ROTATIONTYPE));
	outPort->omxConfigRotate.nPortIndex = OMX_BASE_FILTER_OUTPUTPORT_INDEX;
	outPort->omxConfigRotate.nRotation = 0;

	setHeader(&outPort->omxConfigMirror, sizeof(OMX_CONFIG_MIRRORTYPE));
	outPort->omxConfigMirror.nPortIndex = OMX_BASE_FILTER_OUTPUTPORT_INDEX;
	outPort->omxConfigMirror.eMirror = OMX_MirrorNone;

	setHeader(&outPort->omxConfigScale, sizeof(OMX_CONFIG_SCALEFACTORTYPE));
	outPort->omxConfigScale.nPortIndex = OMX_BASE_FILTER_OUTPUTPORT_INDEX;
	outPort->omxConfigScale.xWidth = outPort->omxConfigScale.xHeight = 0x10000;

	setHeader(&outPort->omxConfigOutputPosition, sizeof(OMX_CONFIG_POINTTYPE));
	outPort->omxConfigOutputPosition.nPortIndex = OMX_BASE_FILTER_OUTPUTPORT_INDEX;
	outPort->omxConfigOutputPosition.nX = outPort->omxConfigOutputPosition.nY = 0;

	colorconv_Priv->messageHandler = omx_v4l_colorconv_MessageHandler;
	colorconv_Priv->destructor = omx_v4l_colorconv_Destructor;
	colorconv_Priv->BufferMgmtCallback = omx_v4l_colorconv_BufferMgmtCallback;
	colorconv_Priv->samsungProprietaryCommunication = OMX_FALSE;

	omxStandComp->SetParameter = omx_v4l_colorconv_SetParameter;
	omxStandComp->GetParameter = omx_v4l_colorconv_GetParameter;
	omxStandComp->SetConfig = omx_v4l_colorconv_SetConfig;
	omxStandComp->GetConfig = omx_v4l_colorconv_GetConfig;

	inPort->ComponentTunnelRequest = fimc_input_port_ComponentTunnelRequest;

#if 1
	omxStandComp->UseBuffer = omx_v4l_colorconv_UseBuffer;
	inPort->Port_UseBuffer = &base_port_UseBuffer;
	outPort->Port_UseBuffer = &base_port_UseBuffer;
#endif

	omxStandComp->UseEGLImage = omx_video_colorconv_UseEGLImage;
	noVideoColorConvInstance++;

	if (noVideoColorConvInstance > MAX_COMPONENT_VIDEOCOLORCONV) {
		return OMX_ErrorInsufficientResources;
	}

	DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s\n", __func__);

	return err;
}

/** The destructor
 */
OMX_ERRORTYPE omx_v4l_colorconv_Destructor(OMX_COMPONENTTYPE *omxStandComp)
{
	omx_v4l_colorconv_PrivateType* colorconv_Priv = omxStandComp->pComponentPrivate;
	OMX_U32 i;

	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s:%d\n", __func__, __LINE__);

	/* frees port/s */
	if (colorconv_Priv->ports) {
		for (i=0; i < colorconv_Priv->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
			if (colorconv_Priv->ports[i])
				colorconv_Priv->ports[i]->PortDestructor(colorconv_Priv->ports[i]);
		}
		free(colorconv_Priv->ports);
		colorconv_Priv->ports=NULL;
	}

	omx_base_filter_Destructor(omxStandComp);
	noVideoColorConvInstance--;

	return OMX_ErrorNone;
}

static OMX_ERRORTYPE omx_v4l_colorconv_verify_capability(int fd)
{
	struct v4l2_capability cap;

	memset(&cap, 0, sizeof(cap));

	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
		return OMX_ErrorHardware;

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
		DEBUG(DEB_LEV_ERR, "%s: Device does not support capture\n", __func__);
		return OMX_ErrorHardware;
	}
	if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE)) {
		DEBUG(DEB_LEV_ERR, "%s: Device does not support output\n", __func__);
		return OMX_ErrorHardware;
	}
	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		DEBUG(DEB_LEV_ERR, "%s: Device does not support streaming\n", __func__);
		return OMX_ErrorHardware;
	}

	DEBUG(DEB_LEV_FULL_SEQ, "%s: Driver name: %s\n", __func__, cap.driver);

	return OMX_ErrorNone;
}

/*
 * src: type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
 * dst: type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
 */
static OMX_ERRORTYPE v4l2_request_buffers(int fd, int *num_bufs, int type)
{
	struct v4l2_requestbuffers reqbuf;

	CLEAR(reqbuf);
	reqbuf.count = *num_bufs;
	reqbuf.memory = V4L2_MEMORY_USERPTR;
	reqbuf.type = type;

	if (ioctl(fd, VIDIOC_REQBUFS, &reqbuf) == -1) {
		if (EINVAL == errno) {
			DEBUG(DEB_LEV_ERR, "%s: USERPTR memory is not supported\n",
			      __func__);
		} else {
			DEBUG(DEB_LEV_ERR, "%s: VIDIOC_REQBUFS error %d, %s\n",
			      __func__, errno, strerror(errno));
		}
		return OMX_ErrorHardware;
	}

	*num_bufs = reqbuf.count;

	return OMX_ErrorNone;
}


static OMX_ERRORTYPE omx_v4l_colorconv_set_pix_format(
	 OMX_COMPONENTTYPE *omxStandComp,
	 OMX_COLOR_FORMATTYPE omx_pixfmt,
	 OMX_U32 *width, OMX_U32 *height,
	 OMX_BOOL bSource)
{
	omx_v4l_colorconv_PrivateType *pPriv = omxStandComp->pComponentPrivate;
	unsigned int fourcc = omx_pixfmt_to_fourcc(omx_pixfmt);
	struct v4l2_format v4l2_fmt;
	int i;

	if (width == NULL || height == NULL || fourcc == 0) {
		return OMX_ErrorBadParameter;
	}

	CLEAR(v4l2_fmt);

	if (bSource)
		v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else
		v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	v4l2_fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
	if (bSource && pPriv->samsungProprietaryCommunication) {
		v4l2_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12MT;
		v4l2_fmt.fmt.pix_mp.width = ALIGN(*width, 128);
		v4l2_fmt.fmt.pix_mp.height = ALIGN(*height, 32);
	} else {
		v4l2_fmt.fmt.pix_mp.pixelformat = fourcc;
		v4l2_fmt.fmt.pix_mp.width = *width;
		v4l2_fmt.fmt.pix_mp.height = *height;
	}

	if (ioctl(pPriv->vid_fd, VIDIOC_S_FMT, &v4l2_fmt) == -1) {
		perror("VIDIOC_S_FMT IOCTL: ");
		return OMX_ErrorHardware;
	}

#if DEBUG_LEVEL > 0
	fourcc = v4l2_fmt.fmt.pix_mp.pixelformat;
	DEBUG(DEB_LEV_FUNCTION_NAME,"format: %c%c%c%c, number of planes: %d\n",
	      (char)((fourcc >> 0) & 0xFF),
	      (char)((fourcc >> 8) & 0xFF),
	      (char)((fourcc >> 16) & 0xFF),
	      (char)((fourcc >> 24) & 0xFF),
	      v4l2_fmt.fmt.pix_mp.num_planes);

	for (i = 0; i < v4l2_fmt.fmt.pix_mp.num_planes; i++) {
		DEBUG(DEB_LEV_FUNCTION_NAME,
		      "plane[%d]: bytesperline: %d, sizeimage: %d\n",
		      i,
		      v4l2_fmt.fmt.pix_mp.plane_fmt[i].bytesperline,
		      v4l2_fmt.fmt.pix_mp.plane_fmt[i].sizeimage);
	}
	DEBUG(DEB_LEV_FUNCTION_NAME, "src_width= %d, src_height= %d\n",
	      v4l2_fmt.fmt.pix_mp.width, v4l2_fmt.fmt.pix_mp.height);
#endif
	*width	= v4l2_fmt.fmt.pix_mp.width;
	*height	= v4l2_fmt.fmt.pix_mp.height;

	return OMX_ErrorNone;
}


static OMX_ERRORTYPE omx_v4l_colorconv_set_crop(
	 OMX_COMPONENTTYPE *omxStandComp,
	 omx_v4l_colorconv_PortType *pPort,
	 OMX_BOOL bSource)
{
	omx_v4l_colorconv_PrivateType *pPriv = omxStandComp->pComponentPrivate;
	struct v4l2_crop crop;

	memset (&crop, 0, sizeof (crop));

	if (bSource)
		crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	crop.c.left = pPort->omxConfigCrop.nLeft;
	crop.c.top = pPort->omxConfigCrop.nTop;
	crop.c.width = pPort->omxConfigCrop.nWidth;
	crop.c.height = pPort->omxConfigCrop.nHeight;

	if (ioctl(pPriv->vid_fd, VIDIOC_S_CROP, &crop) == -1) {
		perror("VIDIOC_S_CROPT IOCTL: ");
		return OMX_ErrorHardware;
	}

	return OMX_ErrorNone;
}

/** The Initialization function
  * This function allocates the frames and buffers to store the color
  * converterted output from input yuv ???
  */
OMX_ERRORTYPE omx_v4l_colorconv_Init(OMX_COMPONENTTYPE *omxStandComp)
{
	omx_v4l_colorconv_PrivateType* colorconv_Priv = omxStandComp->pComponentPrivate;
	omx_v4l_colorconv_PortType *inPort,*outPort;
	OMX_ERRORTYPE err = OMX_ErrorNone;
	int vid_node = 4;

	DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d\n", __func__, __LINE__);

	if (SRM_GetFreeVideoM2MDevName(colorconv_Priv->devnode_name, 0) != OMX_ErrorNone) {
		snprintf(colorconv_Priv->devnode_name, sizeof(colorconv_Priv->devnode_name),
			 "%s%u", VIDEO_DEV_NAME, vid_node);
	}

	DEBUG(DEB_LEV_FULL_SEQ, "Opening %s (M2M)\n", colorconv_Priv->devnode_name);

	colorconv_Priv->vid_fd = open(colorconv_Priv->devnode_name, O_RDWR | O_NONBLOCK, 0);
	if (colorconv_Priv->vid_fd < 0)
		return OMX_ErrorHardware;

	DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d\n", __func__, __LINE__);

	if (omx_v4l_colorconv_verify_capability(colorconv_Priv->vid_fd) != 0) {
		DEBUG(DEB_LEV_ERR, "\n%s capabilities verification failed!\n", colorconv_Priv->devnode_name);
		return OMX_ErrorHardware;
	}

	DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d\n", __func__, __LINE__);

	inPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	outPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];

	DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d\n", __func__, __LINE__);

	DEBUG(DEB_LEV_FULL_SEQ, "Setting dimensions %s:%d to %dx%d\n", __func__, __LINE__,
	      inPort->sPortParam.format.video.nFrameWidth,
	      inPort->sPortParam.format.video.nFrameHeight);

	err = omx_v4l_colorconv_set_pix_format(omxStandComp,
					       inPort->sVideoParam.eColorFormat,
					       &inPort->sPortParam.format.video.nFrameWidth,
					       &inPort->sPortParam.format.video.nFrameHeight,
					       OMX_TRUE);
	if (err != OMX_ErrorNone) {
		return err;
	}

	err = omx_v4l_colorconv_set_pix_format(omxStandComp,
					       outPort->sVideoParam.eColorFormat,
					       &outPort->sPortParam.format.video.nFrameWidth,
					       &outPort->sPortParam.format.video.nFrameHeight,
					       OMX_FALSE);

	if (err != OMX_ErrorNone) {
		return err;
	}


	DEBUG(DEB_LEV_FULL_SEQ, "(Init)Input image parameters Buffer:(%d,%d) Crop offset:(%d,%d) Crop size:(%d,%d) Stride:%d\n",
	      (int)inPort->sPortParam.format.video.nFrameWidth, (int)inPort->sPortParam.format.video.nSliceHeight,
	      (int)inPort->omxConfigCrop.nLeft, (int)inPort->omxConfigCrop.nTop,
	      (int)inPort->omxConfigCrop.nWidth, (int)inPort->omxConfigCrop.nHeight,
	      (int)inPort->sPortParam.format.video.nStride
	      );

	DEBUG(DEB_LEV_FULL_SEQ, "(Init)Output image parameters Buffer:(%d,%d) Crop offset:(%d,%d) Crop size:(%d,%d) Stride:%d\n",
	      (int)outPort->sPortParam.format.video.nFrameWidth, (int)outPort->sPortParam.format.video.nSliceHeight,
	      (int)outPort->omxConfigCrop.nLeft, (int)outPort->omxConfigCrop.nTop,
	      (int)outPort->omxConfigCrop.nWidth, (int)outPort->omxConfigCrop.nHeight,
	      (int)outPort->sPortParam.format.video.nStride
	      );

	omx_v4l_colorconv_set_crop(omxStandComp, inPort, OMX_TRUE);
	omx_v4l_colorconv_set_crop(omxStandComp, outPort, OMX_FALSE);


	if (err != OMX_ErrorNone) {
		return err;
	}

	err = v4l2_request_buffers(colorconv_Priv->vid_fd, &inPort->nBuffersRequested,
				   V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

	DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d err= %d\n", __func__, __LINE__, err);
	if (err != OMX_ErrorNone) {
		return err;
	}
	err = v4l2_request_buffers(colorconv_Priv->vid_fd, &outPort->nBuffersRequested,
				   V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	DEBUG(DEB_LEV_FULL_SEQ, "In %s:%d err= %d\n", __func__, __LINE__, err);
	if (err != OMX_ErrorNone) {
		return err;
	}


	DEBUG(DEB_LEV_FULL_SEQ, "%s: Requested %d INPUT buffers for %s\n", __func__,
	      inPort->nBuffersRequested, colorconv_Priv->devnode_name);
	DEBUG(DEB_LEV_FULL_SEQ, "%s: Requested %d OUTPUT buffers for %s\n", __func__,
	      outPort->nBuffersRequested, colorconv_Priv->devnode_name);

	return err;
};

/** The Deinitialization function
  * This function dealloates the frames and buffers to store the color
  * converterted output from input yuv
  */
OMX_ERRORTYPE omx_v4l_colorconv_Deinit(OMX_COMPONENTTYPE *omxStandComp)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	omx_v4l_colorconv_PrivateType* colorconv_Priv = omxStandComp->pComponentPrivate;
	struct v4l2_requestbuffers req;

	fimc_m2m_stream(colorconv_Priv->vid_fd, 0);

	CLEAR(req);
	req.count = 0;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req.memory = V4L2_MEMORY_USERPTR;

	if (-1 == ioctl(colorconv_Priv->vid_fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			DEBUG(DEB_LEV_ERR, "%s does not support memory mapping\n",
			      colorconv_Priv->devnode_name);
			return OMX_ErrorHardware;
		} else {
			DEBUG(DEB_LEV_ERR, "%s error %d, %s\n", "VIDIOC_REQBUFS", errno,
			      strerror(errno));
			return OMX_ErrorHardware;
		}
	}
	close(colorconv_Priv->vid_fd);
	return err;
}

/** @brief Called by the standard use buffer, it implements a base functionality.
 *
 * This function can be overriden if the use buffer implicate more complicated operations.
 * The parameters are the same as the standard function, except for the handle of the port
 * instead of the handler of the component.
 * When the buffers needed by this port are all assigned or allocated, the variable
 * bIsFullOfBuffers becomes equal to OMX_TRUE
 */

OMX_ERRORTYPE omx_v4l_colorconv_UseBuffer(
	OMX_HANDLETYPE hComponent,
	OMX_BUFFERHEADERTYPE **ppBufferHdr,
	OMX_U32 nPortIndex,
	OMX_PTR pAppPrivate,
	OMX_U32 nSizeBytes,
	OMX_U8 *pBuffer)
{
	OMX_COMPONENTTYPE* omxComponent = (OMX_COMPONENTTYPE *)hComponent;
	omx_v4l_colorconv_PrivateType* colorconv_Priv = omxComponent->pComponentPrivate;
	omx_base_PortType *pPort = (omx_base_PortType *) colorconv_Priv->ports[nPortIndex];
	OMX_ERRORTYPE err;

	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s for port %d (%p)\n", __func__, (int)nPortIndex, pPort);

	/* Call base class UseBuffer method */
	err = pPort->Port_UseBuffer(pPort, ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);
	if (err != OMX_ErrorNone) {
		DEBUG(DEB_LEV_ERR, "Out of %s for component %p with err 0x%x\n", __func__,
		      omxComponent, (unsigned int)err);
	}

	return err;
}

static int m2m_streaming;

#define perror_exit(cond, func) \
	if (cond) { \
		fprintf(stderr, "%s:%d: ", __func__, __LINE__);\
		perror(func);\
		exit(EXIT_FAILURE);\
	}


int fimc_m2m_stream(int vid_fd, int on)
{
	int vidc = on ? VIDIOC_STREAMON : VIDIOC_STREAMOFF;
	enum v4l2_buf_type type;
	int err;

	if (!m2m_streaming == !on)
		return 0;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (ioctl(vid_fd, vidc, &type) == -1) {
		err = errno;
		DEBUG(DEB_LEV_ERR, "ioctl M2M CAPTURE VIDIOC_STREAMO%s\n",
		      on ? "N\n" : "FF\n");
		return err;
	}

	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	if (ioctl(vid_fd, vidc, &type) == -1) {
		err = errno;
		DEBUG(DEB_LEV_ERR, "ioctl M2M OUTPUT VIDIOC_STREAMO%s\n",
		      on ? "N\n" : "FF\n");
		return err;
	}
	m2m_streaming = on;
	return 0;
}


int fimc_m2m_process(int vid_fd, struct buffer *src_buf, struct buffer *dst_buf)
{
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buf;
	struct pollfd test_fd;
	int ret, i;

	DEBUG(DEB_LEV_FULL_SEQ, "src_buf[%d]: addr[0]: 0x%x. size[0]: %ld\n",
	       src_buf->index, (unsigned int)src_buf->addr[0], src_buf->size[0]);

	DEBUG(DEB_LEV_FULL_SEQ, "dst_buf[%d]: addr[0]: 0x%x. size[0]: %ld\n",
	       dst_buf->index, (unsigned int)dst_buf->addr[0], dst_buf->size[0]);

	CLEAR(buf);
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.index = src_buf->index;
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.m.planes = planes;
	buf.length = src_buf->num_planes;

	for (i = 0; i < src_buf->num_planes; i++) {
		planes[i].m.userptr = (unsigned long)src_buf->addr[i];
		planes[i].length = src_buf->size[i];
		planes[i].bytesused = src_buf->planes[i].bytesused;
	}

	ret = ioctl(vid_fd, VIDIOC_QBUF, &buf);

	if (ret == -1) {
		DEBUG(DEB_LEV_ERR, "Error in line: %d\n", __LINE__);
		return OMX_ErrorHardware;
	}

	CLEAR(buf);
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.index = dst_buf->index;
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.m.planes = planes;
	buf.length = dst_buf->num_planes;

	for (i = 0; i < dst_buf->num_planes; i++) {
		planes[i].m.userptr = (unsigned long)dst_buf->addr[i];
		planes[i].length = dst_buf->size[i];
		planes[i].bytesused = dst_buf->planes[i].bytesused;
	}

	ret = ioctl(vid_fd, VIDIOC_QBUF, &buf);

	if (ret == -1) {
			DEBUG(DEB_LEV_ERR, "Error in line: %d\n", __LINE__);
			return OMX_ErrorHardware;
		}

	fimc_m2m_stream(vid_fd, 1);

	test_fd.fd = vid_fd;
	test_fd.events = POLLOUT | POLLERR;
	ret = poll(&test_fd, 1, 2000);
	if (ret == -1)
		perror("poll ioctl");

	CLEAR(buf);
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.m.planes = planes;
	buf.length = src_buf->num_planes;
	ret = ioctl(vid_fd, VIDIOC_DQBUF, &buf);

	if (ret == -1) {
		DEBUG(DEB_LEV_ERR, "Error in line: %d\n", __LINE__);
		return OMX_ErrorHardware;
	}

	test_fd.fd = vid_fd;
	test_fd.events = POLLIN | POLLERR;
	ret = poll(&test_fd, 1, 2000);
	if (ret == -1)
		perror("poll ioctl");

	CLEAR(buf);
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.m.planes = planes;
	buf.length = dst_buf->num_planes;

	ret = ioctl(vid_fd, VIDIOC_DQBUF, &buf);

	if (ret == -1) {
		perror("dqbuf ioctl");
		DEBUG(DEB_LEV_ERR, "Error in line: %d\n", __LINE__);
		return OMX_ErrorHardware;
	}

	return 0;
}


/** This function is used to process the input buffer and provide one output buffer
  */
void omx_v4l_colorconv_BufferMgmtCallback(OMX_COMPONENTTYPE *omxStandComp,
					  OMX_BUFFERHEADERTYPE *pInputBuffer,
					  OMX_BUFFERHEADERTYPE *pOutputBuffer)
{
	struct buffer inBuf, outBuf;

	omx_v4l_colorconv_PrivateType* colorconv_Priv = omxStandComp->pComponentPrivate;
	omx_v4l_colorconv_PortType *inPort = (omx_v4l_colorconv_PortType *)colorconv_Priv->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	omx_v4l_colorconv_PortType *outPort = (omx_v4l_colorconv_PortType *)colorconv_Priv->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];

	DEBUG(DEB_LEV_FULL_SEQ, "Buffer count PROCESS: Actual:%d Min:%d Assigned:%d\n",
	      (int)inPort->sPortParam.nBufferCountActual,
	      (int)inPort->sPortParam.nBufferCountMin,
	      (int)inPort->nNumAssignedBuffers
	      );

	DEBUG(DEB_LEV_FULL_SEQ , "Input image parameters Buffer:(%d,%d) Crop offset:(%d,%d) Crop size:(%d,%d) Stride:%d\n",
	      (int)inPort->sPortParam.format.video.nFrameWidth, (int)inPort->sPortParam.format.video.nSliceHeight,
	      (int)inPort->omxConfigCrop.nLeft, (int)inPort->omxConfigCrop.nTop,
	      (int)inPort->omxConfigCrop.nWidth, (int)inPort->omxConfigCrop.nHeight,
	      (int)inPort->sPortParam.format.video.nStride
	      );

	DEBUG(DEB_LEV_FULL_SEQ, "Output image parameters Buffer:(%d,%d) Crop offset:(%d,%d) Crop size:(%d,%d) Stride:%d\n",
	      (int)outPort->sPortParam.format.video.nFrameWidth, (int)outPort->sPortParam.format.video.nSliceHeight,
	      (int)outPort->omxConfigCrop.nLeft, (int)outPort->omxConfigCrop.nTop,
	      (int)outPort->omxConfigCrop.nWidth, (int)outPort->omxConfigCrop.nHeight,
	      (int)outPort->sPortParam.format.video.nStride
	      );
	DEBUG(DEB_LEV_FULL_SEQ, " nFilledLen= %d\n", (int)pInputBuffer->nFilledLen);

	OMX_U8 *input_src_ptr = (OMX_U8*) (pInputBuffer->pBuffer);
	OMX_U8 *output_dest_ptr = (OMX_U8*) (pOutputBuffer->pBuffer);

	DEBUG(DEB_LEV_FULL_SEQ, " output_dest_ptr: 0x%p, input_src_ptr: 0x%p\n",
	      output_dest_ptr, input_src_ptr);

	if (colorconv_Priv->samsungProprietaryCommunication) {
		SAMSUNG_NV12MT_BUFFER *sb = (SAMSUNG_NV12MT_BUFFER *)pInputBuffer->pBuffer;

		DEBUG(DEB_LEV_FULL_SEQ, "Proprietary Communication Addresses: Y=0x%p UV=0x%p i=%d\n",
		      sb->yPlane, sb->uvPlane, (int)sb->bufferIndex);

		inBuf.addr[0] = (char*)sb->yPlane;
		inBuf.size[0] = sb->yPlaneSize;
		inBuf.addr[1] = (char*)sb->uvPlane;
		inBuf.size[1] = sb->uvPlaneSize;
		inBuf.index = sb->bufferIndex;
		inBuf.num_planes = 2;
		inBuf.planes[0].bytesused = sb->yPlaneSize;
		inBuf.planes[1].bytesused = sb->uvPlaneSize;

		outBuf.addr[0] = (char*)output_dest_ptr;
		outBuf.size[0] = pOutputBuffer->nAllocLen;
		outBuf.index = 0;
		outBuf.num_planes = 1;
		outBuf.planes[0].bytesused = 0;

		outBuf.width = colorconv_Priv->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.video.nFrameWidth;
		outBuf.height = colorconv_Priv->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.video.nFrameHeight;
		outBuf.stride = colorconv_Priv->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.video.nStride;

		TIME("Before processing buffer: %d\n", (int)sb->bufferIndex);

		fimc_m2m_process(colorconv_Priv->vid_fd, &inBuf,  &outBuf);

		TIME("After processing buffer: %d\n", (int)sb->bufferIndex);

		pOutputBuffer->nFilledLen = (OMX_U32) pOutputBuffer->nAllocLen; //abs(output_dest_stride) * output_dest_height;
		pInputBuffer->nFilledLen = 0;

		return;
	}

	inBuf.addr[0] = (char*)input_src_ptr;
	inBuf.size[0] = pInputBuffer->nFilledLen;
	inBuf.index = 0;
	inBuf.num_planes = 1;
	inBuf.planes[0].bytesused = pInputBuffer->nFilledLen;

	outBuf.addr[0] = (char*)output_dest_ptr;
	outBuf.size[0] = pOutputBuffer->nAllocLen;
	outBuf.index = 0;
	outBuf.num_planes = 1;
	outBuf.planes[0].bytesused = 0;
	outBuf.width = colorconv_Priv->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.video.nFrameWidth;
	outBuf.height = colorconv_Priv->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.video.nFrameHeight;
	outBuf.stride = colorconv_Priv->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX]->sPortParam.format.video.nStride;

	fimc_m2m_process(colorconv_Priv->vid_fd, &inBuf,  &outBuf);

	pOutputBuffer->nFilledLen = (OMX_U32) pOutputBuffer->nAllocLen;
	pInputBuffer->nFilledLen = 0;

	DEBUG(DEB_LEV_FULL_SEQ, "in %s One output buffer %p len: %d is full\n",
	      __func__, pOutputBuffer->pBuffer, (int)pOutputBuffer->nFilledLen);
}


OMX_ERRORTYPE omx_v4l_colorconv_SetConfig(OMX_HANDLETYPE hComponent,
					  OMX_INDEXTYPE nIndex,
					  OMX_PTR pComponentConfigStructure)
{
	OMX_U32 portIndex;
	/* Possible configs to set */
	OMX_CONFIG_RECTTYPE *omxConfigCrop;
	OMX_CONFIG_ROTATIONTYPE *omxConfigRotate;
	OMX_CONFIG_MIRRORTYPE *omxConfigMirror;
	OMX_CONFIG_SCALEFACTORTYPE *omxConfigScale;
	OMX_CONFIG_POINTTYPE *omxConfigOutputPosition;
	OMX_ERRORTYPE err = OMX_ErrorNone;

	/* Check which structure we are being fed and make control its header */
	OMX_COMPONENTTYPE *omxStandComp = (OMX_COMPONENTTYPE *)hComponent;
	omx_v4l_colorconv_PrivateType* colorconv_Priv = omxStandComp->pComponentPrivate;
	omx_v4l_colorconv_PortType *pPort;

	if (pComponentConfigStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting configuration %i\n", nIndex);

	switch (nIndex) {
	case OMX_IndexConfigCommonInputCrop:
	case OMX_IndexConfigCommonOutputCrop:
		omxConfigCrop = (OMX_CONFIG_RECTTYPE*)pComponentConfigStructure;
		portIndex = omxConfigCrop->nPortIndex;
		if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_RECTTYPE))) != OMX_ErrorNone) {
			break;
		}
		if ( (nIndex == OMX_IndexConfigCommonOutputCrop && portIndex == OMX_BASE_FILTER_OUTPUTPORT_INDEX)  ||
		     (nIndex == OMX_IndexConfigCommonInputCrop && portIndex == OMX_BASE_FILTER_INPUTPORT_INDEX) ) {
			pPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[portIndex];
			pPort->omxConfigCrop.nLeft = omxConfigCrop->nLeft;
			pPort->omxConfigCrop.nTop = omxConfigCrop->nTop;
			pPort->omxConfigCrop.nWidth = omxConfigCrop->nWidth;
			pPort->omxConfigCrop.nHeight = omxConfigCrop->nHeight;
		} else if (portIndex <= 1) {
			return OMX_ErrorUnsupportedIndex;
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
		if (portIndex <= 1) {
			pPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[portIndex];
			if (omxConfigRotate->nRotation != 0) {
				/* Rotation not supported (yet) */
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
		if (portIndex <= 1) {
			if (omxConfigMirror->eMirror == OMX_MirrorBoth || omxConfigMirror->eMirror == OMX_MirrorHorizontal)  {
				/* Horizontal mirroring not yet supported */
				return OMX_ErrorUnsupportedSetting;
			}
			pPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[portIndex];
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
		if (portIndex <= 1) {
			if (omxConfigScale->xWidth != 0x10000 || omxConfigScale->xHeight != 0x10000) {
				/* Scaling not yet supported */
				return OMX_ErrorUnsupportedSetting;
			}
			pPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[portIndex];
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
		if (portIndex == OMX_BASE_FILTER_OUTPUTPORT_INDEX) {
			pPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[portIndex];
			pPort->omxConfigOutputPosition.nX = omxConfigOutputPosition->nX;
			pPort->omxConfigOutputPosition.nY = omxConfigOutputPosition->nY;
		} else if (portIndex == OMX_BASE_FILTER_INPUTPORT_INDEX) {
			return OMX_ErrorUnsupportedIndex;
		} else {
			return OMX_ErrorBadPortIndex;
		}
		break;

	default: /* delegate to superclass */
		return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
	}
	return err;
}


OMX_ERRORTYPE omx_v4l_colorconv_GetConfig(
				OMX_HANDLETYPE hComponent,
				OMX_INDEXTYPE nIndex,
				OMX_PTR pComponentConfigStructure)
{
	/* Possible configurations to ask for */
	OMX_CONFIG_RECTTYPE *omxConfigCrop;
	OMX_CONFIG_ROTATIONTYPE *omxConfigRotate;
	OMX_CONFIG_MIRRORTYPE *omxConfigMirror;
	OMX_CONFIG_SCALEFACTORTYPE *omxConfigScale;
	OMX_CONFIG_POINTTYPE *omxConfigOutputPosition;
	OMX_ERRORTYPE err = OMX_ErrorNone;

	OMX_COMPONENTTYPE *omxStandComp = (OMX_COMPONENTTYPE *)hComponent;
	omx_v4l_colorconv_PrivateType* colorconv_Priv = omxStandComp->pComponentPrivate;
	omx_v4l_colorconv_PortType *pPort;

	if (pComponentConfigStructure == NULL) {
		return OMX_ErrorBadParameter;
	}
	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting configuration %i\n", nIndex);

	/* Check which structure we are being fed and fill its header */
	switch (nIndex) {
	case OMX_IndexConfigCommonInputCrop:
		omxConfigCrop = (OMX_CONFIG_RECTTYPE*)pComponentConfigStructure;
		if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_RECTTYPE))) != OMX_ErrorNone) {
			break;
		}
		if (omxConfigCrop->nPortIndex == OMX_BASE_FILTER_INPUTPORT_INDEX) {
			pPort = (omx_v4l_colorconv_PortType *)colorconv_Priv->ports[omxConfigCrop->nPortIndex];
			memcpy(omxConfigCrop, &pPort->omxConfigCrop, sizeof(OMX_CONFIG_RECTTYPE));
		} else if (omxConfigCrop->nPortIndex == OMX_BASE_FILTER_OUTPUTPORT_INDEX) {
			return OMX_ErrorUnsupportedIndex;
		} else {
			return OMX_ErrorBadPortIndex;
		}
		break;

	case OMX_IndexConfigCommonOutputCrop:
		omxConfigCrop = (OMX_CONFIG_RECTTYPE*)pComponentConfigStructure;
		if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_RECTTYPE))) != OMX_ErrorNone) {
			break;
		}
		if (omxConfigCrop->nPortIndex == OMX_BASE_FILTER_OUTPUTPORT_INDEX) {
			pPort = (omx_v4l_colorconv_PortType *)colorconv_Priv->ports[omxConfigCrop->nPortIndex];
			memcpy(omxConfigCrop, &pPort->omxConfigCrop, sizeof(OMX_CONFIG_RECTTYPE));
		} else if (omxConfigCrop->nPortIndex == OMX_BASE_FILTER_INPUTPORT_INDEX) {
			return OMX_ErrorUnsupportedIndex;
		} else {
			return OMX_ErrorBadPortIndex;
		}
		break;

	case OMX_IndexConfigCommonRotate:
		omxConfigRotate = (OMX_CONFIG_ROTATIONTYPE*)pComponentConfigStructure;
		if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_ROTATIONTYPE))) != OMX_ErrorNone) {
			break;
		}
		if (omxConfigRotate->nPortIndex <= 1) {
			pPort = (omx_v4l_colorconv_PortType *)colorconv_Priv->ports[omxConfigRotate->nPortIndex];
			memcpy(omxConfigRotate, &pPort->omxConfigRotate, sizeof(OMX_CONFIG_ROTATIONTYPE));
		} else {
			return OMX_ErrorBadPortIndex;
		}
		break;

	case OMX_IndexConfigCommonMirror:
		omxConfigMirror = (OMX_CONFIG_MIRRORTYPE*)pComponentConfigStructure;
		if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_MIRRORTYPE))) != OMX_ErrorNone) {
			break;
		}
		if (omxConfigMirror->nPortIndex <= 1) {
			pPort = (omx_v4l_colorconv_PortType *)colorconv_Priv->ports[omxConfigMirror->nPortIndex];
			memcpy(omxConfigMirror, &pPort->omxConfigMirror, sizeof(OMX_CONFIG_MIRRORTYPE));
		} else {
			return OMX_ErrorBadPortIndex;
		}
		break;

	case OMX_IndexConfigCommonScale:
		omxConfigScale = (OMX_CONFIG_SCALEFACTORTYPE*)pComponentConfigStructure;
		if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_SCALEFACTORTYPE))) != OMX_ErrorNone) {
			break;
		}
		if (omxConfigScale->nPortIndex <= 1) {
			pPort = (omx_v4l_colorconv_PortType *)colorconv_Priv->ports[omxConfigScale->nPortIndex];
			memcpy(omxConfigScale, &pPort->omxConfigScale, sizeof(OMX_CONFIG_SCALEFACTORTYPE));
		} else {
			return OMX_ErrorBadPortIndex;
		}
		break;

	case OMX_IndexConfigCommonOutputPosition:
		omxConfigOutputPosition = (OMX_CONFIG_POINTTYPE*)pComponentConfigStructure;
		if ((err = checkHeader(pComponentConfigStructure, sizeof(OMX_CONFIG_POINTTYPE))) != OMX_ErrorNone) {
			break;
		}
		if (omxConfigOutputPosition->nPortIndex == OMX_BASE_FILTER_OUTPUTPORT_INDEX) {
			pPort = (omx_v4l_colorconv_PortType *)colorconv_Priv->ports[omxConfigOutputPosition->nPortIndex];
			memcpy(omxConfigOutputPosition, &pPort->omxConfigOutputPosition, sizeof(OMX_CONFIG_POINTTYPE));
		} else if (omxConfigOutputPosition->nPortIndex == OMX_BASE_FILTER_INPUTPORT_INDEX) {
			return OMX_ErrorUnsupportedIndex;
		} else {
			return OMX_ErrorBadPortIndex;
		}
		break;

	default: // delegate to superclass
		return omx_base_component_GetConfig(hComponent, nIndex, pComponentConfigStructure);
	}
	return err;
}

OMX_ERRORTYPE omx_v4l_colorconv_SetParameter(
				OMX_HANDLETYPE hComponent,
				OMX_INDEXTYPE nParamIndex,
				OMX_PTR ComponentParameterStructure)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
	OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
	OMX_U32 portIndex;

	/* Check which structure we are being fed and make control its header */
	OMX_COMPONENTTYPE *omxStandComp = (OMX_COMPONENTTYPE *)hComponent;
	omx_v4l_colorconv_PrivateType* colorconv_Priv = omxStandComp->pComponentPrivate;
	omx_v4l_colorconv_PortType *pPort;

	if (ComponentParameterStructure == NULL)
		return OMX_ErrorBadParameter;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter 0x%x\n", nParamIndex);

	switch(nParamIndex) {
	case OMX_IndexParamPortDefinition:
		DEBUG(DEB_LEV_FULL_SEQ, "%s:%s:%d: Setting PortDefinition\n", __FILE__, __func__, __LINE__);

		pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE*) ComponentParameterStructure;
		portIndex = pPortDef->nPortIndex;

		err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pPortDef,
							      sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err);
			break;
		}
		pPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[portIndex];
		pPort->sPortParam.nBufferCountActual = pPortDef->nBufferCountActual;

		/* Copy stuff from OMX_VIDEO_PORTDEFINITIONTYPE structure */
		pPort->sPortParam.format.video.nFrameWidth = pPortDef->format.video.nFrameWidth;
		pPort->sPortParam.format.video.nFrameHeight = pPortDef->format.video.nFrameHeight;
		pPort->sPortParam.format.video.nBitrate = pPortDef->format.video.nBitrate;
		pPort->sPortParam.format.video.xFramerate = pPortDef->format.video.xFramerate;
		pPort->sPortParam.format.video.bFlagErrorConcealment = pPortDef->format.video.bFlagErrorConcealment;
		/* Figure out stride, slice height, min buffer size */
		pPort->sPortParam.format.video.nStride = calcStride(pPort->sPortParam.format.video.nFrameWidth,
								    pPort->sVideoParam.eColorFormat);
		/* No support for slices yet */
		pPort->sPortParam.format.video.nSliceHeight = pPort->sPortParam.format.video.nFrameHeight;
		/* Read-only field by spec */

		DEBUG(DEB_LEV_FULL_SEQ, "PROP: %d portIndex: %d\n", (int)colorconv_Priv->samsungProprietaryCommunication, (int)portIndex);

		if (colorconv_Priv->samsungProprietaryCommunication && portIndex == OMX_BASE_FILTER_INPUTPORT_INDEX) {
			pPort->sPortParam.nBufferSize = sizeof(SAMSUNG_NV12MT_BUFFER);
			DEBUG(DEB_LEV_FULL_SEQ, "Adjusting size for proprietary communication!\n");
		} else {
			pPort->sPortParam.nBufferSize = (OMX_U32) abs(pPort->sPortParam.format.video.nStride) *
				pPort->sPortParam.format.video.nSliceHeight;
		}

		pPort->omxConfigCrop.nWidth = pPort->sPortParam.format.video.nFrameWidth;
		pPort->omxConfigCrop.nHeight = pPort->sPortParam.format.video.nFrameHeight;
		break;

	case OMX_IndexParamVideoPortFormat:
		DEBUG(DEB_LEV_FULL_SEQ, "%s:%s:%d: Setting PortFormat\n", __FILE__, __func__, __LINE__);

		//  FIXME: How do we handle the nIndex member?
		pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
		portIndex = pVideoPortFormat->nPortIndex;
		err = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoPortFormat,
							      sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,err);
			break;
		}
		pPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[portIndex];
		if (pVideoPortFormat->eCompressionFormat != OMX_VIDEO_CodingUnused)  {
			//  No compression allowed
			return OMX_ErrorUnsupportedSetting;
		}
		pPort->sVideoParam.eCompressionFormat = pVideoPortFormat->eCompressionFormat;
		pPort->sVideoParam.eColorFormat = pVideoPortFormat->eColorFormat;

		pPort->sPortParam.format.video.eColorFormat = pPort->sVideoParam.eColorFormat;

		/* FIXME: */
		/* if (pPort->ffmpeg_pxlfmt == PIX_FMT_NONE) { */
		/** no real pixel format supported by ffmpeg for this user input color format
		 * so return bad parameter error to user application */
		/*   return OMX_ErrorBadParameter; */
		/* } */

		//  Figure out stride, slice height, min buffer size
		pPort->sPortParam.format.video.nStride = calcStride(pPort->sPortParam.format.video.nFrameWidth, pPort->sVideoParam.eColorFormat);
		pPort->sPortParam.format.video.nSliceHeight = pPort->sPortParam.format.video.nFrameHeight;  //  No support for slices yet
		pPort->sPortParam.nBufferSize = (OMX_U32) abs(pPort->sPortParam.format.video.nStride) * pPort->sPortParam.format.video.nSliceHeight;

		DEBUG(DEB_LEV_FULL_SEQ, "PROP: %d portIndex: %d\n", (int)colorconv_Priv->samsungProprietaryCommunication, (int)portIndex);

		if (colorconv_Priv->samsungProprietaryCommunication && portIndex == OMX_BASE_FILTER_INPUTPORT_INDEX) {
			pPort->sPortParam.nBufferSize = sizeof(SAMSUNG_NV12MT_BUFFER);
			DEBUG(DEB_LEV_FULL_SEQ, "Adjusting size for proprietary communication!\n");
		}

		break;

	default: /*Call the base component function*/
		return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}
	return err;
}


OMX_ERRORTYPE omx_v4l_colorconv_GetParameter(
				OMX_HANDLETYPE hComponent,
				OMX_INDEXTYPE nParamIndex,
				OMX_PTR ComponentParameterStructure)
{

	OMX_COMPONENTTYPE *omxStandComp = (OMX_COMPONENTTYPE *)hComponent;
	omx_v4l_colorconv_PrivateType* colorconv_Priv = omxStandComp->pComponentPrivate;
	OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
	omx_v4l_colorconv_PortType *pPort;
	OMX_ERRORTYPE err = OMX_ErrorNone;

	if (ComponentParameterStructure == NULL) {
		return OMX_ErrorBadParameter;
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter 0x%x\n", nParamIndex);
	/* Check which structure we are being fed and fill its header */
	switch(nParamIndex) {
	case OMX_IndexParamVideoInit:
		if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) {
			break;
		}
		memcpy(ComponentParameterStructure, &colorconv_Priv->sPortTypesParam[OMX_PortDomainVideo], sizeof(OMX_PORT_PARAM_TYPE));
		break;
	case OMX_IndexParamVideoPortFormat:
		pVideoPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
		if ((err = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) {
			break;
		}
		if (pVideoPortFormat->nPortIndex <= 1) {
			pPort = (omx_v4l_colorconv_PortType *)colorconv_Priv->ports[pVideoPortFormat->nPortIndex];
			memcpy(pVideoPortFormat, &pPort->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
		} else {
			return OMX_ErrorBadPortIndex;
		}
		break;
	default: /*Call the base component function*/
		return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
	}
	return err;
}


OMX_ERRORTYPE omx_v4l_colorconv_MessageHandler(OMX_COMPONENTTYPE* omxStandComp,
					       internalRequestMessageType *message)
{
	DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s\n", __func__);

	omx_v4l_colorconv_PrivateType* colorconv_Priv = (omx_v4l_colorconv_PrivateType*)omxStandComp->pComponentPrivate;
	OMX_ERRORTYPE err = OMX_ErrorNone;
	OMX_STATETYPE eState;
	omx_v4l_colorconv_PortType *inPort, *outPort;

	eState = colorconv_Priv->state; /* storing current state */

	if (message->messageType == OMX_CommandStateSet) {
		DEBUG(DEB_LEV_SIMPLE_SEQ, "CommandStateSet state: %d (current: %d)\n",
		      message->messageParam, colorconv_Priv->state);

		inPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
		outPort = (omx_v4l_colorconv_PortType *) colorconv_Priv->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];

		DEBUG(DEB_LEV_FULL_SEQ, "Buffer count: Actual:%d Min:%d Assigned:%d\n",
		      (int)inPort->sPortParam.nBufferCountActual,
		      (int)inPort->sPortParam.nBufferCountMin,
		      (int)inPort->nNumAssignedBuffers
		      );

		if ((message->messageParam == OMX_StateExecuting) && (colorconv_Priv->state == OMX_StateIdle)) {
			DEBUG(DEB_LEV_SIMPLE_SEQ, "%s:%d -- 2??\n", __func__, __LINE__);

			err = omx_v4l_colorconv_Init(omxStandComp);
			if (err != OMX_ErrorNone) {
				DEBUG(DEB_LEV_ERR, "In %s Video Color Converter Init Error: 0x%x\n", __func__, err);
				return err;
			}
		}

		if ((message->messageParam == OMX_StateExecuting) && (colorconv_Priv->state == OMX_StateIdle)) {
			int numberOfBuffers;

			DEBUG(DEB_LEV_SIMPLE_SEQ, "%s:%d Transition to Executing (from Idle)\n", __func__, __LINE__);

			numberOfBuffers = inPort->nNumAssignedBuffers;
			err = v4l2_request_buffers(colorconv_Priv->vid_fd, &numberOfBuffers, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
			if (err != OMX_ErrorNone) {
				DEBUG(DEB_LEV_ERR, "In %s Video Color Converter Output Reqbufs Error: 0x%x\n",__func__,err);
				return err;
			}
			if (numberOfBuffers != inPort->nNumAssignedBuffers) {
				DEBUG(DEB_LEV_ERR,
				      "In %s Video Color Converter Output Reqbufs returned too little buffers: 0x%x\n", __func__, err);
				return err;
			}


			numberOfBuffers = outPort->nNumAssignedBuffers;
			err = v4l2_request_buffers(colorconv_Priv->vid_fd, &numberOfBuffers, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
			if (err != OMX_ErrorNone) {
				DEBUG(DEB_LEV_ERR, "In %s Video Color Converter Capture Reqbufs Error: 0x%x\n",__func__,err);
				return err;
			}
			if (numberOfBuffers != outPort->nNumAssignedBuffers) {
				DEBUG(DEB_LEV_ERR, "In %s Video Color Converter Capture Reqbufs returned too little buffers: 0x%x\n",
				      __func__, err);
				return err;
			}
		}

		if ((message->messageParam == OMX_StateExecuting ) && (colorconv_Priv->state == OMX_StateIdle)) {
			err = OMX_ErrorNone; //omx_v4l_colorconv_StreamOn(omxStandComp);
			if (err != OMX_ErrorNone) {
				DEBUG(DEB_LEV_ERR, "In %s Video Color Converter Stream On Error: 0x%x\n",__func__,err);
				return err;
			}
		}
	}

	DEBUG(DEB_LEV_SIMPLE_SEQ, "%s:%d\n", __func__, __LINE__);

	/* Execute the base message handling */
	err = omx_base_component_MessageHandler(omxStandComp, message);

	DEBUG(DEB_LEV_SIMPLE_SEQ, "%s:%d\n", __func__, __LINE__);

	if (message->messageType == OMX_CommandStateSet) {
		if ((message->messageParam == OMX_StateIdle ) &&
		    (colorconv_Priv->state == OMX_StateIdle) && eState == OMX_StateExecuting) {

			err = omx_v4l_colorconv_Deinit(omxStandComp);
			if (err != OMX_ErrorNone) {
				DEBUG(DEB_LEV_ERR, "In %s Video Color Converter Deinit Error=%x\n",__func__,err);
				return err;
			}
		}
	}
	return err;
}

OMX_ERRORTYPE omx_video_colorconv_UseEGLImage (
			OMX_HANDLETYPE hComponent,
			OMX_BUFFERHEADERTYPE** ppBufferHdr,
			OMX_U32 nPortIndex,
			OMX_PTR pAppPrivate,
			void* eglImage)
{
	return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE fimc_input_port_ComponentTunnelRequest(
			omx_base_PortType* openmaxStandPort,
			OMX_HANDLETYPE hTunneledComp,
			OMX_U32 nTunneledPort,
			OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
	OMX_COMPONENTTYPE *omxTunStandComp = (OMX_COMPONENTTYPE *)hTunneledComp;

	OMX_COMPONENTTYPE* openmaxStandComp = openmaxStandPort->standCompContainer;
	omx_v4l_colorconv_PrivateType* colorconv_Priv = (omx_v4l_colorconv_PrivateType*)openmaxStandComp->pComponentPrivate;

	char componentName[128];
	OMX_STRING MFCName = "OMX.samsung.video_decoder";
	OMX_VERSIONTYPE componentVesion, specVersion;
	OMX_UUIDTYPE componentUUID;

	omxTunStandComp->GetComponentVersion(hTunneledComp, componentName, &componentVesion, &specVersion, &componentUUID);

	DEBUG(DEB_LEV_FULL_SEQ, "In %s, pairing with %s\n", __func__, componentName);

	if (strncmp((char*)componentName, (char*)MFCName, strlen((char*)MFCName)) == 0) {
		DEBUG(DEB_LEV_FULL_SEQ, "%s: Pairing with Samsung MFC (%s)\n", __func__, MFCName);

		pTunnelSetup->nTunnelFlags |= TUNNEL_IS_SUPPLIER;
		pTunnelSetup->nTunnelFlags |= PROPRIETARY_COMMUNICATION_ESTABLISHED;

		colorconv_Priv->samsungProprietaryCommunication = OMX_TRUE;

		openmaxStandPort->sPortParam.nBufferSize = 1;
	}

	return base_port_ComponentTunnelRequest(openmaxStandPort, hTunneledComp, nTunneledPort, pTunnelSetup);
}

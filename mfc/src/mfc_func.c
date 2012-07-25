/*
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
 * mfc_func.c: MFC related functions
 */

#include <bellagio/omxcore.h>
#include <bellagio/omx_base_video_port.h>
#include <omx_videodec_component.h>
#include <OMX_Video.h>

#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <poll.h>

#include "../samsung-proprietary.h"
#include "mfc_func.h"
#include "parser.h"
#include "tile_convert.h"

#define memzero(x)\
        memset(&(x), 0, sizeof (x));

/** Initialize the MFC codec hardware */
OMX_ERRORTYPE omx_videodec_component_MFCInit(omx_videodec_component_PrivateType* omx_videodec_component_Private) {
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	int ret;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "MFC initialization started\n");

	memset(&fmt, 0, sizeof(struct v4l2_format));

	switch(omx_videodec_component_Private->video_coding_type) {
	case OMX_VIDEO_CodingMPEG4 :
		fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MPEG4;
		break;
	case OMX_VIDEO_CodingMPEG2 :
			fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MPEG2;
			break;
	case OMX_VIDEO_CodingAVC :
		fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
		break;
	case OMX_VIDEO_CodingH263 :
		fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H263;
		break;
	default :
	  DEBUG(DEB_LEV_ERR, "Codecs other than H.264, MPEG-4 and MPEG-2 are not supported -- codec not found\nAnd its number is %d\n", (int)omx_videodec_component_Private->video_coding_type);
	  return OMX_ErrorComponentNotFound;
	}

	omx_videodec_component_Private->mfcFileHandle = open(MFC_DEVICE_NAME, O_RDWR, 0);

	if (omx_videodec_component_Private->mfcFileHandle < 0) {
		DEBUG(DEB_LEV_ERR, "Cannot open MFC device\n");
		return OMX_ErrorComponentNotFound; // or should it be OMX_ErrorInsufficientResources ?
	}

	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_QUERYCAP, &cap);
	if (ret != 0) {
		DEBUG(DEB_LEV_ERR, "Cannot query device capabilities\n");
		return OMX_ErrorComponentNotFound;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) ||
		!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE) ||
		!(cap.capabilities & V4L2_CAP_STREAMING)) {
		DEBUG(DEB_LEV_ERR, "Capabilities of the opened v4l2 device are insufficient\n");
		return OMX_ErrorComponentNotFound;
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	fmt.fmt.pix_mp.plane_fmt[0].sizeimage = MFC_COMPRESSED_BUFFER_SIZE;
	fmt.fmt.pix_mp.num_planes = 1;

	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_S_FMT, &fmt);
	if (ret != 0) {
		DEBUG(DEB_LEV_ERR, "S_FMT failed\n");
		return OMX_ErrorInsufficientResources;
	}

	//tsem_up(omx_videodec_component_Private->avCodecSyncSem);
	omx_videodec_component_Private->mfcParserLastFrameFinished = OMX_TRUE;
	omx_videodec_component_Private->mfcInitialized = OMX_TRUE;

	DEBUG(DEB_LEV_SIMPLE_SEQ, "MFC initialization complete\n");

	return OMX_ErrorNone;
}

/** Deinitialize the MFC codec hardware */
void omx_videodec_component_MFCDeInit(omx_videodec_component_PrivateType* omx_videodec_component_Private) {
	close(omx_videodec_component_Private->mfcFileHandle);
	DEBUG(DEB_LEV_SIMPLE_SEQ, "MFC deinitialization complete\n");
}

OMX_ERRORTYPE MFCAllocInputBuffers(omx_videodec_component_PrivateType* omx_videodec_component_Private)
{
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[MFC_NUM_PLANES];
	int ret;

	// First lets request buffers
	memzero(reqbuf);
	reqbuf.count    = 1;
	reqbuf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	reqbuf.memory   = V4L2_MEMORY_MMAP;

	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_REQBUFS, &reqbuf);
	if (ret) {
		DEBUG(DEB_LEV_ERR, "Failed to allocate buffers for MFC input\n");
		return OMX_ErrorHardware;
	}
	//mfc_num_src_bufs   = reqbuf.count;

	if (reqbuf.count != 1) {
		DEBUG(DEB_LEV_ERR, "Incorrect number of buffers allocated\n");
		return OMX_ErrorHardware;
	}

	memzero(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;
	buf.m.planes = planes;
	buf.length = 1;

	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_QUERYBUF, &buf);
	if (ret) {
		DEBUG(DEB_LEV_ERR, "Failed to query input buffer from MFC\n");
		return OMX_ErrorInsufficientResources;
	}

	DEBUG(DEB_LEV_FULL_SEQ, "Plane Length: %d\n", buf.m.planes[0].length);

	omx_videodec_component_Private->mfcInBufferSize = buf.m.planes[0].length;
	omx_videodec_component_Private->mfcInBufferOff = 0;


	omx_videodec_component_Private->mfcInBuffer =
	        mmap(NULL, buf.m.planes[0].length, PROT_READ | PROT_WRITE,
	        MAP_SHARED, omx_videodec_component_Private->mfcFileHandle, buf.m.planes[0].m.mem_offset);
	if (omx_videodec_component_Private->mfcInBuffer == MAP_FAILED) {
		DEBUG(DEB_LEV_ERR, "Failed to map input buffer from MFC\n");
		return OMX_ErrorHardware;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE MFCQueueOutputBuffer(omx_videodec_component_PrivateType* omx_videodec_component_Private, OMX_U32 bufferIndex)
{
	struct v4l2_buffer buf;
	struct v4l2_plane planes[MFC_NUM_PLANES];
	int ret;

	if (omx_videodec_component_Private->mfcOutBufferQueued[bufferIndex] == 1) {
		DEBUG(DEB_LEV_ERR, "Warning: Tried to queue an already queued buffer.\n");
		return OMX_ErrorNone;
	}

	memzero(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = bufferIndex;
	buf.m.planes = planes;
	buf.length = MFC_NUM_PLANES;

	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_QBUF, &buf);
	if (ret) {
		DEBUG(DEB_LEV_ERR, "Failed to queue output buffer from MFC\n");
		return OMX_ErrorHardware;
	}

	omx_videodec_component_Private->mfcOutBufferQueued[bufferIndex] = 1;
	return OMX_ErrorNone;
}

OMX_ERRORTYPE MFCAllocOutputBuffers(omx_videodec_component_PrivateType* omx_videodec_component_Private)
{
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[MFC_NUM_PLANES];
	int ret;
	int i, j;

	// First lets request buffers
	memzero(reqbuf);
	reqbuf.count    = omx_videodec_component_Private->mfcOutBufferMinCount + MFC_DEFUALT_EXTRA_OUT_BUFFERS_COUNT;
	reqbuf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	reqbuf.memory   = V4L2_MEMORY_MMAP;

	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_REQBUFS, &reqbuf);
	if (ret) {
		DEBUG(DEB_LEV_ERR, "Failed to allocate buffers for MFC input\n");
		return OMX_ErrorHardware;
	}

	if (reqbuf.count < omx_videodec_component_Private->mfcOutBufferMinCount) {
		DEBUG(DEB_LEV_ERR, "Incorrect number of buffers allocated\n");
		return OMX_ErrorHardware;
	}

	omx_videodec_component_Private->mfcOutBufferCount = reqbuf.count;

	for (i = 0; i < reqbuf.count; i++) {
		memzero(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		buf.m.planes = planes;
		buf.length = MFC_NUM_PLANES;

		ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_QUERYBUF, &buf);
		if (ret) {
			DEBUG(DEB_LEV_ERR, "Failed to query output buffer from MFC\n");
			return OMX_ErrorInsufficientResources;
		}

		for (j = 0; j < MFC_NUM_PLANES; j++) {
			DEBUG(DEB_LEV_FULL_SEQ, "Plane %d Length: %d\n", j, buf.m.planes[j].length);
			omx_videodec_component_Private->mfcOutBufferPlaneSize[j] = buf.m.planes[j].length;
			omx_videodec_component_Private->mfcOutBuffer[i][j] =
					mmap(NULL, buf.m.planes[j].length, PROT_READ | PROT_WRITE,
					MAP_SHARED, omx_videodec_component_Private->mfcFileHandle, buf.m.planes[j].m.mem_offset);
			if (omx_videodec_component_Private->mfcOutBuffer[i][j] == MAP_FAILED) {
				DEBUG(DEB_LEV_ERR, "Failed to map input buffer from MFC\n");
				return OMX_ErrorHardware;
			}
		}

		omx_videodec_component_Private->mfcSamsungProprietaryBuffers[i].bufferIndex = i;
		omx_videodec_component_Private->mfcSamsungProprietaryBuffers[i].yPlane = omx_videodec_component_Private->mfcOutBuffer[i][0];
		omx_videodec_component_Private->mfcSamsungProprietaryBuffers[i].yPlaneSize = omx_videodec_component_Private->mfcOutBufferPlaneSize[0];
		omx_videodec_component_Private->mfcSamsungProprietaryBuffers[i].uvPlane = omx_videodec_component_Private->mfcOutBuffer[i][1];
		omx_videodec_component_Private->mfcSamsungProprietaryBuffers[i].uvPlaneSize = omx_videodec_component_Private->mfcOutBufferPlaneSize[1];

		ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_QBUF, &buf);
		if (ret) {
			DEBUG(DEB_LEV_ERR, "Failed to queue output buffer from MFC\n");
			return OMX_ErrorHardware;
		}

		omx_videodec_component_Private->mfcOutBufferQueued[i] = 1;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE MFCStartProcessing(omx_videodec_component_PrivateType* omx_videodec_component_Private)
{
	enum v4l2_buf_type type;
	int ret;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_STREAMON, &type);
	if (ret) {
		DEBUG(DEB_LEV_ERR, "Failed to start header processing\n");
		return OMX_ErrorHardware;
	}

	return OMX_ErrorNone;
}


OMX_ERRORTYPE MFCProcessHeader(omx_videodec_component_PrivateType* omx_videodec_component_Private)
{
	struct v4l2_plane planes[MFC_NUM_PLANES];
	struct v4l2_buffer qbuf;
	enum v4l2_buf_type type;
	int ret;

	memzero(qbuf);
	qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	qbuf.memory = V4L2_MEMORY_MMAP;
	qbuf.index = 0;
	qbuf.m.planes = planes;
	qbuf.length = 1;
	qbuf.m.planes[0].bytesused = omx_videodec_component_Private->mfcInBufferFilled;

	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_QBUF, &qbuf);

	if (ret) {
		DEBUG(DEB_LEV_ERR, "Failed to queue stream header for processing\n");
		return OMX_ErrorHardware;
	}

	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	// Processing the header requires running streamon
	// on OUTPUT queue
	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_STREAMON, &type);
	if (ret) {
		DEBUG(DEB_LEV_ERR, "Failed to start header processing\n");
		return OMX_ErrorHardware;
	}

	return OMX_ErrorNone;
}


OMX_ERRORTYPE MFCProcessFrame(omx_videodec_component_PrivateType* omx_videodec_component_Private)
{
	struct v4l2_plane planes[MFC_NUM_PLANES];
	struct v4l2_buffer qbuf;
	int ret;

	memzero(qbuf);
	qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	qbuf.memory = V4L2_MEMORY_MMAP;
	qbuf.index = 0;
	qbuf.m.planes = planes;
	qbuf.length = 1;
	qbuf.m.planes[0].bytesused = omx_videodec_component_Private->mfcInBufferFilled;

	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_QBUF, &qbuf);

	if (ret) {
		DEBUG(DEB_LEV_ERR, "Failed to queue stream header for processing\n");
		return OMX_ErrorHardware;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE MFCReadHeadInfo(omx_videodec_component_PrivateType* omx_videodec_component_Private)
{
	struct v4l2_control ctrl;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	omx_base_video_PortType *inPort = (omx_base_video_PortType *)omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
	int ret;

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_G_FMT, &fmt);

	if (ret) {
		DEBUG(DEB_LEV_ERR, "Failed to read processed header data\n");
		return OMX_ErrorHardware;
	}

	omx_videodec_component_Private->mfcOutBufferWidth = fmt.fmt.pix.width;
	omx_videodec_component_Private->mfcOutBufferHeight = fmt.fmt.pix.height;

	DEBUG(DEB_LEV_FULL_SEQ, "MFC buffer size is: %dx%d\n",
			(unsigned int)omx_videodec_component_Private->mfcOutBufferWidth,
			(unsigned int)omx_videodec_component_Private->mfcOutBufferHeight);

	memzero(crop);
	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_G_CROP, &crop);
	if (ret) {
		DEBUG(DEB_LEV_ERR, "Failed to get cropping information.\n");
		return OMX_ErrorHardware;
	}

	omx_videodec_component_Private->mfcOutBufferCropWidth = crop.c.width;
	omx_videodec_component_Private->mfcOutBufferCropHeight = crop.c.height;
	omx_videodec_component_Private->mfcOutBufferCropLeft = crop.c.left;
	omx_videodec_component_Private->mfcOutBufferCropTop = crop.c.top;



	DEBUG(DEB_LEV_FULL_SEQ, "MFC movie crop is: (l=%d, t=%d, w=%d, h=%d)\n",
			(unsigned int)omx_videodec_component_Private->mfcOutBufferCropLeft,
			(unsigned int)omx_videodec_component_Private->mfcOutBufferCropTop,
			(unsigned int)omx_videodec_component_Private->mfcOutBufferCropWidth,
			(unsigned int)omx_videodec_component_Private->mfcOutBufferCropHeight);

	inPort->sPortParam.format.video.nFrameWidth = crop.c.width;
	inPort->sPortParam.format.video.nFrameHeight = crop.c.height;

	ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_G_CTRL, &ctrl);

	DEBUG(DEB_LEV_FULL_SEQ, "MFC min buffer number is: %d\n", ctrl.value);
	omx_videodec_component_Private->mfcOutBufferMinCount = ctrl.value;
	return OMX_ErrorNone;
}

OMX_BOOL MFCInputBufferProcessed(omx_videodec_component_PrivateType* omx_videodec_component_Private)
{
	struct pollfd p;
	int ret;

	p.fd = omx_videodec_component_Private->mfcFileHandle;
	p.events = POLLOUT;

	ret = poll(&p, 1, 0);

	if (ret < 0) {
		DEBUG(DEB_LEV_ERR, "Error while polling MFC file descriptor\n");
		return OMX_FALSE;
	}

	if (ret == 0)
		return OMX_FALSE;

	return OMX_TRUE;
}

OMX_BOOL MFCOutputBufferProcessed(omx_videodec_component_PrivateType* omx_videodec_component_Private)
{
	struct pollfd p;
	int ret;

	p.fd = omx_videodec_component_Private->mfcFileHandle;
	p.events = POLLIN;

	ret = poll(&p, 1, 0);

	if (ret < 0) {
		DEBUG(DEB_LEV_ERR, "Error while polling MFC file descriptor\n");
		return OMX_FALSE;
	}

	if (ret == 0)
		return OMX_FALSE;

	return OMX_TRUE;
}

OMX_ERRORTYPE MFCDequeueInputBuffer(omx_videodec_component_PrivateType* omx_videodec_component_Private)
{
	struct v4l2_buffer qbuf;
	struct v4l2_plane planes[MFC_NUM_PLANES];
	int ret;

	memzero(qbuf);
	qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	qbuf.memory = V4L2_MEMORY_MMAP;
	qbuf.m.planes = planes;
	qbuf.length = MFC_NUM_PLANES;

	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_DQBUF, &qbuf);
	if (ret) {
		DEBUG(DEB_LEV_ERR, "Failed to deque\n");
		return OMX_ErrorHardware;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE MFCDequeueOutputBuffer(omx_videodec_component_PrivateType* omx_videodec_component_Private, OMX_U32 *pBufferIndex)
{
	struct v4l2_buffer qbuf;
	struct v4l2_plane planes[MFC_NUM_PLANES];
	int ret;

	memzero(qbuf);
	qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	qbuf.memory = V4L2_MEMORY_MMAP;
	qbuf.m.planes = planes;
	qbuf.length = MFC_NUM_PLANES;

	ret = ioctl(omx_videodec_component_Private->mfcFileHandle, VIDIOC_DQBUF, &qbuf);
	if (ret) {
			DEBUG(DEB_LEV_ERR, "Failed to dequeue\n");
			return OMX_ErrorHardware;
	}

	if (omx_videodec_component_Private->mfcOutBufferQueued[qbuf.index] == 0) {
		DEBUG(DEB_LEV_ERR, "Warning: Tried to dequeue an not queued buffer.\n");
		return OMX_ErrorNone;
	}

	omx_videodec_component_Private->mfcOutBufferQueued[qbuf.index] = 0;
	*pBufferIndex = qbuf.index;

	return OMX_ErrorNone;
}



OMX_ERRORTYPE MFCParseAndQueueInput(omx_videodec_component_PrivateType* omx_videodec_component_Private, OMX_BUFFERHEADERTYPE* pInputBuffer)
{
	OMX_U32 parserConsumed;
	OMX_U32 parserCopied;
	int ret;

	DEBUG(DEB_LEV_FULL_SEQ, "In %s\n", __func__);

	if (!omx_videodec_component_Private->mfcInitialized) {
		DEBUG(DEB_LEV_FULL_SEQ, "%s: MFC not; initialized\n", __func__);
		return OMX_ErrorIncorrectStateOperation;
	}

	if (!omx_videodec_component_Private->headerParsed) {
		/* Let's parse the header, shall we? */
		DEBUG(DEB_LEV_FULL_SEQ, "Parsing header\n");

		/* Allocate buffer in MFC for input */

		if (omx_videodec_component_Private->mfcInBuffer == 0) {
			ret = MFCAllocInputBuffers(omx_videodec_component_Private);
			if (ret != OMX_ErrorNone) {
				DEBUG(DEB_LEV_ERR, "Failed to allocate MFC buffers\n");
				return ret;
			}
		}

		DEBUG(DEB_LEV_FULL_SEQ, "MFCInBufAddr: %p, MFCInOff: 0x%x\n",
						omx_videodec_component_Private->mfcInBuffer,
						(unsigned int)omx_videodec_component_Private->mfcInBufferOff
						);

		switch(omx_videodec_component_Private->video_coding_type) {
		case OMX_VIDEO_CodingAVC:
			parserConsumed = h264_ParseStream(
							pInputBuffer->pBuffer + pInputBuffer->nOffset,
							pInputBuffer->nFilledLen,
							omx_videodec_component_Private->mfcInBuffer + omx_videodec_component_Private->mfcInBufferOff,
							omx_videodec_component_Private->mfcInBufferSize - omx_videodec_component_Private->mfcInBufferOff,
							&omx_videodec_component_Private->mfcH264ParserState,
							&omx_videodec_component_Private->mfcParserLastFrameFinished,
							&parserCopied,
							OMX_TRUE
						);
		break;
		case OMX_VIDEO_CodingH263:
		case OMX_VIDEO_CodingMPEG4:
			parserConsumed = mpeg4_ParseStream(pInputBuffer->pBuffer + pInputBuffer->nOffset,
							pInputBuffer->nFilledLen,
							omx_videodec_component_Private->mfcInBuffer + omx_videodec_component_Private->mfcInBufferOff,
							omx_videodec_component_Private->mfcInBufferSize - omx_videodec_component_Private->mfcInBufferOff,
							&omx_videodec_component_Private->mfcMPEG4ParserState,
							&omx_videodec_component_Private->mfcParserLastFrameFinished,
							&parserCopied,
							OMX_TRUE
							);
		break;
		case OMX_VIDEO_CodingMPEG2:
				parserConsumed = mpeg2_ParseStream(pInputBuffer->pBuffer + pInputBuffer->nOffset,
								pInputBuffer->nFilledLen,
								omx_videodec_component_Private->mfcInBuffer + omx_videodec_component_Private->mfcInBufferOff,
								omx_videodec_component_Private->mfcInBufferSize - omx_videodec_component_Private->mfcInBufferOff,
								&omx_videodec_component_Private->mfcMPEG4ParserState,
								&omx_videodec_component_Private->mfcParserLastFrameFinished,
								&parserCopied,
								OMX_TRUE
								);
				break;
		default:
			DEBUG(DEB_LEV_ERR, "Unknown video codec chosen.\n");
			return OMX_ErrorComponentNotFound;
		}

		if (pInputBuffer->nFilledLen < parserConsumed) {
			DEBUG(DEB_LEV_ERR, "Parser consumed more data than it was available\n");
		}
		if (omx_videodec_component_Private->video_coding_type != OMX_VIDEO_CodingH263) {
			pInputBuffer->nFilledLen -= parserConsumed;
			pInputBuffer->nOffset += parserConsumed;
		} else {
			omx_videodec_component_Private->mfcMPEG4ParserState.seekEnd = OMX_FALSE;
			omx_videodec_component_Private->mfcMPEG4ParserState.gotStart = OMX_FALSE;
			omx_videodec_component_Private->mfcMPEG4ParserState.shortHeader = OMX_FALSE;
			omx_videodec_component_Private->mfcMPEG4ParserState.vopCount = 0;
			omx_videodec_component_Private->mfcMPEG4ParserState.headersCount = 1;
		}

		if (&omx_videodec_component_Private->mfcParserLastFrameFinished) {
			omx_videodec_component_Private->mfcInBufferFilled = parserCopied + omx_videodec_component_Private->mfcInBufferOff;
			DEBUG(DEB_LEV_FULL_SEQ, "Header length: %u\n", (unsigned int)omx_videodec_component_Private->mfcInBufferFilled);
			omx_videodec_component_Private->mfcInBufferOff = 0;

			ret = MFCProcessHeader(omx_videodec_component_Private);
			if (ret == OMX_ErrorNone) ret = MFCReadHeadInfo(omx_videodec_component_Private);
			if (ret == OMX_ErrorNone) ret = MFCAllocOutputBuffers(omx_videodec_component_Private);
			if (ret == OMX_ErrorNone) ret = MFCStartProcessing(omx_videodec_component_Private);

			if (ret != OMX_ErrorNone) {
				DEBUG(DEB_LEV_ERR, "Error processing header\n");
				return ret;
			}

			// TODO Add handling of buffer resize

		} else {
			omx_videodec_component_Private->mfcInBufferOff += parserCopied;
			DEBUG(DEB_LEV_FULL_SEQ, "Header parsing continued\n");
		}

		omx_videodec_component_Private->headerParsed = 1;

		DEBUG(DEB_LEV_FULL_SEQ, "Parsed header\n");
		return OMX_ErrorNone;
	}

	/* Parse only if there is place for new data */
	if (MFCInputBufferProcessed(omx_videodec_component_Private)) {

		DEBUG(DEB_LEV_FULL_SEQ, "MFCInBufAddr: %p, MFCInOff: 0x%x\n",
				omx_videodec_component_Private->mfcInBuffer,
				(unsigned int)omx_videodec_component_Private->mfcInBufferOff
				);

			if (pInputBuffer->nFilledLen != 0) {

				switch(omx_videodec_component_Private->video_coding_type) {
				case OMX_VIDEO_CodingAVC:
					parserConsumed = h264_ParseStream(
									pInputBuffer->pBuffer + pInputBuffer->nOffset,
									pInputBuffer->nFilledLen,
									omx_videodec_component_Private->mfcInBuffer + omx_videodec_component_Private->mfcInBufferOff,
									omx_videodec_component_Private->mfcInBufferSize - omx_videodec_component_Private->mfcInBufferOff,
									&omx_videodec_component_Private->mfcH264ParserState,
									&omx_videodec_component_Private->mfcParserLastFrameFinished,
									&parserCopied,
									OMX_FALSE
								);
				break;
				case OMX_VIDEO_CodingH263:
				case OMX_VIDEO_CodingMPEG4:
					parserConsumed = mpeg4_ParseStream(pInputBuffer->pBuffer + pInputBuffer->nOffset,
									pInputBuffer->nFilledLen,
									omx_videodec_component_Private->mfcInBuffer + omx_videodec_component_Private->mfcInBufferOff,
									omx_videodec_component_Private->mfcInBufferSize - omx_videodec_component_Private->mfcInBufferOff,
									&omx_videodec_component_Private->mfcMPEG4ParserState,
									&omx_videodec_component_Private->mfcParserLastFrameFinished,
									&parserCopied,
									OMX_FALSE
									);
				break;
				case OMX_VIDEO_CodingMPEG2:
					parserConsumed = mpeg2_ParseStream(pInputBuffer->pBuffer + pInputBuffer->nOffset,
									pInputBuffer->nFilledLen,
									omx_videodec_component_Private->mfcInBuffer + omx_videodec_component_Private->mfcInBufferOff,
									omx_videodec_component_Private->mfcInBufferSize - omx_videodec_component_Private->mfcInBufferOff,
									&omx_videodec_component_Private->mfcMPEG4ParserState,
									&omx_videodec_component_Private->mfcParserLastFrameFinished,
									&parserCopied,
									OMX_FALSE
									);
				break;
				default:
					DEBUG(DEB_LEV_ERR, "Unknown video codec chosen.\n");
					return OMX_ErrorComponentNotFound;
				}
				if (pInputBuffer->nFilledLen < parserConsumed) {
					DEBUG(DEB_LEV_ERR, "Parser consumed more data than was available\n");
					return OMX_ErrorUnderflow;
				}

				pInputBuffer->nFilledLen -= parserConsumed;
				pInputBuffer->nOffset += parserConsumed;
			} else {
				DEBUG(DEB_LEV_FULL_SEQ, "%s: Sending last frame of size %d\n", __func__, omx_videodec_component_Private->mfcInBufferOff);
				omx_videodec_component_Private->mfcParserLastFrameFinished = OMX_TRUE;
				parserCopied = 0;
			}

		if (omx_videodec_component_Private->mfcParserLastFrameFinished) {
			omx_videodec_component_Private->mfcInBufferFilled = parserCopied + omx_videodec_component_Private->mfcInBufferOff;

			DEBUG(DEB_LEV_FULL_SEQ, "Frame length: %u\n", (unsigned int)omx_videodec_component_Private->mfcInBufferFilled);
			omx_videodec_component_Private->mfcInBufferOff = 0;

			/* Dequeue the processed buffer */
			ret = MFCDequeueInputBuffer(omx_videodec_component_Private);
			/* Queue the buffer for processing */
			if (ret == OMX_ErrorNone ) ret = MFCProcessFrame(omx_videodec_component_Private);
			if (ret != OMX_ErrorNone) {
				DEBUG(DEB_LEV_FULL_SEQ, "Failed to process frame data\n");
				return ret;
			}

		} else {
			omx_videodec_component_Private->mfcInBufferOff += parserCopied;
			omx_videodec_component_Private->mfcInBufferFilled += parserCopied;
			DEBUG(DEB_LEV_FULL_SEQ, "Frame parsing continued\n");
		}

	}

	return OMX_ErrorNone;
}


OMX_ERRORTYPE MFCProcessAndDequeueOutput(omx_videodec_component_PrivateType* omx_videodec_component_Private, OMX_BUFFERHEADERTYPE* pOutputBuffer)
{
	OMX_U32 outBufIndex;
	int ret;

	DEBUG(DEB_LEV_FULL_SEQ, "OutputPoll: %d\n", MFCOutputBufferProcessed(omx_videodec_component_Private));


	if (MFCOutputBufferProcessed(omx_videodec_component_Private)) {
		ret = MFCDequeueOutputBuffer(omx_videodec_component_Private, &outBufIndex);
		TIME("Dequeued out mfc buf number: %d\n", (int)outBufIndex);
		DEBUG(DEB_LEV_FULL_SEQ, "Dequeued output buffer with index (!): %d\n", (unsigned int)outBufIndex);
		if (ret != OMX_ErrorNone)
			return ret;

		DEBUG(DEB_LEV_FULL_SEQ, "%s: Got a buffer from MFC\n", __func__);

		//if (!omx_videodec_component_Private->tunelledOutput) {
		if (omx_videodec_component_Private->samsungProprietaryCommunication) {
			//pOutputBuffer->pBuffer = (OMX_U8 *)&omx_videodec_component_Private->mfcSamsungProprietaryBuffers[outBufIndex];

			memcpy(pOutputBuffer->pBuffer, &omx_videodec_component_Private->mfcSamsungProprietaryBuffers[outBufIndex], sizeof(SAMSUNG_NV12MT_BUFFER));

			pOutputBuffer->nFilledLen = sizeof(SAMSUNG_NV12MT_BUFFER);
			pOutputBuffer->nOffset = 0;


		} else{
			Y_tile_to_linear_4x2(pOutputBuffer->pBuffer,
								omx_videodec_component_Private->mfcOutBuffer[outBufIndex][0],
								omx_videodec_component_Private->mfcOutBufferCropWidth,
								omx_videodec_component_Private->mfcOutBufferCropHeight
								);

			CbCr_tile_to_linear_4x2(pOutputBuffer->pBuffer + omx_videodec_component_Private->mfcOutBufferCropWidth * omx_videodec_component_Private->mfcOutBufferCropHeight,
					omx_videodec_component_Private->mfcOutBuffer[outBufIndex][1],
					omx_videodec_component_Private->mfcOutBufferCropWidth,
					omx_videodec_component_Private->mfcOutBufferCropHeight);

			pOutputBuffer->nFilledLen = (3 * omx_videodec_component_Private->mfcOutBufferCropWidth * omx_videodec_component_Private->mfcOutBufferCropHeight) / 2;
			pOutputBuffer->nOffset = 0;

			ret = MFCQueueOutputBuffer(omx_videodec_component_Private, outBufIndex);
			if (ret != OMX_ErrorNone)
				return ret;

		}
	}
	return OMX_ErrorNone;
}


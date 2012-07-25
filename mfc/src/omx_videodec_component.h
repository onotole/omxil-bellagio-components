/**
* @file src/components/mfc/omx_videodec_component.h
*
* This component implements an H.263 / H.264 / MPEG-2 / MPEG-4 AVC video decoder.
* It uses the MFC hardware video codec present in the S5P family of Samsung
* SoCs.
*
* It has been based on the H.264 / MPEG-4 AVC Video decoder using FFmpeg
* software library from the Bellagio OpenMAX IL distribution.
*
* Copyright (C) 2007-2008 STMicroelectronics
* Copyright (C) 2007-2008 Nokia Corporation and/or its subsidiary(-ies).
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
* $Date: 2008-06-27 12:00:23 +0200 (Fri, 27 Jun 2008) $
* Revision $Rev: 554 $
* Author $Author: pankaj_sen $
*/

#ifndef _OMX_VIDEODEC_COMPONENT_H_
#define _OMX_VIDEODEC_COMPONENT_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <bellagio/omx_base_filter.h>
#include <string.h>
#include <linux/videodev2.h>
#include "../samsung-proprietary.h"

/* Specific include files */
#if 1 // FFMPEG_LIBNAME_HEADERS
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#else
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#include <ffmpeg/swscale.h>
#include <ffmpeg/avutil.h>
#endif


#define VIDEO_DEC_BASE_NAME "OMX.samsung.video_decoder"
#define VIDEO_DEC_MPEG4_NAME "OMX.samsung.video_decoder.mpeg4"
#define VIDEO_DEC_H264_NAME "OMX.samsung.video_decoder.avc"
#define VIDEO_DEC_H263_NAME "OMX.samsung.video_decoder.h263"
#define VIDEO_DEC_MPEG2_NAME "OMX.samsung.video_decoder.mpeg2"
#define VIDEO_DEC_MPEG4_ROLE "video_decoder.mpeg4"
#define VIDEO_DEC_H264_ROLE "video_decoder.avc"
#define VIDEO_DEC_MPEG2_ROLE "video_decoder.mpeg2"
#define VIDEO_DEC_H263_ROLE "video_decoder.h263"

/* Size for compressed MFC buffer size */
#define MFC_COMPRESSED_BUFFER_SIZE (1 << 20)
/* MFC device name */
#define MFC_DEVICE_NAME "/dev/video8"

/* Number of planes used by MFC - this can not be modified - it is determined by the pixel format
 * used by the hardware */
#define MFC_NUM_PLANES 2
/* Maximum number of MFC output buffer - again this is limited by the hardware */
#define MFC_MAX_OUT_BUFFERS 32

/* The default number of extra buffers for output. This is the number that will be added
 * to the minimum number of buffers required by MFC */
#define MFC_DEFUALT_EXTRA_OUT_BUFFERS_COUNT 1

/* H264 parser states */
enum mfc_h264_parser_state {
	H264_PARSER_NO_CODE,
	H264_PARSER_CODE_0x1,
	H264_PARSER_CODE_0x2,
	H264_PARSER_CODE_0x3,
	H264_PARSER_CODE_1x1,
	H264_PARSER_CODE_SLICE,
};

/* H264 parser tag types */
enum mfc_h264_tag_type {
	H264_TAG_HEAD,
	H264_TAG_SLICE,
};

/* H264 parser context */
struct mfc_h264_parser_context {
	enum mfc_h264_parser_state state;
	enum mfc_h264_tag_type lastTag;
	OMX_U8 bytes[6];
	OMX_BOOL fourBytesTag;
	OMX_U32 firstSliceCount;
	OMX_U32 headersCount;
	OMX_S32 tmpCodeStart;
	OMX_S32 codeStart;
	OMX_S32 codeEnd;
	OMX_BOOL gotStart;
	OMX_BOOL gotEnd;
	OMX_BOOL seekEnd;
};

/* MPEG4 parser states (also used by the MPEG2 parser) */
enum mfc_mpeg4_parser_state {
	MPEG4_PARSER_NO_CODE,
	MPEG4_PARSER_CODE_0x1,
	MPEG4_PARSER_CODE_0x2,
	MPEG4_PARSER_CODE_1x1,
};

/* MPEG4 parser tag types (also used by the MPEG2 parser) */
enum mfc_mpeg4_tag_type {
	MPEG4_TAG_HEAD,
	MPEG4_TAG_VOP,
};

/* MPEG4 parser context (also used by the MPEG2 parser) */
struct mfc_mpeg4_parser_context {
	enum mfc_mpeg4_parser_state state;
	enum mfc_mpeg4_tag_type lastTag;
	OMX_U8 bytes[6];
	OMX_BOOL fourBytesTag;
	OMX_U32 vopCount;
	OMX_U32 headersCount;
	OMX_S32 tmpCodeStart;
	OMX_S32 codeStart;
	OMX_S32 codeEnd;
	OMX_BOOL gotStart;
	OMX_BOOL gotEnd;
	OMX_BOOL seekEnd;
	OMX_BOOL shortHeader;
};

/** Video Decoder component private structure.
  */
DERIVEDCLASS(omx_videodec_component_PrivateType, omx_base_filter_PrivateType)
#define omx_videodec_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  /** @param avCodec pointer to the FFmpeg video decoder */ \
  AVCodec *avCodec; \
  /** @param avCodecContext pointer to FFmpeg decoder context  */ \
  AVCodecContext *avCodecContext; \
  /** @param picture pointer to FFmpeg AVFrame  */ \
  AVFrame *avFrame; \
  /** @param semaphore for avcodec access syncrhonization */\
  tsem_t* avCodecSyncSem; \
  /** @param pVideoMpeg2 Referece to OMX_VIDEO_PARAM_MPEG2TYPE structure*/  \
  OMX_VIDEO_PARAM_MPEG2TYPE pVideoMpeg2;  \
  /** @param pVideoMpeg4 Referece to OMX_VIDEO_PARAM_MPEG4TYPE structure*/  \
  OMX_VIDEO_PARAM_MPEG4TYPE pVideoMpeg4;  \
  /** @param pVideoMpeg4 Referece to OMX_VIDEO_PARAM_H263TYPE structure*/  \
  OMX_VIDEO_PARAM_H263TYPE pVideoH263;  \
  /** @param pVideoAvc Reference to OMX_VIDEO_PARAM_AVCTYPE structure */ \
  OMX_VIDEO_PARAM_AVCTYPE pVideoAvc;  \
  /** @param avcodecReady boolean flag that is true when the video coded has been initialized */ \
  OMX_BOOL avcodecReady;  \
  /** @param minBufferLength Field that stores the minimun allowed size for FFmpeg decoder */ \
  OMX_U16 minBufferLength; \
  /** @param inputCurrBuffer Field that stores pointer of the current input buffer position */ \
  OMX_U8* inputCurrBuffer;\
  /** @param inputCurrLength Field that stores current input buffer length in bytes */ \
  OMX_U32 inputCurrLength;\
  /** @param isFirstBuffer Field that the buffer is the first buffer */ \
  OMX_S32 isFirstBuffer;\
  /** @param isNewBuffer Field that indicate a new buffer has arrived*/ \
  OMX_S32 isNewBuffer;  \
  /** @param video_coding_type Field that indicate the supported video format of video decoder */ \
  OMX_U32 video_coding_type;   \
  /** @param eOutFramePixFmt Field that indicate output frame pixel format */ \
  enum PixelFormat eOutFramePixFmt; \
  /** @param extradata pointer to extradata*/ \
  OMX_U8* extradata; \
  /** @param extradata_size extradata size*/ \
  OMX_U32 extradata_size; \
  \
  /** File handle of the MFC device */ \
  OMX_S32 mfcFileHandle; \
  /** MFC initialized flag */ \
  OMX_BOOL mfcInitialized; \
  /** Header parsed flag */ \
  OMX_BOOL headerParsed; \
  /** Parser callback function */ \
  OMX_U32 (*ParseHeader)(OMX_U8*, OMX_U32, OMX_BUFFERHEADERTYPE*, OMX_BOOL); \
  /** MFC input buffer */ \
  OMX_U8* mfcInBuffer; \
  /** MFC input buffer offset */ \
  OMX_U32 mfcInBufferOff; \
  /** MFC input buffer size */ \
  OMX_U32 mfcInBufferSize; \
  /** Size of data in the MFC input buffer */ \
  OMX_U32 mfcInBufferFilled; \
  /** MPEG4 parser state */ \
  struct mfc_mpeg4_parser_context mfcMPEG4ParserState; \
  /** Last frame finished flag */ \
  OMX_BOOL mfcParserLastFrameFinished; \
  /** MFC output buffer width */ \
  OMX_U32 mfcOutBufferWidth; \
  /** MFC output buffer height */ \
  OMX_U32 mfcOutBufferHeight; \
  /** MFC output buffer crop parameters */ \
  OMX_U32 mfcOutBufferCropLeft; \
  OMX_U32 mfcOutBufferCropTop; \
  OMX_U32 mfcOutBufferCropWidth; \
  OMX_U32 mfcOutBufferCropHeight; \
  /** Minimum number of buffers for MFC */ \
  OMX_U32 mfcOutBufferMinCount; \
  /** Actual number of buffers allocated for MFC */ \
  OMX_U32 mfcOutBufferCount; \
  /** MFC out buffers */ \
  OMX_U8*  mfcOutBuffer[MFC_MAX_OUT_BUFFERS][MFC_NUM_PLANES]; \
  /** MFC out buffer queued state flags */ \
  OMX_BOOL mfcOutBufferQueued[MFC_MAX_OUT_BUFFERS]; \
  /** MFC out plane sizes */ \
  OMX_U32 mfcOutBufferPlaneSize[MFC_NUM_PLANES]; \
  /** H264 parser context */ \
  struct mfc_h264_parser_context mfcH264ParserState; \
  /** Tunneled output flag */ \
  OMX_BOOL tunneledOutput; \
  /** Flag indicating Samsung proprietary communication (between MFC and FIMC) */ \
  OMX_BOOL samsungProprietaryCommunication; \
  /** BUffers used in Samsung proprietary communication */ \
  SAMSUNG_NV12MT_BUFFER mfcSamsungProprietaryBuffers[MFC_MAX_OUT_BUFFERS];

ENDCLASS(omx_videodec_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_videodec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_videodec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_videodec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_videodec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_videodec_component_MessageHandler(OMX_COMPONENTTYPE*,internalRequestMessageType*);

OMX_ERRORTYPE omx_videodec_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_videodec_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_videodec_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex);

void SetInternalVideoParameters(OMX_COMPONENTTYPE *openmaxStandComp);

OMX_ERRORTYPE omx_videodec_component_SetConfig(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nIndex,
  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_videodec_component_GetExtensionIndex(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_STRING cParameterName,
  OMX_OUT OMX_INDEXTYPE* pIndexType);

/* Allocate output buffer for non tunneled communication */
OMX_ERRORTYPE output_port_AllocateBuffer(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE** pBuffer,
  OMX_U32 nPortIndex,
  OMX_PTR pAppPrivate,
  OMX_U32 nSizeBytes);

/* Allocate input buffer for non tunneled communication */
OMX_ERRORTYPE input_port_AllocateBuffer(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE** pBuffer,
  OMX_U32 nPortIndex,
  OMX_PTR pAppPrivate,
  OMX_U32 nSizeBytes);

/* Allocate output buffer for tunneled communication wrapper */
OMX_ERRORTYPE output_port_AllocateTunnelBuffer(
		omx_base_PortType *openmaxStandPort,
		OMX_U32 nPortIndex);

/* Allocate input buffer for tunneled communication wrapper */
OMX_ERRORTYPE input_port_AllocateTunnelBuffer(
		omx_base_PortType *openmaxStandPort,
		OMX_U32 nPortIndex);

OMX_ERRORTYPE output_port_SendBufferFunction(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE input_port_SendBufferFunction(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE* pBuffer);

/* Buffer management function - here all processing is done */
void* omx_videodec_BufferMgmtFunction (void* param);

/* Allocate output buffer for tunneled communication - allocation takes place here */
OMX_ERRORTYPE mfc_output_port_AllocateTunnelBuffer(
		omx_base_PortType *openmaxStandPort,
		OMX_U32 nPortIndex);

/* Allocate input buffer for tunneled communication - allocation takes place here */
OMX_ERRORTYPE mfc_output_port_FreeTunnelBuffer(
		omx_base_PortType *openmaxStandPort,
		OMX_U32 nPortIndex);

/* Handle the tunnel request and if possible use Samsung proprietary communication */
OMX_ERRORTYPE mfc_output_port_ComponentTunnelRequest(
		omx_base_PortType* openmaxStandPort,
		OMX_HANDLETYPE hTunneledComp,
		OMX_U32 nTunneledPort,
		OMX_TUNNELSETUPTYPE* pTunnelSetup);

#endif

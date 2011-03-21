/**
  src/omx_videodec_component.c

  This component implements H.264 / MPEG-4 AVC video decoder.
  The H.264 / MPEG-4 AVC Video decoder is based on the FFmpeg software library.

  Copyright (C) 2007-2009 STMicroelectronics
  Copyright (C) 2007-2009 Nokia Corporation and/or its subsidiary(-ies)

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
#include <bellagio/omx_base_video_port.h>
#include <omx_videodec_component.h>
#include <OMX_Video.h>

/** Maximum Number of Video Component Instance*/
#define MAX_COMPONENT_VIDEODEC 4
#define TEMP_BUFFER_SIZE 32000

/** Counter of Video Component Instance*/
static OMX_U32 noVideoDecInstance = 0;

/** The output decoded color format */
#define OUTPUT_DECODED_COLOR_FMT OMX_COLOR_FormatYUV420Planar

#define DEFAULT_WIDTH 352
#define DEFAULT_HEIGHT 288
/** define the max input buffer size */
#define DEFAULT_VIDEO_OUTPUT_BUF_SIZE DEFAULT_WIDTH*DEFAULT_HEIGHT*3/2   // YUV 420P

/** The Constructor of the video decoder component
  * @param openmaxStandComp the component handle to be constructed
  * @param cComponentName is the name of the constructed component
  */
OMX_ERRORTYPE omx_videodec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {

  OMX_ERRORTYPE eError = OMX_ErrorNone;
  omx_videodec_component_PrivateType* omx_videodec_component_Private;
  omx_base_video_PortType *inPort,*outPort;
  OMX_U32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n", __func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_videodec_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  } else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %p Already Allocated\n", __func__, openmaxStandComp->pComponentPrivate);
  }

  omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_videodec_component_Private->ports = NULL;

  eError = omx_base_filter_Constructor(openmaxStandComp, cComponentName);

  omx_videodec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nStartPortNumber = 0;
  omx_videodec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts = 2;

  /** Allocate Ports and call port constructor. */
  if (omx_videodec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts && !omx_videodec_component_Private->ports) {
    omx_videodec_component_Private->ports = calloc(omx_videodec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts, sizeof(omx_base_PortType *));
    if (!omx_videodec_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_videodec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
      omx_videodec_component_Private->ports[i] = calloc(1, sizeof(omx_base_video_PortType));
      if (!omx_videodec_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  base_video_port_Constructor(openmaxStandComp, &omx_videodec_component_Private->ports[0], 0, OMX_TRUE);
  base_video_port_Constructor(openmaxStandComp, &omx_videodec_component_Private->ports[1], 1, OMX_FALSE);

  /** here we can override whatever defaults the base_component constructor set
    * e.g. we can override the function pointers in the private struct
    */

  /** Domain specific section for the ports.
    * first we set the parameter common to both formats
    */
  //common parameters related to input port
  inPort = (omx_base_video_PortType *)omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  inPort->sPortParam.nBufferSize = DEFAULT_OUT_BUFFER_SIZE;
  inPort->sPortParam.format.video.xFramerate = 25;

  //common parameters related to output port
  outPort = (omx_base_video_PortType *)omx_videodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  outPort->sPortParam.format.video.eColorFormat = OUTPUT_DECODED_COLOR_FMT;
  outPort->sPortParam.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUF_SIZE;
  outPort->sPortParam.format.video.xFramerate = 25;

  /** settings of output port parameter definition */
  outPort->sVideoParam.eColorFormat = OUTPUT_DECODED_COLOR_FMT;
  outPort->sVideoParam.xFramerate = 25;

  /** now it's time to know the video coding type of the component */
  if(!strcmp(cComponentName, VIDEO_DEC_MPEG4_NAME)) {
    omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG4;
  } else if(!strcmp(cComponentName, VIDEO_DEC_H264_NAME)) {
    omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
  } else if (!strcmp(cComponentName, VIDEO_DEC_BASE_NAME)) {
    omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingUnused;
  } else {
    // IL client specified an invalid component name
    return OMX_ErrorInvalidComponentName;
  }

  SetInternalVideoParameters(openmaxStandComp);

  omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_YUV420P;

  if(omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
    omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  } else {
    omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  }

  /** general configuration irrespective of any video formats
    * setting other parameters of omx_videodec_component_private
    */
  omx_videodec_component_Private->avCodec = NULL;
  omx_videodec_component_Private->avCodecContext= NULL;
  omx_videodec_component_Private->avcodecReady = OMX_FALSE;
  omx_videodec_component_Private->extradata = NULL;
  omx_videodec_component_Private->extradata_size = 0;
  omx_videodec_component_Private->BufferMgmtCallback = omx_videodec_component_BufferMgmtCallback;

  /** initializing the codec context etc that was done earlier by ffmpeglibinit function */
  omx_videodec_component_Private->messageHandler = omx_videodec_component_MessageHandler;
  omx_videodec_component_Private->destructor = omx_videodec_component_Destructor;
  openmaxStandComp->SetParameter = omx_videodec_component_SetParameter;
  openmaxStandComp->GetParameter = omx_videodec_component_GetParameter;
  openmaxStandComp->ComponentRoleEnum = omx_videodec_component_ComponentRoleEnum;

  noVideoDecInstance++;

  if(noVideoDecInstance > MAX_COMPONENT_VIDEODEC) {
    return OMX_ErrorInsufficientResources;
  }
  return eError;
}


/** The destructor of the video decoder component
  */
OMX_ERRORTYPE omx_videodec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;

  if(omx_videodec_component_Private->extradata) {
    free(omx_videodec_component_Private->extradata);
    omx_videodec_component_Private->extradata=NULL;
  }

  /* frees port/s */
  if (omx_videodec_component_Private->ports) {
    for (i=0; i < omx_videodec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
      if(omx_videodec_component_Private->ports[i])
        omx_videodec_component_Private->ports[i]->PortDestructor(omx_videodec_component_Private->ports[i]);
    }
    free(omx_videodec_component_Private->ports);
    omx_videodec_component_Private->ports=NULL;
  }


  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of video decoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);
  noVideoDecInstance--;

  return OMX_ErrorNone;
}


/** It initializates the FFmpeg framework, and opens an FFmpeg videodecoder of type specified by IL client
  */
OMX_ERRORTYPE omx_videodec_component_ffmpegLibInit(omx_videodec_component_PrivateType* omx_videodec_component_Private) {

  OMX_U32 target_codecID;
  avcodec_init();
  av_register_all();

  DEBUG(DEB_LEV_SIMPLE_SEQ, "FFmpeg library/codec initialization\n");

  switch(omx_videodec_component_Private->video_coding_type) {
    case OMX_VIDEO_CodingMPEG4 :
    	DEBUG(DEB_LEV_ERR, "FFmpeg set to CODEC_ID_MPEG4\n");
    	target_codecID = CODEC_ID_MPEG4;
      break;
    case OMX_VIDEO_CodingAVC :
    	DEBUG(DEB_LEV_ERR, "FFmpeg set to CODEC_ID_H264\n");
      target_codecID = CODEC_ID_H264;
      break;
    default :
      DEBUG(DEB_LEV_ERR, "\n codecs other than H.264 / MPEG-4 AVC are not supported -- codec not found\n");
      return OMX_ErrorComponentNotFound;
  }

  /** Find the  decoder corresponding to the video type specified by IL client*/
  omx_videodec_component_Private->avCodec = avcodec_find_decoder(target_codecID);
  if (omx_videodec_component_Private->avCodec == NULL) {
    DEBUG(DEB_LEV_ERR, "Codec not found\n");
    return OMX_ErrorInsufficientResources;
  }

  omx_videodec_component_Private->avCodecContext = avcodec_alloc_context();

  /** necessary flags for MPEG-4 or H.264 stream */
  omx_videodec_component_Private->avFrame = avcodec_alloc_frame();
  if(omx_videodec_component_Private->extradata_size >0) {
    omx_videodec_component_Private->avCodecContext->extradata = omx_videodec_component_Private->extradata;
    omx_videodec_component_Private->avCodecContext->extradata_size = (int)omx_videodec_component_Private->extradata_size;
  } else {
    omx_videodec_component_Private->avCodecContext->flags |= CODEC_FLAG_TRUNCATED;
  }

  if (avcodec_open(omx_videodec_component_Private->avCodecContext, omx_videodec_component_Private->avCodec) < 0) {
    DEBUG(DEB_LEV_ERR, "Could not open codec\n");
    return OMX_ErrorInsufficientResources;
  }
  av_init_packet(&(omx_videodec_component_Private->avPacket));
  DEBUG(DEB_LEV_SIMPLE_SEQ, "done\n");

  return OMX_ErrorNone;
}

/** It Deinitializates the ffmpeg framework, and close the ffmpeg video decoder of selected coding type
  */
void omx_videodec_component_ffmpegLibDeInit(omx_videodec_component_PrivateType* omx_videodec_component_Private) {

  avcodec_close(omx_videodec_component_Private->avCodecContext);
  if (omx_videodec_component_Private->avCodecContext->priv_data) {
    avcodec_close (omx_videodec_component_Private->avCodecContext);
  }
  if (omx_videodec_component_Private->extradata_size == 0 && omx_videodec_component_Private->avCodecContext->extradata) {
    av_free (omx_videodec_component_Private->avCodecContext->extradata);
    //omx_videodec_component_Private->avCodecContext->extradata = NULL;
    //omx_videodec_component_Private->avCodecContext->extradata_size = 0;
  }
  av_free (omx_videodec_component_Private->avCodecContext);

  av_free(omx_videodec_component_Private->avFrame);

}

/** internal function to set codec related parameters in the private type structure
  */
void SetInternalVideoParameters(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videodec_component_PrivateType* omx_videodec_component_Private;
  omx_base_video_PortType *inPort ;

  omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;;

  if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
    strcpy(omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.cMIMEType,"video/mpeg4");
    omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;

    setHeader(&omx_videodec_component_Private->pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
    omx_videodec_component_Private->pVideoMpeg4.nPortIndex = 0;
    omx_videodec_component_Private->pVideoMpeg4.nSliceHeaderSpacing = 0;
    omx_videodec_component_Private->pVideoMpeg4.bSVH = OMX_FALSE;
    omx_videodec_component_Private->pVideoMpeg4.bGov = OMX_FALSE;
    omx_videodec_component_Private->pVideoMpeg4.nPFrames = 0;
    omx_videodec_component_Private->pVideoMpeg4.nBFrames = 0;
    omx_videodec_component_Private->pVideoMpeg4.nIDCVLCThreshold = 0;
    omx_videodec_component_Private->pVideoMpeg4.bACPred = OMX_FALSE;
    omx_videodec_component_Private->pVideoMpeg4.nMaxPacketSize = 0;
    omx_videodec_component_Private->pVideoMpeg4.nTimeIncRes = 0;
    omx_videodec_component_Private->pVideoMpeg4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    omx_videodec_component_Private->pVideoMpeg4.eLevel = OMX_VIDEO_MPEG4Level0;
    omx_videodec_component_Private->pVideoMpeg4.nAllowedPictureTypes = 0;
    omx_videodec_component_Private->pVideoMpeg4.nHeaderExtension = 0;
    omx_videodec_component_Private->pVideoMpeg4.bReversibleVLC = OMX_FALSE;

    inPort = (omx_base_video_PortType *)omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
    inPort->sVideoParam.eCompressionFormat = OMX_VIDEO_CodingMPEG4;

  } else if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingAVC) {
    strcpy(omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.cMIMEType,"video/avc(h264)");
    omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

    setHeader(&omx_videodec_component_Private->pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    omx_videodec_component_Private->pVideoAvc.nPortIndex = 0;
    omx_videodec_component_Private->pVideoAvc.nSliceHeaderSpacing = 0;
    omx_videodec_component_Private->pVideoAvc.bUseHadamard = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.nRefFrames = 2;
    omx_videodec_component_Private->pVideoAvc.nPFrames = 0;
    omx_videodec_component_Private->pVideoAvc.nBFrames = 0;
    omx_videodec_component_Private->pVideoAvc.bUseHadamard = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.nRefFrames = 2;
    omx_videodec_component_Private->pVideoAvc.eProfile = OMX_VIDEO_AVCProfileBaseline;
    omx_videodec_component_Private->pVideoAvc.eLevel = OMX_VIDEO_AVCLevel1;
    omx_videodec_component_Private->pVideoAvc.nAllowedPictureTypes = 0;
    omx_videodec_component_Private->pVideoAvc.bFrameMBsOnly = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.nRefIdx10ActiveMinus1 = 0;
    omx_videodec_component_Private->pVideoAvc.nRefIdx11ActiveMinus1 = 0;
    omx_videodec_component_Private->pVideoAvc.bEnableUEP = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.bEnableFMO = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.bEnableASO = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.bEnableRS = OMX_FALSE;

    omx_videodec_component_Private->pVideoAvc.bMBAFF = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.bEntropyCodingCABAC = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.bWeightedPPrediction = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.nWeightedBipredicitonMode = 0;
    omx_videodec_component_Private->pVideoAvc.bconstIpred = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.bDirect8x8Inference = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.bDirectSpatialTemporal = OMX_FALSE;
    omx_videodec_component_Private->pVideoAvc.nCabacInitIdc = 0;
    omx_videodec_component_Private->pVideoAvc.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterDisable;

    inPort = (omx_base_video_PortType *)omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
    inPort->sVideoParam.eCompressionFormat = OMX_VIDEO_CodingAVC;
  }
}


/** The Initialization function of the video decoder
  */
OMX_ERRORTYPE omx_videodec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  /** Temporary First Output buffer size */
  omx_videodec_component_Private->inputCurrBuffer = NULL;
  omx_videodec_component_Private->inputCurrLength = 0;
  omx_videodec_component_Private->isFirstBuffer = OMX_TRUE;
  omx_videodec_component_Private->isNewBuffer = 1;
  omx_videodec_component_Private->pendingOffset = 0;
  omx_videodec_component_Private->isPendingBuffer = OMX_FALSE;
  omx_videodec_component_Private->internalInputBuffer = malloc(TEMP_BUFFER_SIZE);
  if (omx_videodec_component_Private->internalInputBuffer == NULL) {
	    DEBUG(DEB_LEV_ERR, "Out of memory\n");
	    eError = OMX_ErrorInsufficientResources;
  }
  return eError;
}

/** The Deinitialization function of the video decoder
  */
OMX_ERRORTYPE omx_videodec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  if (omx_videodec_component_Private->avcodecReady) {
    omx_videodec_component_ffmpegLibDeInit(omx_videodec_component_Private);
    omx_videodec_component_Private->avcodecReady = OMX_FALSE;
  }

  return eError;
}

/** Executes all the required steps after an output buffer frame-size has changed.
*/
static inline void UpdateFrameSize(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *outPort = (omx_base_video_PortType *)omx_videodec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  omx_base_video_PortType *inPort = (omx_base_video_PortType *)omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  outPort->sPortParam.format.video.nFrameWidth = inPort->sPortParam.format.video.nFrameWidth;
  outPort->sPortParam.format.video.nFrameHeight = inPort->sPortParam.format.video.nFrameHeight;
  switch(outPort->sVideoParam.eColorFormat) {
    case OMX_COLOR_FormatYUV420Planar:
      if(outPort->sPortParam.format.video.nFrameWidth && outPort->sPortParam.format.video.nFrameHeight) {
        outPort->sPortParam.nBufferSize = outPort->sPortParam.format.video.nFrameWidth * outPort->sPortParam.format.video.nFrameHeight * 3/2;
      }
      break;
    default:
      if(outPort->sPortParam.format.video.nFrameWidth && outPort->sPortParam.format.video.nFrameHeight) {
        outPort->sPortParam.nBufferSize = outPort->sPortParam.format.video.nFrameWidth * outPort->sPortParam.format.video.nFrameHeight * 3;
      }
      break;
  }
}

struct SwsContext *imgConvertYuvCtx_dec = NULL;
/** This function is used to process the input buffer and provide one output buffer
  */
void omx_videodec_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_BUFFERHEADERTYPE* pOutputBuffer) {

  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  AVPicture pic;
  int got_picture;

  OMX_S32 nOutputFilled = 0;
  OMX_U8* outputCurrBuffer;
  int nLen = 0;
  int nSize;
  OMX_ERRORTYPE err;
  unsigned int frameLen;
  int ret = 0;
  DEBUG(DEB_LEV_ERR, "In %s \n",__func__);

  if(omx_videodec_component_Private->isFirstBuffer == OMX_TRUE) {
    omx_videodec_component_Private->isFirstBuffer = OMX_FALSE;

    if (!omx_videodec_component_Private->avcodecReady) {
      err = omx_videodec_component_ffmpegLibInit(omx_videodec_component_Private);
      if (err != OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s omx_videodec_component_ffmpegLibInit Failed\n",__func__);
        return;
      }
      omx_videodec_component_Private->avcodecReady = OMX_TRUE;
    }

    if(pInputBuffer->nFilledLen == 0) {
      return;
    }
  }

  /** Fill up the current input buffer when a new buffer has arrived */
  if(omx_videodec_component_Private->isNewBuffer) {

	  DEBUG(DEB_LEV_ERR, "NEW BUFFER!!!\n");
    omx_videodec_component_Private->inputCurrBuffer = pInputBuffer->pBuffer;
    omx_videodec_component_Private->inputCurrLength = pInputBuffer->nFilledLen;

    omx_videodec_component_Private->isNewBuffer = 0;
    DEBUG(DEB_LEV_FULL_SEQ, "New Buffer FilledLen = %d\n", (int)pInputBuffer->nFilledLen);

  }

  outputCurrBuffer = pOutputBuffer->pBuffer;
  pOutputBuffer->nFilledLen = 0;
  pOutputBuffer->nOffset = 0;

  if (omx_videodec_component_Private->isPendingBuffer) {
	  ret = nextFrameLen(omx_videodec_component_Private->inputCurrBuffer, omx_videodec_component_Private->inputCurrLength, &frameLen);
	  DEBUG(DEB_LEV_FULL_SEQ, "latest old buffer COMPRESSED FRAME LEN = %u\n", omx_videodec_component_Private->pendingOffset);
	  DEBUG(DEB_LEV_FULL_SEQ, "new buffer COMPRESSED FRAME LEN = %u ret = %i\n", frameLen, ret);
	  if (ret < 0) {
		  DEBUG(DEB_LEV_ERR, "Unrecoverable error!!!!!!!\n");
		  return;
	  }
	  if ((frameLen + omx_videodec_component_Private->pendingOffset) > TEMP_BUFFER_SIZE) {
		  DEBUG(DEB_LEV_ERR, "Not enough memory for the frame!!!!!!!\n");
		  return;
	  }
	  memcpy(omx_videodec_component_Private->internalInputBuffer + omx_videodec_component_Private->pendingOffset,
			  omx_videodec_component_Private->inputCurrBuffer,
			  frameLen);
	  omx_videodec_component_Private->pendingOffset = omx_videodec_component_Private->pendingOffset + frameLen;
  }
  while (!nOutputFilled) {
	  if (!omx_videodec_component_Private->isPendingBuffer) {
		  ret = nextFrameLen(omx_videodec_component_Private->inputCurrBuffer, omx_videodec_component_Private->inputCurrLength, &frameLen);
		  DEBUG(DEB_LEV_FULL_SEQ, "COMPRESSED FRAME LEN = %u ret = %i\n", frameLen, ret);
		  if (ret < 0) {
			  if (frameLen < TEMP_BUFFER_SIZE) {
				  memcpy(omx_videodec_component_Private->internalInputBuffer, omx_videodec_component_Private->inputCurrBuffer, frameLen);
				  if (pInputBuffer->nFilledLen != frameLen) {
					  DEBUG(DEB_LEV_ERR, "NOOOOOOO!!!!!!!\n");
				  }
				  pInputBuffer->nFilledLen = 0;
				  omx_videodec_component_Private->pendingOffset = frameLen;
				  omx_videodec_component_Private->isPendingBuffer = OMX_TRUE;
				  omx_videodec_component_Private->isNewBuffer = 1;
				  break;
			  } else {
				  DEBUG(DEB_LEV_ERR, "Too much!!!!!!!\n");
			  }
		  }
		  omx_videodec_component_Private->avPacket.size = frameLen;
		  omx_videodec_component_Private->avPacket.data = omx_videodec_component_Private->inputCurrBuffer;
	  } else {
		  DEBUG(DEB_LEV_ERR, "omx_videodec_component_Private->pendingOffset %i\n", omx_videodec_component_Private->pendingOffset);
		  omx_videodec_component_Private->avPacket.size = omx_videodec_component_Private->pendingOffset;
		  DEBUG(DEB_LEV_ERR, "omx_videodec_component_Private->avPacket.size %i\n", omx_videodec_component_Private->avPacket.size);
		  omx_videodec_component_Private->avPacket.data = omx_videodec_component_Private->internalInputBuffer;
		  omx_videodec_component_Private->pendingOffset = 0;
		  omx_videodec_component_Private->isPendingBuffer = OMX_FALSE;
		  DEBUG(DEB_LEV_ERR, "Before decode first bytes are: 0x%02x%02x%02x%02x - 0x%02x%02x%02x%02x\n",
				  (*(omx_videodec_component_Private->inputCurrBuffer)),
				  (*(omx_videodec_component_Private->inputCurrBuffer+1)),
				  (*(omx_videodec_component_Private->inputCurrBuffer+2)),
				  (*(omx_videodec_component_Private->inputCurrBuffer+3)),
				  (*(omx_videodec_component_Private->inputCurrBuffer+4)),
				  (*(omx_videodec_component_Private->inputCurrBuffer+5)),
				  (*(omx_videodec_component_Private->inputCurrBuffer+6)),
				  (*(omx_videodec_component_Private->inputCurrBuffer+7)));
	  }
//		  omx_videodec_component_Private->avCodecContext->frame_number++;
	  DEBUG(DEB_LEV_ERR, "Before decode first bytes are: 0x%02x%02x%02x%02x - 0x%02x%02x%02x%02x\n",
			  (*(omx_videodec_component_Private->avPacket.data)),
			  (*(omx_videodec_component_Private->avPacket.data+1)),
			  (*(omx_videodec_component_Private->avPacket.data+2)),
			  (*(omx_videodec_component_Private->avPacket.data+3)),
			  (*(omx_videodec_component_Private->avPacket.data+4)),
			  (*(omx_videodec_component_Private->avPacket.data+5)),
			  (*(omx_videodec_component_Private->avPacket.data+6)),
			  (*(omx_videodec_component_Private->avPacket.data+7)));
	  DEBUG(DEB_LEV_ERR, "Before decode size is %u\n", omx_videodec_component_Private->avPacket.size);
	  DEBUG(DEB_LEV_ERR, "Frame number is %u\n", omx_videodec_component_Private->avCodecContext->frame_number);


	  nLen = avcodec_decode_video2(omx_videodec_component_Private->avCodecContext,
    		omx_videodec_component_Private->avFrame,
    		&got_picture,
    		&(omx_videodec_component_Private->avPacket));

	  if (nLen < 0) {
		  DEBUG(DEB_LEV_ERR, "A general error or simply frame not decoded? nLen = %i\n", nLen);
	  }

	  if ( nLen > 0) {
		  DEBUG(DEB_LEV_ERR, "nLen > 0\n");
		  omx_videodec_component_Private->inputCurrBuffer += nLen;
		  omx_videodec_component_Private->inputCurrLength -= nLen;
		  pInputBuffer->nFilledLen -= nLen;

		  //Buffer is fully consumed. Request for new Input Buffer
		  if(pInputBuffer->nFilledLen == 0) {
			  omx_videodec_component_Private->isNewBuffer = 1;
		  }

		  avpicture_fill (&pic, (unsigned char*)(outputCurrBuffer),
				  omx_videodec_component_Private->eOutFramePixFmt,
				  omx_videodec_component_Private->avCodecContext->width,
				  omx_videodec_component_Private->avCodecContext->height);

		  if ( !imgConvertYuvCtx_dec ) {
        	imgConvertYuvCtx_dec = sws_getContext( omx_videodec_component_Private->avCodecContext->width,
        			omx_videodec_component_Private->avCodecContext->height,
        			omx_videodec_component_Private->avCodecContext->pix_fmt,
        			omx_videodec_component_Private->avCodecContext->width,
        			omx_videodec_component_Private->avCodecContext->height,
        			omx_videodec_component_Private->eOutFramePixFmt, SWS_FAST_BILINEAR, NULL, NULL, NULL );
        }

        sws_scale(imgConvertYuvCtx_dec, (const uint8_t* const*)(omx_videodec_component_Private->avFrame->data),
        		omx_videodec_component_Private->avFrame->linesize, 0,
                omx_videodec_component_Private->avCodecContext->height, pic.data, pic.linesize );

        nSize = avpicture_get_size (omx_videodec_component_Private->eOutFramePixFmt,
                                      omx_videodec_component_Private->avCodecContext->width,
                                      omx_videodec_component_Private->avCodecContext->height);

        DEBUG(DEB_LEV_FULL_SEQ, "nSize=%d,frame linesize=%d,height=%d,pic linesize=%d PixFmt=%d\n",nSize,
                omx_videodec_component_Private->avFrame->linesize[0],
                omx_videodec_component_Private->avCodecContext->height,
                pic.linesize[0],omx_videodec_component_Private->eOutFramePixFmt);
        if(pOutputBuffer->nAllocLen < nSize) {
        	DEBUG(DEB_LEV_ERR, "Ouch!!!! Output buffer Alloc Len %d less than Frame Size %d\n",(int)pOutputBuffer->nAllocLen,nSize);
           	return;
        }

        pOutputBuffer->nFilledLen += nSize;

    } else if (nLen == 0) {
        DEBUG(DEB_LEV_ERR, "nLen == 0\n");
      /**  This condition becomes true when the input buffer has completely be consumed.
        * In this case is immediately switched because there is no real buffer consumption
        */
      pInputBuffer->nFilledLen = 0;
      /** Few bytes may be left in the input buffer but can't generate one output frame.
        *  Request for new Input Buffer
        */
      omx_videodec_component_Private->isNewBuffer = 1;
      pOutputBuffer->nFilledLen = 0;
    } else {
        DEBUG(DEB_LEV_ERR, "nLen < 0\n");
        DEBUG(DEB_LEV_ERR, "Skip frame or pseudo-frame of length %u\n", frameLen);
        omx_videodec_component_Private->inputCurrBuffer += frameLen;
        omx_videodec_component_Private->inputCurrLength -= frameLen;
        pInputBuffer->nFilledLen -= frameLen;
        continue;
    }

    nOutputFilled = 1;
  }
  DEBUG(DEB_LEV_FULL_SEQ, "One output buffer %p nLen=%d is full returning in video decoder\n",
            pOutputBuffer->pBuffer, (int)pOutputBuffer->nFilledLen);
}

OMX_ERRORTYPE omx_videodec_component_SetParameter(
		OMX_HANDLETYPE hComponent,
		OMX_INDEXTYPE nParamIndex,
		OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = hComponent;
  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *port;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  switch(nParamIndex) {
    case OMX_IndexParamPortDefinition:
      {
        eError = omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
        if(eError == OMX_ErrorNone) {
          OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE*)ComponentParameterStructure;
          UpdateFrameSize (openmaxStandComp);
          portIndex = pPortDef->nPortIndex;
          port = (omx_base_video_PortType *)omx_videodec_component_Private->ports[portIndex];
          port->sVideoParam.eColorFormat = port->sPortParam.format.video.eColorFormat;
        }
        break;
      }
    case OMX_IndexParamVideoPortFormat:
      {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
        pVideoPortFormat = ComponentParameterStructure;
        portIndex = pVideoPortFormat->nPortIndex;
        /*Check Structure Header and verify component state*/
        eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if(eError!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError);
          break;
        }
        if (portIndex <= 1) {
          port = (omx_base_video_PortType *)omx_videodec_component_Private->ports[portIndex];
          memcpy(&port->sVideoParam, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
          omx_videodec_component_Private->ports[portIndex]->sPortParam.format.video.eColorFormat = port->sVideoParam.eColorFormat;

          if (portIndex == 1) {
            switch(port->sVideoParam.eColorFormat) {
              case OMX_COLOR_Format24bitRGB888 :
                omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_RGB24;
                break;
              case OMX_COLOR_Format24bitBGR888 :
                omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_BGR24;
                break;
              case OMX_COLOR_Format32bitBGRA8888 :
                omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_BGR32;
                break;
              case OMX_COLOR_Format32bitARGB8888 :
                omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_RGB32;
                break;
              case OMX_COLOR_Format16bitARGB1555 :
                omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_RGB555;
                break;
              case OMX_COLOR_Format16bitRGB565 :
                omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_RGB565;
                break;
              case OMX_COLOR_Format16bitBGR565 :
                omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_BGR565;
                break;
              default:
                omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_YUV420P;
                break;
            }
            UpdateFrameSize (openmaxStandComp);
          }
        } else {
          return OMX_ErrorBadPortIndex;
        }
        break;
      }
    case OMX_IndexParamVideoAvc:
      {
        OMX_VIDEO_PARAM_AVCTYPE *pVideoAvc;
        pVideoAvc = ComponentParameterStructure;
        portIndex = pVideoAvc->nPortIndex;
        eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        if(eError!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError);
          break;
        }
        memcpy(&omx_videodec_component_Private->pVideoAvc, pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        break;
      }
    case OMX_IndexParamStandardComponentRole:
      {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole;
        pComponentRole = ComponentParameterStructure;
        if (omx_videodec_component_Private->state != OMX_StateLoaded && omx_videodec_component_Private->state != OMX_StateWaitForResources) {
          DEBUG(DEB_LEV_ERR, "In %s Incorrect State=%x lineno=%d\n",__func__,omx_videodec_component_Private->state,__LINE__);
          return OMX_ErrorIncorrectStateOperation;
        }

        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) {
          break;
        }

        if (!strcmp((char *)pComponentRole->cRole, VIDEO_DEC_MPEG4_ROLE)) {
          omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG4;
        } else if (!strcmp((char *)pComponentRole->cRole, VIDEO_DEC_H264_ROLE)) {
          omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
        } else {
          return OMX_ErrorBadParameter;
        }
        SetInternalVideoParameters(openmaxStandComp);
        break;
      }
    case OMX_IndexParamVideoMpeg4:
      {
        OMX_VIDEO_PARAM_MPEG4TYPE *pVideoMpeg4;
        pVideoMpeg4 = ComponentParameterStructure;
        portIndex = pVideoMpeg4->nPortIndex;
        eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        if(eError!=OMX_ErrorNone) {
          DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError);
          break;
        }
        if (pVideoMpeg4->nPortIndex == 0) {
          memcpy(&omx_videodec_component_Private->pVideoMpeg4, pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        } else {
          return OMX_ErrorBadPortIndex;
        }
        break;
      }
    default: /*Call the base component function*/
      return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return eError;
}

OMX_ERRORTYPE omx_videodec_component_GetParameter(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nParamIndex,
  OMX_PTR ComponentParameterStructure) {

  omx_base_video_PortType *port;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  OMX_COMPONENTTYPE *openmaxStandComp = hComponent;
  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
    case OMX_IndexParamVideoInit:
      if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE))) != OMX_ErrorNone) {
        break;
      }
      memcpy(ComponentParameterStructure, &omx_videodec_component_Private->sPortTypesParam[OMX_PortDomainVideo], sizeof(OMX_PORT_PARAM_TYPE));
      break;
    case OMX_IndexParamVideoPortFormat:
      {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;
        pVideoPortFormat = ComponentParameterStructure;
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) {
          break;
        }
        if (pVideoPortFormat->nPortIndex <= 1) {
          port = (omx_base_video_PortType *)omx_videodec_component_Private->ports[pVideoPortFormat->nPortIndex];
          memcpy(pVideoPortFormat, &port->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        } else {
          return OMX_ErrorBadPortIndex;
        }
        break;
      }
    case OMX_IndexParamVideoMpeg4:
      {
        OMX_VIDEO_PARAM_MPEG4TYPE *pVideoMpeg4;
        pVideoMpeg4 = ComponentParameterStructure;
        if (pVideoMpeg4->nPortIndex != 0) {
          return OMX_ErrorBadPortIndex;
        }
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE))) != OMX_ErrorNone) {
          break;
        }
        memcpy(pVideoMpeg4, &omx_videodec_component_Private->pVideoMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
        break;
      }
    case OMX_IndexParamVideoAvc:
      {
        OMX_VIDEO_PARAM_AVCTYPE * pVideoAvc;
        pVideoAvc = ComponentParameterStructure;
        if (pVideoAvc->nPortIndex != 0) {
          return OMX_ErrorBadPortIndex;
        }
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_AVCTYPE))) != OMX_ErrorNone) {
          break;
        }
        memcpy(pVideoAvc, &omx_videodec_component_Private->pVideoAvc, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        break;
      }
    case OMX_IndexParamStandardComponentRole:
      {
        OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
        pComponentRole = ComponentParameterStructure;
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) {
          break;
        }
        if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
          strcpy((char *)pComponentRole->cRole, VIDEO_DEC_MPEG4_ROLE);
        } else if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingAVC) {
          strcpy((char *)pComponentRole->cRole, VIDEO_DEC_H264_ROLE);
        } else {
          strcpy((char *)pComponentRole->cRole,"\0");
        }
        break;
      }
    default: /*Call the base component function*/
      return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_videodec_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message) {
  omx_videodec_component_PrivateType* omx_videodec_component_Private = (omx_videodec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err;
  OMX_STATETYPE eCurrentState = omx_videodec_component_Private->state;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateExecuting ) && (omx_videodec_component_Private->state == OMX_StateIdle)) {
      omx_videodec_component_Private->isFirstBuffer = OMX_TRUE;
    }
    else if ((message->messageParam == OMX_StateIdle ) && (omx_videodec_component_Private->state == OMX_StateLoaded)) {
      err = omx_videodec_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Video Decoder Init Failed Error=%x\n",__func__,err);
        return err;
      }
    } else if ((message->messageParam == OMX_StateLoaded) && (omx_videodec_component_Private->state == OMX_StateIdle)) {
      err = omx_videodec_component_Deinit(openmaxStandComp);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Video Decoder Deinit Failed Error=%x\n",__func__,err);
        return err;
      }
    }
  }
  // Execute the base message handling
  err =  omx_base_component_MessageHandler(openmaxStandComp,message);

  if (message->messageType == OMX_CommandStateSet){
   if ((message->messageParam == OMX_StateIdle  ) && (eCurrentState == OMX_StateExecuting)) {
      if (omx_videodec_component_Private->avcodecReady) {
        omx_videodec_component_ffmpegLibDeInit(omx_videodec_component_Private);
        omx_videodec_component_Private->avcodecReady = OMX_FALSE;
      }
    }
  }
  return err;
}
OMX_ERRORTYPE omx_videodec_component_ComponentRoleEnum(
  OMX_HANDLETYPE hComponent,
  OMX_U8 *cRole,
  OMX_U32 nIndex) {

  if (nIndex == 0) {
    strcpy((char *)cRole, VIDEO_DEC_MPEG4_ROLE);
  } else if (nIndex == 1) {
    strcpy((char *)cRole, VIDEO_DEC_H264_ROLE);
  }  else {
    return OMX_ErrorUnsupportedIndex;
  }
  return OMX_ErrorNone;
}

int nextFrameLen(OMX_U8* pBuffer, OMX_U32 nFilledLen, unsigned int* length) {
	unsigned int i;
	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s nFilledLen %u, pBuffer %p\n", __func__, (unsigned int)nFilledLen, pBuffer);

	for (i = 1; i<nFilledLen-3; i++) {
		if (pBuffer[i] == 0x0) {
			if (pBuffer[i+1] == 0x0) {
				if (pBuffer[i+2] == 0x0) {
					if (pBuffer[i+3] == 0x01) {
						DEBUG(DEB_LEV_ERR, "GOTCHA!!\n");
						*length = i;
						return 0;
					}
				}
			}
		}
	}
	*length = nFilledLen;
	return -1;
}

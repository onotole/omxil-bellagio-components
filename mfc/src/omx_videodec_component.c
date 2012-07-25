/**
* @file src/components/mfc/omx_videodec_component.c
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
* $Date: 2008-08-29 06:10:33 +0200 (Fri, 29 Aug 2008) $
* Revision $Rev: 584 $
* Author $Author: pankaj_sen $
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

#include "parser.h"
#include "mfc_func.h"
#include "../samsung-proprietary.h"

/** Maximum Number of Video Component Instances */
#define MAX_COMPONENT_VIDEODEC 4

/** Counter of Video Component Instance*/
static OMX_U32 noVideoDecInstance = 0;

/** The output decoded color format */
#define OUTPUT_DECODED_COLOR_FMT OMX_COLOR_FormatYUV420Planar

/* Default resolution */
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
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", __func__, (int)openmaxStandComp->pComponentPrivate);
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

  /** Setup port allocators */
  inPort->Port_AllocateBuffer = input_port_AllocateBuffer;
  outPort->Port_AllocateBuffer = output_port_AllocateBuffer;

    /** Setup processing callback */
  inPort->Port_SendBufferFunction = input_port_SendBufferFunction;
  outPort->Port_SendBufferFunction = output_port_SendBufferFunction;


  inPort->Port_AllocateTunnelBuffer = input_port_AllocateTunnelBuffer;
  outPort->Port_AllocateTunnelBuffer = output_port_AllocateTunnelBuffer;

  outPort->Port_FreeTunnelBuffer = mfc_output_port_FreeTunnelBuffer;

  outPort->ComponentTunnelRequest = mfc_output_port_ComponentTunnelRequest;

  /** now it's time to know the video coding type of the component */
  if(!strcmp(cComponentName, VIDEO_DEC_MPEG4_NAME)) { 
    omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG4;
  } else if(!strcmp(cComponentName, VIDEO_DEC_H264_NAME)) {
    omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
  } else if(!strcmp(cComponentName, VIDEO_DEC_H263_NAME)) {
      omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingH263;
  } else if(!strcmp(cComponentName, VIDEO_DEC_MPEG2_NAME)) {
      omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG2;
  } else if (!strcmp(cComponentName, VIDEO_DEC_BASE_NAME)) {
    omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingUnused;
  } else {
    // IL client specified an invalid component name 
    return OMX_ErrorInvalidComponentName;
  }  

  if(!omx_videodec_component_Private->avCodecSyncSem) {
    omx_videodec_component_Private->avCodecSyncSem = calloc(1,sizeof(tsem_t));
    if(omx_videodec_component_Private->avCodecSyncSem == NULL) {
      return OMX_ErrorInsufficientResources;
    }
    tsem_init(omx_videodec_component_Private->avCodecSyncSem, 0);
  }

  SetInternalVideoParameters(openmaxStandComp);

  omx_videodec_component_Private->eOutFramePixFmt = PIX_FMT_YUV420P;

  if(omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
    omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  } else if(omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG2) {
        omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
  } else if(omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingH263) {
          omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
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
  omx_videodec_component_Private->BufferMgmtCallback = NULL; // Callback is not used

  omx_videodec_component_Private->BufferMgmtFunction = omx_videodec_BufferMgmtFunction;

  omx_videodec_component_Private->mfcFileHandle = 0;
  omx_videodec_component_Private->mfcInitialized = OMX_FALSE;
  omx_videodec_component_Private->headerParsed = OMX_FALSE;
  omx_videodec_component_Private->mfcInBuffer = 0;
  omx_videodec_component_Private->mfcInBufferOff = 0;
  omx_videodec_component_Private->mfcInBufferSize = 0;
  omx_videodec_component_Private->mfcInBufferFilled = 0;

  omx_videodec_component_Private->mfcOutBufferWidth = 0;
  omx_videodec_component_Private->mfcOutBufferHeight = 0;
  omx_videodec_component_Private->mfcOutBufferCropLeft = 0;
  omx_videodec_component_Private->mfcOutBufferCropTop = 0;
  omx_videodec_component_Private->mfcOutBufferCropWidth = 0;
  omx_videodec_component_Private->mfcOutBufferCropHeight = 0;
  omx_videodec_component_Private->mfcOutBufferMinCount = 1;
  omx_videodec_component_Private->mfcOutBufferCount = 0;

  omx_videodec_component_Private->tunneledOutput = OMX_FALSE;

  /* Initialize H264 parser */
  omx_videodec_component_Private->mfcH264ParserState.gotStart = OMX_FALSE;
  omx_videodec_component_Private->mfcH264ParserState.gotEnd = OMX_FALSE;
  omx_videodec_component_Private->mfcH264ParserState.seekEnd = OMX_FALSE;
  omx_videodec_component_Private->mfcH264ParserState.state = H264_PARSER_NO_CODE;
  omx_videodec_component_Private->mfcH264ParserState.firstSliceCount = 0;
  omx_videodec_component_Private->mfcH264ParserState.headersCount = 0;

  /* Initialize MPEG4 parser */
  omx_videodec_component_Private->mfcMPEG4ParserState.gotStart = OMX_FALSE;
  omx_videodec_component_Private->mfcMPEG4ParserState.gotEnd = OMX_FALSE;
  omx_videodec_component_Private->mfcMPEG4ParserState.seekEnd = OMX_FALSE;
  omx_videodec_component_Private->mfcMPEG4ParserState.state = H264_PARSER_NO_CODE;
  omx_videodec_component_Private->mfcMPEG4ParserState.vopCount = 0;
  omx_videodec_component_Private->mfcMPEG4ParserState.headersCount = 0;
  omx_videodec_component_Private->mfcMPEG4ParserState.shortHeader = 0;

  omx_videodec_component_Private->samsungProprietaryCommunication = OMX_FALSE;

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

  if(omx_videodec_component_Private->avCodecSyncSem) {
    tsem_deinit(omx_videodec_component_Private->avCodecSyncSem); 
    free(omx_videodec_component_Private->avCodecSyncSem);
    omx_videodec_component_Private->avCodecSyncSem = NULL;
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

/** internal function to set codec related parameters in the private type structure 
  */
void SetInternalVideoParameters(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videodec_component_PrivateType* omx_videodec_component_Private;
  omx_base_video_PortType *inPort ; 

  omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;;

  if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingH263) {
        strcpy(omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.cMIMEType,"video/h263");
        omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;

        setHeader(&omx_videodec_component_Private->pVideoH263, sizeof(OMX_VIDEO_PARAM_H263TYPE));

        omx_videodec_component_Private->pVideoH263.bForceRoundingTypeToZero = OMX_FALSE;
        omx_videodec_component_Private->pVideoH263.bPLUSPTYPEAllowed = OMX_FALSE;
        omx_videodec_component_Private->pVideoH263.eLevel = OMX_VIDEO_H263LevelMax;
        omx_videodec_component_Private->pVideoH263.eProfile = OMX_VIDEO_H263ProfileBaseline;
        omx_videodec_component_Private->pVideoH263.nAllowedPictureTypes = 0;
        omx_videodec_component_Private->pVideoH263.nBFrames = 0;
        omx_videodec_component_Private->pVideoH263.nGOBHeaderInterval = 0;
        omx_videodec_component_Private->pVideoH263.nPFrames = 0;
        omx_videodec_component_Private->pVideoH263.nPictureHeaderRepetition = 0;
        omx_videodec_component_Private->pVideoH263.nPortIndex = 0;

        inPort = (omx_base_video_PortType *)omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
        inPort->sVideoParam.eCompressionFormat = OMX_VIDEO_CodingH263;

   } else if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG2) {
      strcpy(omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.cMIMEType,"video/mpeg2");
      omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;

      setHeader(&omx_videodec_component_Private->pVideoMpeg2, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
      omx_videodec_component_Private->pVideoMpeg2.eLevel = OMX_VIDEO_MPEG2LevelMax;
      omx_videodec_component_Private->pVideoMpeg2.eProfile = OMX_VIDEO_MPEG2ProfileSimple;
      omx_videodec_component_Private->pVideoMpeg2.nBFrames = 0;
      omx_videodec_component_Private->pVideoMpeg2.nPFrames = 0;
      omx_videodec_component_Private->pVideoMpeg2.nPortIndex = 0;

      inPort = (omx_base_video_PortType *)omx_videodec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
      inPort->sVideoParam.eCompressionFormat = OMX_VIDEO_CodingMPEG2;

    } else if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
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
  omx_videodec_component_Private->isFirstBuffer = 1;
  omx_videodec_component_Private->isNewBuffer = 1;

  return eError;
}

/** The Deinitialization function of the video decoder  
  */
OMX_ERRORTYPE omx_videodec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_videodec_component_PrivateType* omx_videodec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  if (omx_videodec_component_Private->avcodecReady) {
    omx_videodec_component_MFCDeInit(omx_videodec_component_Private);
    omx_videodec_component_Private->avcodecReady = OMX_FALSE;
  }

  return eError;
} 

/** Executes all the required steps after an output buffer frame-size has changed.
 * This is necessary, because the resolution of the encoded video is not known until
 * the header of the stream is parsed.
 *
 * Also the minimum number of buffers is not known and has to be determined by parsing the header.
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

OMX_ERRORTYPE omx_videodec_component_SetParameter(
OMX_IN  OMX_HANDLETYPE hComponent,
OMX_IN  OMX_INDEXTYPE nParamIndex,
OMX_IN  OMX_PTR ComponentParameterStructure) {

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

        if (!strcmp((char *)pComponentRole->cRole, VIDEO_DEC_MPEG2_ROLE)) {
          omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG2;
        } else if (!strcmp((char *)pComponentRole->cRole, VIDEO_DEC_MPEG4_ROLE)) {
          omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG4;
        } else if (!strcmp((char *)pComponentRole->cRole, VIDEO_DEC_H264_ROLE)) {
          omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
        } else if (!strcmp((char *)pComponentRole->cRole, VIDEO_DEC_H263_ROLE)) {
                  omx_videodec_component_Private->video_coding_type = OMX_VIDEO_CodingH263;
        } else {
          return OMX_ErrorBadParameter;
        }
        SetInternalVideoParameters(openmaxStandComp);
        break;
      }
    case OMX_IndexParamVideoH263:
          {
            OMX_VIDEO_PARAM_MPEG4TYPE *pVideoH263;
            pVideoH263 = ComponentParameterStructure;
            portIndex = pVideoH263->nPortIndex;
            eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoH263, sizeof(OMX_VIDEO_PARAM_H263TYPE));
            if(eError!=OMX_ErrorNone) {
              DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError);
              break;
            }
            if (pVideoH263->nPortIndex == 0) {
              memcpy(&omx_videodec_component_Private->pVideoH263, pVideoH263, sizeof(OMX_VIDEO_PARAM_H263TYPE));
            } else {
              return OMX_ErrorBadPortIndex;
            }
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
    case OMX_IndexParamVideoMpeg2:
          {
            OMX_VIDEO_PARAM_MPEG2TYPE *pVideoMpeg2;
            pVideoMpeg2 = ComponentParameterStructure;
            portIndex = pVideoMpeg2->nPortIndex;
            eError = omx_base_component_ParameterSanityCheck(hComponent, portIndex, pVideoMpeg2, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
            if(eError!=OMX_ErrorNone) {
              DEBUG(DEB_LEV_ERR, "In %s Parameter Check Error=%x\n",__func__,eError);
              break;
            }
            if (pVideoMpeg2->nPortIndex == 0) {
              memcpy(&omx_videodec_component_Private->pVideoMpeg2, pVideoMpeg2, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
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
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

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
    case OMX_IndexParamVideoH263:
      {
        OMX_VIDEO_PARAM_H263TYPE *pVideoH263;
        pVideoH263 = ComponentParameterStructure;
        if (pVideoH263->nPortIndex != 0) {
          return OMX_ErrorBadPortIndex;
        }
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_H263TYPE))) != OMX_ErrorNone) {
          break;
        }
        memcpy(pVideoH263, &omx_videodec_component_Private->pVideoH263, sizeof(OMX_VIDEO_PARAM_H263TYPE));
        break;
      }
    case OMX_IndexParamVideoMpeg2:
      {
        OMX_VIDEO_PARAM_MPEG2TYPE *pVideoMpeg2;
        pVideoMpeg2 = ComponentParameterStructure;
        if (pVideoMpeg2->nPortIndex != 0) {
          return OMX_ErrorBadPortIndex;
        }
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE))) != OMX_ErrorNone) {
          break;
        }
        memcpy(pVideoMpeg2, &omx_videodec_component_Private->pVideoMpeg2, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
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
        if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG2) {
	  strcpy((char *)pComponentRole->cRole, VIDEO_DEC_MPEG2_ROLE);
        } else if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
          strcpy((char *)pComponentRole->cRole, VIDEO_DEC_MPEG4_ROLE);
        } else if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingAVC) {
          strcpy((char *)pComponentRole->cRole, VIDEO_DEC_H264_ROLE);
        } else if (omx_videodec_component_Private->video_coding_type == OMX_VIDEO_CodingH263) {
                  strcpy((char *)pComponentRole->cRole, VIDEO_DEC_H263_ROLE);
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
      if (!omx_videodec_component_Private->avcodecReady) {
        err = omx_videodec_component_MFCInit(omx_videodec_component_Private);
        if (err != OMX_ErrorNone) {
          return OMX_ErrorNotReady;
        }
        omx_videodec_component_Private->avcodecReady = OMX_TRUE;
      }
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
        omx_videodec_component_MFCDeInit(omx_videodec_component_Private);
        omx_videodec_component_Private->avcodecReady = OMX_FALSE;
      }
    }
  }
  return err;
}

OMX_ERRORTYPE omx_videodec_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex) {

  if (nIndex == 0) {
    strcpy((char *)cRole, VIDEO_DEC_MPEG4_ROLE);
  } else if (nIndex == 1) {
    strcpy((char *)cRole, VIDEO_DEC_H264_ROLE);
  } else if (nIndex == 2) {
      strcpy((char *)cRole, VIDEO_DEC_MPEG2_ROLE);
  } else if (nIndex == 3) {
      strcpy((char *)cRole, VIDEO_DEC_H263_ROLE);
  }  else {
    return OMX_ErrorUnsupportedIndex;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE output_port_AllocateBuffer(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE** pBuffer,
  OMX_U32 nPortIndex,
  OMX_PTR pAppPrivate,
  OMX_U32 nSizeBytes) {

	OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
	omx_videodec_component_PrivateType*  omx_base_component_Private = (omx_videodec_component_PrivateType*)omxComponent->pComponentPrivate;

	omx_base_component_Private->tunneledOutput = OMX_FALSE;

	DEBUG(DEB_LEV_FUNCTION_NAME,"In  %s (this is the one)\n",__func__);
	return base_port_AllocateBuffer(openmaxStandPort, pBuffer, nPortIndex, pAppPrivate, nSizeBytes);

}

OMX_ERRORTYPE input_port_AllocateBuffer(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE** pBuffer,
  OMX_U32 nPortIndex,
  OMX_PTR pAppPrivate,
  OMX_U32 nSizeBytes) {

	DEBUG(DEB_LEV_FUNCTION_NAME,"In  %s (not this)\n",__func__);
	return base_port_AllocateBuffer(openmaxStandPort, pBuffer, nPortIndex, pAppPrivate, nSizeBytes);

}

/* Allocate tunnel buffers - copied from omx_base_port.c and modified */
OMX_ERRORTYPE mfc_output_port_AllocateTunnelBuffer(
		omx_base_PortType *openmaxStandPort,
		OMX_U32 nPortIndex)
{
  unsigned int i;
  OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
  omx_videodec_component_PrivateType* omx_base_component_Private = (omx_videodec_component_PrivateType*)omxComponent->pComponentPrivate;
  OMX_U8* pBuffer=NULL;
  OMX_ERRORTYPE eError=OMX_ErrorNone,err;
  int errQue;
  OMX_U32 numRetry=0,nBufferSize;
  OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
  OMX_U32 nLocalBufferCountActual;

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

      if (!omx_base_component_Private->headerParsed) {
	  pBuffer = calloc(1, 1);
	  nBufferSize = 1;
      } else {
	  //pBuffer = (OMX_U8 *)&omx_base_component_Private->mfcSamsungProprietaryBuffers[i];
	  pBuffer = calloc(1, sizeof(SAMSUNG_NV12MT_BUFFER));
	  DEBUG(DEB_LEV_FULL_SEQ, "Requested size: %d (i=%d, cnt=%d, p=%p)\n", (unsigned int)nBufferSize, i, (unsigned int)omx_base_component_Private->mfcOutBufferCount, pBuffer);
	  nBufferSize = sizeof(SAMSUNG_NV12MT_BUFFER);
      }

      if(pBuffer==NULL) {
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
	   * some retrials, or other checking. For the moment this is a critical error
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

/* Free tunnel buffers - copied from omx_base_port.c and modified */
OMX_ERRORTYPE mfc_output_port_FreeTunnelBuffer(omx_base_PortType *openmaxStandPort,OMX_U32 nPortIndex)
{
  unsigned int i;
  OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
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
        free(openmaxStandPort->pInternalBufferStorage[i]->pBuffer);
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
        //tsem_up(openmaxStandPort->pAllocSem);
      }
    }
  }
  DEBUG(DEB_LEV_FUNCTION_NAME, "Out of %s for port %p Qelem=%d BSem=%d\n", __func__, openmaxStandPort,
							  openmaxStandPort->pBufferQueue->nelem, openmaxStandPort->pBufferSem->semval);
  return OMX_ErrorNone;
}

/* This function acts as a wrapper for the mfc_output_port_AllocateTunnelBuffer */
OMX_ERRORTYPE output_port_AllocateTunnelBuffer(
		omx_base_PortType *openmaxStandPort,
		OMX_U32 nPortIndex)
{
	OMX_COMPONENTTYPE* openmaxStandComp = openmaxStandPort->standCompContainer;
	omx_videodec_component_PrivateType* omx_videodec_component_Private = (omx_videodec_component_PrivateType*)openmaxStandComp->pComponentPrivate;

	DEBUG(DEB_LEV_FUNCTION_NAME, "In  %s\n",__func__);

	OMX_PARAM_PORTDEFINITIONTYPE omx_PortDefinition;

	setHeader(&omx_PortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	omx_PortDefinition.nPortIndex = 1;
	omx_videodec_component_GetParameter(openmaxStandPort->standCompContainer, OMX_IndexParamPortDefinition , &omx_PortDefinition);
	omx_PortDefinition.nBufferCountActual = omx_videodec_component_Private->mfcOutBufferCount;
	omx_videodec_component_SetParameter(openmaxStandPort->standCompContainer, OMX_IndexParamPortDefinition , &omx_PortDefinition);


	setHeader(&omx_PortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	omx_PortDefinition.nPortIndex = 0;
	omx_videodec_component_GetParameter(openmaxStandPort->hTunneledComponent, OMX_IndexParamPortDefinition , &omx_PortDefinition);
	omx_PortDefinition.nBufferCountActual = omx_videodec_component_Private->mfcOutBufferCount;
	omx_videodec_component_SetParameter(openmaxStandPort->hTunneledComponent, OMX_IndexParamPortDefinition , &omx_PortDefinition);

	DEBUG(DEB_LEV_FUNCTION_NAME, "Changing number of buffers to %d (from %d)\n",
			(unsigned int)omx_videodec_component_Private->mfcOutBufferCount,
			(unsigned int)openmaxStandPort->sPortParam.nBufferCountActual);

	return mfc_output_port_AllocateTunnelBuffer(openmaxStandPort, nPortIndex);
}

/* In case of input the tunneled buffers are allocated with the base_port function.
 * This wrapper has been added to display additional debug messages. */
OMX_ERRORTYPE input_port_AllocateTunnelBuffer(
		omx_base_PortType *openmaxStandPort,
		OMX_U32 nPortIndex)
{
	DEBUG(DEB_LEV_FUNCTION_NAME,"In  %s (not this)\n",__func__);
	return base_port_AllocateTunnelBuffer(openmaxStandPort, nPortIndex);
}

OMX_ERRORTYPE output_port_SendBufferFunction(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  OMX_U32 portIndex;
  OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;
  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
  omx_videodec_component_PrivateType* omx_videodec_component_Private = (omx_videodec_component_PrivateType*)omxComponent->pComponentPrivate;

  DEBUG(DEB_LEV_FUNCTION_NAME,"In  %s (mfc) \n",__func__);

#if NO_GST_OMX_PATCH
  unsigned int i;
#endif
  portIndex = (openmaxStandPort->sPortParam.eDir == OMX_DirInput)?pBuffer->nInputPortIndex:pBuffer->nOutputPortIndex;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s portIndex %lu\n", __func__, portIndex);

  if (portIndex != openmaxStandPort->sPortParam.nPortIndex) {
    DEBUG(DEB_LEV_ERR, "In %s: wrong port for this operation portIndex=%d port->portIndex=%d\n", __func__, (int)portIndex, (int)openmaxStandPort->sPortParam.nPortIndex);
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

  if (omx_videodec_component_Private->samsungProprietaryCommunication && portIndex == OMX_BASE_FILTER_OUTPUTPORT_INDEX) {
		SAMSUNG_NV12MT_BUFFER *sh;

		sh = (SAMSUNG_NV12MT_BUFFER *)pBuffer->pBuffer;
		TIME("Queueing buffer: %d\n", (int)sh->bufferIndex);
		err = MFCQueueOutputBuffer(omx_videodec_component_Private, sh->bufferIndex);
		if (err != OMX_ErrorNone) {
			DEBUG(DEB_LEV_ERR, "%s:%s:%d: Failed to queue an output buffer\n", __FILE__, __func__, __LINE__);

			DEBUG(DEB_LEV_ERR, "Problem with processing. Aborting.\n");

			omx_videodec_component_Private->state = OMX_StateInvalid;

			(*(omx_videodec_component_Private->callbacks->EventHandler))
				  (omxComponent,
				  omx_videodec_component_Private->callbackData,
				  OMX_EventError, /* The command was completed */
				  OMX_ErrorInvalidState, /* The commands was a OMX_CommandStateSet */
				  0, /* The state has been changed in message->messageParam2 */
				  NULL);

			return err;
		}

		TIME("Queued buffer: %d\n", (int)sh->bufferIndex);
 	}

  /* And notify the buffer management thread we have a fresh new buffer to manage */
  if(!PORT_IS_BEING_FLUSHED(openmaxStandPort) && !(PORT_IS_BEING_DISABLED(openmaxStandPort) && PORT_IS_TUNNELED_N_BUFFER_SUPPLIER(openmaxStandPort))){
    queue(openmaxStandPort->pBufferQueue, pBuffer);
    tsem_up(openmaxStandPort->pBufferSem);
    DEBUG(DEB_LEV_PARAMS, "In %s Signalling bMgmtSem Port Index=%d\n",__func__, (int)portIndex);
    tsem_up(omx_base_component_Private->bMgmtSem);
  }else if(PORT_IS_BUFFER_SUPPLIER(openmaxStandPort)){
    DEBUG(DEB_LEV_FULL_SEQ, "In %s: Comp %s received io:%d buffer\n",
        __func__,omx_base_component_Private->name,(int)openmaxStandPort->sPortParam.nPortIndex);
    queue(openmaxStandPort->pBufferQueue, pBuffer);
    tsem_up(openmaxStandPort->pBufferSem);
  }
  else { // If port being flushed and not tunneled then return error
    DEBUG(DEB_LEV_FULL_SEQ, "In %s \n", __func__);
    return OMX_ErrorIncorrectStateOperation;
  }
  return OMX_ErrorNone;
}


OMX_ERRORTYPE input_port_SendBufferFunction(
  omx_base_PortType *openmaxStandPort,
  OMX_BUFFERHEADERTYPE* pBuffer) {

  OMX_ERRORTYPE err;
  OMX_U32 portIndex;
  OMX_COMPONENTTYPE* omxComponent = openmaxStandPort->standCompContainer;

  DEBUG(DEB_LEV_FUNCTION_NAME,"In  %s (mfc)\n",__func__);

  omx_base_component_PrivateType* omx_base_component_Private = (omx_base_component_PrivateType*)omxComponent->pComponentPrivate;
#if NO_GST_OMX_PATCH
  unsigned int i;
#endif
  portIndex = (openmaxStandPort->sPortParam.eDir == OMX_DirInput)?pBuffer->nInputPortIndex:pBuffer->nOutputPortIndex;
  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s portIndex %lu\n", __func__, portIndex);

  if (portIndex != openmaxStandPort->sPortParam.nPortIndex) {
    DEBUG(DEB_LEV_ERR, "In %s: wrong port for this operation portIndex=%d port->portIndex=%d\n", __func__, (int)portIndex, (int)openmaxStandPort->sPortParam.nPortIndex);
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
    queue(openmaxStandPort->pBufferQueue, pBuffer);
    tsem_up(openmaxStandPort->pBufferSem);
    DEBUG(DEB_LEV_PARAMS, "In %s Signalling bMgmtSem Port Index=%d\n",__func__, (int)portIndex);
    tsem_up(omx_base_component_Private->bMgmtSem);
  }else if(PORT_IS_BUFFER_SUPPLIER(openmaxStandPort)){
    DEBUG(DEB_LEV_FULL_SEQ, "In %s: Comp %s received io:%d buffer\n",
        __func__,omx_base_component_Private->name,(int)openmaxStandPort->sPortParam.nPortIndex);
    queue(openmaxStandPort->pBufferQueue, pBuffer);
    tsem_up(openmaxStandPort->pBufferSem);
  }
  else { // If port being flushed and not tunneled then return error
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
void* omx_videodec_BufferMgmtFunction (void* param) {
  OMX_COMPONENTTYPE* openmaxStandComp = (OMX_COMPONENTTYPE*)param;
  omx_base_component_PrivateType* omx_base_component_Private=(omx_base_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  omx_base_filter_PrivateType* omx_base_filter_Private = (omx_base_filter_PrivateType*)omx_base_component_Private;
  omx_base_PortType *pInPort=(omx_base_PortType *)omx_base_filter_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  omx_base_PortType *pOutPort=(omx_base_PortType *)omx_base_filter_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  tsem_t* pInputSem = pInPort->pBufferSem;
  tsem_t* pOutputSem = pOutPort->pBufferSem;
  queue_t* pInputQueue = pInPort->pBufferQueue;
  queue_t* pOutputQueue = pOutPort->pBufferQueue;
  OMX_BUFFERHEADERTYPE* pOutputBuffer=NULL;
  OMX_BUFFERHEADERTYPE* pInputBuffer=NULL;
  OMX_BOOL isInputBufferNeeded=OMX_TRUE,isOutputBufferNeeded=OMX_TRUE;
  int inBufExchanged=0,outBufExchanged=0;
  int ret;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  while(omx_base_filter_Private->state == OMX_StateIdle || omx_base_filter_Private->state == OMX_StateExecuting ||  omx_base_filter_Private->state == OMX_StatePause ||
    omx_base_filter_Private->transientState == OMX_TransStateLoadedToIdle){

	  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s (mfc) while !\n", __func__);

    /*Wait till the ports are being flushed*/
    pthread_mutex_lock(&omx_base_filter_Private->flush_mutex);
    while( PORT_IS_BEING_FLUSHED(pInPort) ||
           PORT_IS_BEING_FLUSHED(pOutPort)) {
      pthread_mutex_unlock(&omx_base_filter_Private->flush_mutex);

      DEBUG(DEB_LEV_FULL_SEQ, "In %s 1 signalling flush all cond iE=%d,iF=%d,oE=%d,oF=%d iSemVal=%d,oSemval=%d\n",
        __func__,inBufExchanged,isInputBufferNeeded,outBufExchanged,isOutputBufferNeeded,pInputSem->semval,pOutputSem->semval);


      if(isOutputBufferNeeded==OMX_FALSE && PORT_IS_BEING_FLUSHED(pOutPort)) {
        pOutPort->ReturnBufferFunction(pOutPort,pOutputBuffer);
        outBufExchanged--;
        pOutputBuffer=NULL;
        isOutputBufferNeeded=OMX_TRUE;
        DEBUG(DEB_LEV_FULL_SEQ, "Ports are flushing,so returning output buffer\n");
      }

      if(isInputBufferNeeded==OMX_FALSE && PORT_IS_BEING_FLUSHED(pInPort)) {
        pInPort->ReturnBufferFunction(pInPort,pInputBuffer);
        inBufExchanged--;
        pInputBuffer=NULL;
        isInputBufferNeeded=OMX_TRUE;
        DEBUG(DEB_LEV_FULL_SEQ, "Ports are flushing,so returning input buffer\n");
      }

      DEBUG(DEB_LEV_FULL_SEQ, "In %s 2 signaling flush all cond iE=%d,iF=%d,oE=%d,oF=%d iSemVal=%d,oSemval=%d\n",
        __func__,inBufExchanged,isInputBufferNeeded,outBufExchanged,isOutputBufferNeeded,pInputSem->semval,pOutputSem->semval);

      tsem_up(omx_base_filter_Private->flush_all_condition);
      tsem_down(omx_base_filter_Private->flush_condition);
      pthread_mutex_lock(&omx_base_filter_Private->flush_mutex);
    }
    pthread_mutex_unlock(&omx_base_filter_Private->flush_mutex);

    /*No buffer to process. So wait here*/
    if((isInputBufferNeeded==OMX_TRUE && pInputSem->semval==0) &&
      (omx_base_filter_Private->state != OMX_StateLoaded && omx_base_filter_Private->state != OMX_StateInvalid)) {
      //Signalled from EmptyThisBuffer or FillThisBuffer or some thing else
      DEBUG(DEB_LEV_FULL_SEQ, "Waiting for next input/output buffer\n");
      tsem_down(omx_base_filter_Private->bMgmtSem);

    }
    if(omx_base_filter_Private->state == OMX_StateLoaded || omx_base_filter_Private->state == OMX_StateInvalid) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Buffer Management Thread is exiting\n",__func__);
      break;
    }
    if((isOutputBufferNeeded==OMX_TRUE && pOutputSem->semval==0) &&
      (omx_base_filter_Private->state != OMX_StateLoaded && omx_base_filter_Private->state != OMX_StateInvalid) &&
       !(PORT_IS_BEING_FLUSHED(pInPort) || PORT_IS_BEING_FLUSHED(pOutPort))) {
      // Signaled from EmptyThisBuffer or FillThisBuffer or some thing else
      DEBUG(DEB_LEV_FULL_SEQ, "Waiting for next input/output buffer\n");
      tsem_down(omx_base_filter_Private->bMgmtSem);

    }
    if(omx_base_filter_Private->state == OMX_StateLoaded || omx_base_filter_Private->state == OMX_StateInvalid) {
      DEBUG(DEB_LEV_SIMPLE_SEQ, "In %s Buffer Management Thread is exiting\n",__func__);
      break;
    }

    DEBUG(DEB_LEV_SIMPLE_SEQ, "Waiting for input buffer semval=%d \n",pInputSem->semval);
    if(pInputSem->semval>0 && isInputBufferNeeded==OMX_TRUE ) {
      tsem_down(pInputSem);
      if(pInputQueue->nelem>0){
        inBufExchanged++;
        isInputBufferNeeded=OMX_FALSE;
        pInputBuffer = dequeue(pInputQueue);
        if(pInputBuffer == NULL){
          DEBUG(DEB_LEV_ERR, "Had NULL input buffer.\n");
          break;
        }
      }
    }
    /*When we have input buffer to process then get one output buffer*/
    if(pOutputSem->semval>0 && isOutputBufferNeeded==OMX_TRUE) {
      tsem_down(pOutputSem);
      if(pOutputQueue->nelem>0){
        outBufExchanged++;
        isOutputBufferNeeded=OMX_FALSE;
        pOutputBuffer = dequeue(pOutputQueue);

        if(pOutputBuffer == NULL){
          DEBUG(DEB_LEV_ERR, "Had NULL output buffer op is=%d,iq=%d\n",pOutputSem->semval,pOutputQueue->nelem);
          break;
        }
      }
    }

    if(isInputBufferNeeded==OMX_FALSE) {
      if(pInputBuffer->hMarkTargetComponent != NULL){
        if((OMX_COMPONENTTYPE*)pInputBuffer->hMarkTargetComponent ==(OMX_COMPONENTTYPE *)openmaxStandComp) {
          /*Clear the mark and generate an event*/
          (*(omx_base_filter_Private->callbacks->EventHandler))
            (openmaxStandComp,
            omx_base_filter_Private->callbackData,
            OMX_EventMark, /* The command was completed */
            1, /* The commands was a OMX_CommandStateSet */
            0, /* The state has been changed in message->messageParam2 */
            pInputBuffer->pMarkData);
        } else {
          /*If this is not the target component then pass the mark*/
          omx_base_filter_Private->pMark.hMarkTargetComponent = pInputBuffer->hMarkTargetComponent;
          omx_base_filter_Private->pMark.pMarkData            = pInputBuffer->pMarkData;
        }
        pInputBuffer->hMarkTargetComponent = NULL;
      }
    }

    if(isInputBufferNeeded==OMX_FALSE && isOutputBufferNeeded==OMX_FALSE) {

      if(omx_base_filter_Private->pMark.hMarkTargetComponent != NULL){
        pOutputBuffer->hMarkTargetComponent = omx_base_filter_Private->pMark.hMarkTargetComponent;
        pOutputBuffer->pMarkData            = omx_base_filter_Private->pMark.pMarkData;
        omx_base_filter_Private->pMark.hMarkTargetComponent = NULL;
        omx_base_filter_Private->pMark.pMarkData            = NULL;
      }

      pOutputBuffer->nTimeStamp = pInputBuffer->nTimeStamp;
      if(pInputBuffer->nFlags == OMX_BUFFERFLAG_STARTTIME) {
         DEBUG(DEB_LEV_FULL_SEQ, "Detected  START TIME flag in the input buffer filled len=%d\n", (int)pInputBuffer->nFilledLen);
         pOutputBuffer->nFlags = pInputBuffer->nFlags;
         pInputBuffer->nFlags = 0;
      }

      if (pInputBuffer->nFilledLen > 0) {

	  omx_videodec_component_PrivateType *omx_videodec_component_Private = (omx_videodec_component_PrivateType *)omx_base_filter_Private;

	if (omx_videodec_component_Private->headerParsed == OMX_FALSE) {

	  DEBUG(DEB_LEV_FULL_SEQ, "Going to parse the header\n");

		  ret = MFCParseAndQueueInput((omx_videodec_component_PrivateType *)omx_base_filter_Private, pInputBuffer);

		  if (ret != OMX_ErrorNone) {
			  DEBUG(DEB_LEV_ERR, "Failed to parse header\n");

			  omx_base_filter_Private->state = OMX_StateInvalid;
			      // ? tsem_signal(omx_base_component_Private->bStateSem);

		  (*(omx_base_filter_Private->callbacks->EventHandler))
			  (openmaxStandComp,
			  omx_base_filter_Private->callbackData,
			  OMX_EventError, /* The command was completed */
			  OMX_ErrorInvalidState, /* The commands was a OMX_CommandStateSet */
			  0, /* The state has been changed in message->messageParam2 */
			  NULL);

			  return NULL;
		  }

		  if (omx_videodec_component_Private->headerParsed == OMX_TRUE) {

			  DEBUG(DEB_LEV_FULL_SEQ, "After parsing the header\n");
			  // Need to update resolution

			  UpdateFrameSize (openmaxStandComp);

				  /** Send Port Settings changed call back */
				  (*(omx_videodec_component_Private->callbacks->EventHandler))
					(openmaxStandComp,
					 omx_videodec_component_Private->callbackData,
					 OMX_EventPortSettingsChanged, // The command was completed
					 0,  //to adjust the file pointer to resume the correct decode process
					 // ^ above might be wrong, it was nLen in ffmpeg component
					 0, // This is the input port index
					 NULL);
		  }

	  } else {
		  DEBUG(DEB_LEV_FULL_SEQ, "Not the header, parsing frames\n");

		  ret = MFCParseAndQueueInput((omx_videodec_component_PrivateType *)omx_base_filter_Private, pInputBuffer);
		  if (ret != OMX_ErrorNone) {
			  DEBUG(DEB_LEV_ERR, "Problem with processing. Aborting.\n");

			  omx_base_filter_Private->state = OMX_StateInvalid;
			      // tsem_signal(omx_base_component_Private->bStateSem);

		  (*(omx_base_filter_Private->callbacks->EventHandler))
			  (openmaxStandComp,
			  omx_base_filter_Private->callbackData,
			  OMX_EventError, /* The command was completed */
			  OMX_ErrorInvalidState, /* The commands was a OMX_CommandStateSet */
			  0, /* The state has been changed in message->messageParam2 */
			  NULL);

		  return NULL;
		  }

	  }
	} else {
	  // Go input buffer with nFilledLen = 0
	  // If there is some frame parsed in previous pass then we should queue it in MFC
	  ret = MFCParseAndQueueInput((omx_videodec_component_PrivateType *)omx_base_filter_Private, pInputBuffer);
		  if (ret != OMX_ErrorNone) {
			  DEBUG(DEB_LEV_ERR, "Problem with processing. Aborting.\n");

			  omx_base_filter_Private->state = OMX_StateInvalid;
				  // ? tsem_signal(omx_base_component_Private->bStateSem);

			  (*(omx_base_filter_Private->callbacks->EventHandler))
					  (openmaxStandComp,
					  omx_base_filter_Private->callbackData,
					  OMX_EventError, /* The command was completed */
					  OMX_ErrorInvalidState, /* The commands was a OMX_CommandStateSet */
					  0, /* The state has been changed in message->messageParam2 */
					  NULL);

			  return NULL;
		  }
      }

      if(pInputBuffer->nFlags==OMX_BUFFERFLAG_EOS && pInputBuffer->nFilledLen==0) {
        DEBUG(DEB_LEV_FULL_SEQ, "Detected EOS flags in input buffer filled len=%d\n", (int)pInputBuffer->nFilledLen);
        pOutputBuffer->nFlags=pInputBuffer->nFlags;
        pInputBuffer->nFlags=0;
        (*(omx_base_filter_Private->callbacks->EventHandler))
          (openmaxStandComp,
          omx_base_filter_Private->callbackData,
          OMX_EventBufferFlag, /* The command was completed */
          1, /* The commands was a OMX_CommandStateSet */
          pOutputBuffer->nFlags, /* The state has been changed in message->messageParam2 */
          NULL);
      }
      if(omx_base_filter_Private->state==OMX_StatePause && !(PORT_IS_BEING_FLUSHED(pInPort) || PORT_IS_BEING_FLUSHED(pOutPort))) {
        /*Waiting at paused state*/
        tsem_wait(omx_base_component_Private->bStateSem);
      }

      DEBUG(DEB_LEV_FULL_SEQ, "Current buffer to return 0x%x\n", (size_t)pOutputBuffer);

      ret = MFCProcessAndDequeueOutput((struct omx_videodec_component_PrivateType*)omx_base_component_Private, pOutputBuffer);
      if (ret != OMX_ErrorNone) {
	  DEBUG(DEB_LEV_ERR, "Problem with processing. Aborting.\n");

	  omx_base_filter_Private->state = OMX_StateInvalid;
	  // tsem_signal(omx_base_component_Private->bStateSem);

	  (*(omx_base_filter_Private->callbacks->EventHandler))
		  (openmaxStandComp,
		  omx_base_filter_Private->callbackData,
		  OMX_EventError, /* The command was completed */
		  OMX_ErrorInvalidState, /* The commands was a OMX_CommandStateSet */
		  0, /* The state has been changed in message->messageParam2 */
		  NULL);

	  return NULL;

	  // XXX another idea - change flags?
	  // XXX another - restart parsing ?
      }

      /*If EOS and Input buffer Filled Len Zero then Return output buffer immediately*/
      if(pOutputBuffer->nFilledLen!=0 || pOutputBuffer->nFlags==OMX_BUFFERFLAG_EOS) {
        pOutPort->ReturnBufferFunction(pOutPort,pOutputBuffer);
        outBufExchanged--;
        pOutputBuffer=NULL;
        isOutputBufferNeeded=OMX_TRUE;
      }
    }

    if(omx_base_filter_Private->state==OMX_StatePause && !(PORT_IS_BEING_FLUSHED(pInPort) || PORT_IS_BEING_FLUSHED(pOutPort))) {
      /*Waiting at paused state*/
      tsem_wait(omx_base_component_Private->bStateSem);
    }

    /*Input Buffer has been completely consumed. So, return input buffer*/
    if((isInputBufferNeeded == OMX_FALSE) && (pInputBuffer->nFilledLen==0)) {
      pInPort->ReturnBufferFunction(pInPort,pInputBuffer);
      inBufExchanged--;
      pInputBuffer=NULL;
      isInputBufferNeeded=OMX_TRUE;
    }
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ,"Exiting Buffer Management Thread\n");
  return NULL;
}


OMX_ERRORTYPE mfc_output_port_ComponentTunnelRequest(omx_base_PortType* openmaxStandPort, OMX_HANDLETYPE hTunneledComp, OMX_U32 nTunneledPort, OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
	OMX_COMPONENTTYPE *omxTunStandComp = (OMX_COMPONENTTYPE *)hTunneledComp;

	OMX_COMPONENTTYPE* openmaxStandComp = openmaxStandPort->standCompContainer;
	omx_videodec_component_PrivateType* omx_videodec_component_Private = (omx_videodec_component_PrivateType*)openmaxStandComp->pComponentPrivate;

	char componentName[128];
	OMX_STRING MFCName = "OMX.samsung.v4l.video_colorconv";
	OMX_VERSIONTYPE componentVesion, specVersion;
	OMX_UUIDTYPE componentUUID;

	omxTunStandComp->GetComponentVersion(hTunneledComp, componentName, &componentVesion, &specVersion, &componentUUID);

	DEBUG(DEB_LEV_FULL_SEQ, "In %s, pairing with %s\n", __func__, componentName);

	if (strncmp((char*)componentName, (char*)MFCName, strlen((char*)MFCName)) == 0) {
		DEBUG(DEB_LEV_FULL_SEQ, "%s: Pairing with Samsung FIMC (%s)\n", __func__, MFCName);

		omx_videodec_component_Private->samsungProprietaryCommunication = OMX_TRUE;

		pTunnelSetup->nTunnelFlags |= PROPRIETARY_COMMUNICATION_ESTABLISHED;
		openmaxStandPort->sPortParam.nBufferSize = sizeof(SAMSUNG_NV12MT_BUFFER);
	}


	DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
	return base_port_ComponentTunnelRequest(openmaxStandPort, hTunneledComp, nTunneledPort, pTunnelSetup);
}

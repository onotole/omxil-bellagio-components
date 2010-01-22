/**
  src/omx_theoradec_component.c
  
  This component implements a Theora video decoder using libtheora.

  Copyright (C) 2007-2008 STMicroelectronics
  Copyright (C) 2007-2008 Nokia Corporation and/or its subsidiary(-ies)

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

  $Date: 2008-08-29 06:10:33 +0200 (Fri, 29 Aug 2008) $
  Revision $Rev: 584 $
  Author $Author: pankaj_sen $
*/

#include <bellagio/omxcore.h>
#include <bellagio/omx_base_video_port.h>
#include <omx_theoradec_component.h>

/** Maximum Number of Video Component Instance*/
#define MAX_COMPONENT_THEORADEC 4

/** Counter of Video Component Instance*/
static OMX_U32 noVideoDecInstance = 0;

/** The output decoded color format */
#define OUTPUT_DECODED_COLOR_FMT OMX_COLOR_FormatYUV420PackedPlanar

#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 360
/** define the max input buffer size */   
#define DEFAULT_VIDEO_OUTPUT_BUF_SIZE DEFAULT_WIDTH*DEFAULT_HEIGHT*3/2   // YUV 420P 

/** The Constructor of the video decoder component
  * @param openmaxStandComp the component handle to be constructed
  * @param cComponentName is the name of the constructed component
  */
OMX_ERRORTYPE omx_theoradec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName) {

  OMX_ERRORTYPE eError = OMX_ErrorNone;  
  omx_theoradec_component_PrivateType* omx_theoradec_component_Private;
  omx_base_video_PortType *inPort,*outPort;
  OMX_U32 i;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n", __func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_theoradec_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  } else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", __func__, (int)openmaxStandComp->pComponentPrivate);
  }

  omx_theoradec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_theoradec_component_Private->ports = NULL;

  eError = omx_base_filter_Constructor(openmaxStandComp, cComponentName);

  omx_theoradec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nStartPortNumber = 0;
  omx_theoradec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts = 2;

  /** Allocate Ports and call port constructor. */
  if (omx_theoradec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts && !omx_theoradec_component_Private->ports) {
    omx_theoradec_component_Private->ports = calloc(omx_theoradec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts, sizeof(omx_base_PortType *));
    if (!omx_theoradec_component_Private->ports) {
      return OMX_ErrorInsufficientResources;
    }
    for (i=0; i < omx_theoradec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {
      omx_theoradec_component_Private->ports[i] = calloc(1, sizeof(omx_base_video_PortType));
      if (!omx_theoradec_component_Private->ports[i]) {
        return OMX_ErrorInsufficientResources;
      }
    }
  }

  base_video_port_Constructor(openmaxStandComp, &omx_theoradec_component_Private->ports[0], 0, OMX_TRUE);
  base_video_port_Constructor(openmaxStandComp, &omx_theoradec_component_Private->ports[1], 1, OMX_FALSE);

  /** here we can override whatever defaults the base_component constructor set
    * e.g. we can override the function pointers in the private struct  
    */

  /** Domain specific section for the ports.   
    * first we set the parameter common to both formats
    */
  //common parameters related to input port
  inPort = (omx_base_video_PortType *)omx_theoradec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  inPort->sPortParam.nBufferSize = 256000;
  inPort->sPortParam.format.video.xFramerate = 25;

  //common parameters related to output port
  outPort = (omx_base_video_PortType *)omx_theoradec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  outPort->sPortParam.format.video.eColorFormat = OUTPUT_DECODED_COLOR_FMT;
  outPort->sPortParam.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUF_SIZE;
  outPort->sPortParam.format.video.xFramerate = 25;

  /** settings of output port parameter definition */
  outPort->sVideoParam.eColorFormat = OUTPUT_DECODED_COLOR_FMT;
  outPort->sVideoParam.xFramerate = 25;

  SetInternalVideoParameters(openmaxStandComp);

  omx_theoradec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingTheora;

  memset (&omx_theoradec_component_Private->info, 0, sizeof(th_info));
  memset (&omx_theoradec_component_Private->comment, 0, sizeof(th_comment));
  omx_theoradec_component_Private->setup = NULL;
  omx_theoradec_component_Private->BufferMgmtCallback = omx_theoradec_component_BufferMgmtCallback;

  omx_theoradec_component_Private->messageHandler = omx_theoradec_component_MessageHandler;
  omx_theoradec_component_Private->destructor = omx_theoradec_component_Destructor;
  openmaxStandComp->SetParameter = omx_theoradec_component_SetParameter;
  openmaxStandComp->GetParameter = omx_theoradec_component_GetParameter;
  openmaxStandComp->SetConfig    = omx_theoradec_component_SetConfig;
  openmaxStandComp->ComponentRoleEnum = omx_theoradec_component_ComponentRoleEnum;
  openmaxStandComp->GetExtensionIndex = omx_theoradec_component_GetExtensionIndex;

  noVideoDecInstance++;

  if(noVideoDecInstance > MAX_COMPONENT_THEORADEC) {
    return OMX_ErrorInsufficientResources;
  }
  return eError;
}


/** The destructor of the video decoder component
  */
OMX_ERRORTYPE omx_theoradec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_theoradec_component_PrivateType* omx_theoradec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_U32 i;
  
  /* frees port/s */   
  if (omx_theoradec_component_Private->ports) {   
    for (i=0; i < omx_theoradec_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts; i++) {   
      if(omx_theoradec_component_Private->ports[i])   
        omx_theoradec_component_Private->ports[i]->PortDestructor(omx_theoradec_component_Private->ports[i]);   
    }   
    free(omx_theoradec_component_Private->ports);   
    omx_theoradec_component_Private->ports=NULL;   
  } 


  DEBUG(DEB_LEV_FUNCTION_NAME, "Destructor of video decoder component is called\n");

  omx_base_filter_Destructor(openmaxStandComp);
  noVideoDecInstance--;

  return OMX_ErrorNone;
}


/** internal function to set codec related parameters in the private type structure 
  */
void SetInternalVideoParameters(OMX_COMPONENTTYPE *openmaxStandComp) {

  omx_theoradec_component_PrivateType* omx_theoradec_component_Private;
  //omx_base_video_PortType *inPort ; 

  omx_theoradec_component_Private = openmaxStandComp->pComponentPrivate;;

  strcpy(omx_theoradec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.cMIMEType,"video/mpeg4");
  omx_theoradec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX]->sPortParam.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
}


/** The Initialization function of the video decoder
  */
OMX_ERRORTYPE omx_theoradec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp) {

  OMX_ERRORTYPE eError = OMX_ErrorNone;

  return eError;
}

/** The Deinitialization function of the video decoder  
  */
OMX_ERRORTYPE omx_theoradec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp) {

  //omx_theoradec_component_PrivateType* omx_theoradec_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  return eError;
} 

/** Executes all the required steps after an output buffer frame-size has changed.
*/
static inline void UpdateFrameSize(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_theoradec_component_PrivateType* omx_theoradec_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *outPort = (omx_base_video_PortType *)omx_theoradec_component_Private->ports[OMX_BASE_FILTER_OUTPUTPORT_INDEX];
  omx_base_video_PortType *inPort = (omx_base_video_PortType *)omx_theoradec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];
  outPort->sPortParam.format.video.nFrameWidth = inPort->sPortParam.format.video.nFrameWidth;
  outPort->sPortParam.format.video.nFrameHeight = inPort->sPortParam.format.video.nFrameHeight;
  switch(outPort->sVideoParam.eColorFormat) {
    case OMX_COLOR_FormatYUV420PackedPlanar:
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

/** This function is used to process the input buffer and provide one output buffer
  */
void omx_theoradec_component_BufferMgmtCallback(OMX_COMPONENTTYPE *openmaxStandComp, OMX_BUFFERHEADERTYPE* pInputBuffer, OMX_BUFFERHEADERTYPE* pOutputBuffer) {

  omx_theoradec_component_PrivateType* omx_theoradec_component_Private = openmaxStandComp->pComponentPrivate;

  OMX_U8* outputCurrBuffer;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);
  /** Fill up the current input buffer when a new buffer has arrived */

  ogg_packet packet;

  packet.packet = pInputBuffer->pBuffer;
  packet.bytes = pInputBuffer->nFilledLen;
  packet.granulepos = 0;
  packet.packetno = 0;
  packet.b_o_s = 1;
  packet.e_o_s = 0;

  if (omx_theoradec_component_Private->n_headers < 3) {
    int ret;

    ret = th_decode_headerin (&omx_theoradec_component_Private->info,
        &omx_theoradec_component_Private->comment,
        &omx_theoradec_component_Private->setup, &packet);
    if (ret < 0) {
      DEBUG(DEB_LEV_ERR, "Theora headerin returned %d\n", ret);
    }

    omx_theoradec_component_Private->n_headers++;

    if (packet.packet[0] == 0x82) {
      omx_base_video_PortType *inPort = (omx_base_video_PortType *)
        omx_theoradec_component_Private->ports[OMX_BASE_FILTER_INPUTPORT_INDEX];

      DEBUG(DEB_LEV_SIMPLE_SEQ,
          "---->Sending Port Settings Change Event in video decoder\n");

      inPort->sPortParam.format.video.nFrameWidth =
        omx_theoradec_component_Private->info.pic_width;
      inPort->sPortParam.format.video.nFrameHeight =
        omx_theoradec_component_Private->info.pic_height;

      UpdateFrameSize (openmaxStandComp);

      /** Send Port Settings changed call back */
      (*(omx_theoradec_component_Private->callbacks->EventHandler))
        (openmaxStandComp,
         omx_theoradec_component_Private->callbackData,
         OMX_EventPortSettingsChanged, // The command was completed 
         pInputBuffer->nFilledLen,  //to adjust the file pointer to resume the correct decode process
         0, // This is the input port index 
         NULL);

      omx_theoradec_component_Private->decoder = th_decode_alloc (
          &omx_theoradec_component_Private->info,
          omx_theoradec_component_Private->setup);
    }
  } else {
    int ret;
    ogg_int64_t gp;
    th_ycbcr_buffer ycbcr_buf;
    int nSize;

    outputCurrBuffer = pOutputBuffer->pBuffer;
    pOutputBuffer->nFilledLen = 0;
    pOutputBuffer->nOffset = 0;

    nSize = omx_theoradec_component_Private->info.pic_width *
      omx_theoradec_component_Private->info.pic_height * 3 / 2;
    if(pOutputBuffer->nAllocLen < nSize) {
      DEBUG(DEB_LEV_ERR, "Ouch!!!! Output buffer Alloc Len %d less than Frame Size %d\n",(int)pOutputBuffer->nAllocLen,nSize);
exit (1);
      return;
    }

    ret = th_decode_packetin (omx_theoradec_component_Private->decoder,
        &packet, &gp);
    if (ret < 0) {
      DEBUG(DEB_LEV_ERR, "Theora packetin returned %d\n", ret);
    }

    ret = th_decode_ycbcr_out (omx_theoradec_component_Private->decoder,
        ycbcr_buf);
    if (ret < 0) {
      DEBUG(DEB_LEV_ERR, "Theora ycbcr_out returned %d\n", ret);
    }

    {
      int j;
      unsigned char *dest;
      unsigned char *src;
      int width, height;

      dest = pOutputBuffer->pBuffer;
      src = ycbcr_buf[0].data;
      width = omx_theoradec_component_Private->info.pic_width;
      height = omx_theoradec_component_Private->info.pic_height;
      for(j=0;j<height;j++){
        memcpy (dest, src, width);
        dest += width;
        src += ycbcr_buf[0].stride;
      }

      src = ycbcr_buf[1].data;
      width = omx_theoradec_component_Private->info.pic_width/2;
      height = omx_theoradec_component_Private->info.pic_height/2;
      for(j=0;j<height;j++){
        memcpy (dest, src, width);
        dest += width;
        src += ycbcr_buf[1].stride;
      }

      src = ycbcr_buf[2].data;
      width = omx_theoradec_component_Private->info.pic_width/2;
      height = omx_theoradec_component_Private->info.pic_height/2;
      for(j=0;j<height;j++){
        memcpy (dest, src, width);
        dest += width;
        src += ycbcr_buf[2].stride;
      }
    }

    pOutputBuffer->nFilledLen += nSize;

    {
      th_info *info = &omx_theoradec_component_Private->info;
      int frame_number;

      frame_number = gp >> info->keyframe_granule_shift;
      frame_number += gp & ((1<<info->keyframe_granule_shift) - 1);
      pOutputBuffer->nTimeStamp =
        (frame_number * 1000000LL * info->fps_denominator) /
        info->fps_numerator;
    }
  }
  pInputBuffer->nFilledLen = 0;

  DEBUG(DEB_LEV_FULL_SEQ, "One output buffer %x nLen=%d is full returning in video decoder\n", 
            (int)pOutputBuffer->pBuffer, (int)pOutputBuffer->nFilledLen);
}

OMX_ERRORTYPE omx_theoradec_component_SetParameter(
OMX_IN  OMX_HANDLETYPE hComponent,
OMX_IN  OMX_INDEXTYPE nParamIndex,
OMX_IN  OMX_PTR ComponentParameterStructure) {

  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_U32 portIndex;

  /* Check which structure we are being fed and make control its header */
  OMX_COMPONENTTYPE *openmaxStandComp = hComponent;
  omx_theoradec_component_PrivateType* omx_theoradec_component_Private = openmaxStandComp->pComponentPrivate;
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
          port = (omx_base_video_PortType *)omx_theoradec_component_Private->ports[portIndex];
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
          port = (omx_base_video_PortType *)omx_theoradec_component_Private->ports[portIndex];
          memcpy(&port->sVideoParam, pVideoPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
          omx_theoradec_component_Private->ports[portIndex]->sPortParam.format.video.eColorFormat = port->sVideoParam.eColorFormat;

          if (portIndex == 1) {
            UpdateFrameSize (openmaxStandComp);
          }
        } else {
          return OMX_ErrorBadPortIndex;
        }
        break;
      }
    case OMX_IndexParamStandardComponentRole:
      {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole;
        pComponentRole = ComponentParameterStructure;
        if (omx_theoradec_component_Private->state != OMX_StateLoaded && omx_theoradec_component_Private->state != OMX_StateWaitForResources) {
          DEBUG(DEB_LEV_ERR, "In %s Incorrect State=%x lineno=%d\n",__func__,omx_theoradec_component_Private->state,__LINE__);
          return OMX_ErrorIncorrectStateOperation;
        }
  
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) { 
          break;
        }

        SetInternalVideoParameters(openmaxStandComp);
        break;
      }
    default: /*Call the base component function*/
      return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return eError;
}

OMX_ERRORTYPE omx_theoradec_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

  omx_base_video_PortType *port;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  OMX_COMPONENTTYPE *openmaxStandComp = hComponent;
  omx_theoradec_component_PrivateType* omx_theoradec_component_Private = openmaxStandComp->pComponentPrivate;
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
      memcpy(ComponentParameterStructure, &omx_theoradec_component_Private->sPortTypesParam[OMX_PortDomainVideo], sizeof(OMX_PORT_PARAM_TYPE));
      break;    
    case OMX_IndexParamVideoPortFormat:
      {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoPortFormat;  
        pVideoPortFormat = ComponentParameterStructure;
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE))) != OMX_ErrorNone) { 
          break;
        }
        if (pVideoPortFormat->nPortIndex <= 1) {
          port = (omx_base_video_PortType *)omx_theoradec_component_Private->ports[pVideoPortFormat->nPortIndex];
          memcpy(pVideoPortFormat, &port->sVideoParam, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        } else {
          return OMX_ErrorBadPortIndex;
        }
        break;    
      }
    case OMX_IndexParamStandardComponentRole:
      {
        OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
        pComponentRole = ComponentParameterStructure;
        if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) { 
          break;
        }
        strcpy((char *)pComponentRole->cRole, THEORA_DEC_ROLE);
        break;
      }
    default: /*Call the base component function*/
      return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_theoradec_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message) {
  omx_theoradec_component_PrivateType* omx_theoradec_component_Private = (omx_theoradec_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateExecuting ) && (omx_theoradec_component_Private->state == OMX_StateIdle)) {
    } 
    else if ((message->messageParam == OMX_StateIdle ) && (omx_theoradec_component_Private->state == OMX_StateLoaded)) {
      err = omx_theoradec_component_Init(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Video Decoder Init Failed Error=%x\n",__func__,err); 
        return err;
      } 
    } else if ((message->messageParam == OMX_StateLoaded) && (omx_theoradec_component_Private->state == OMX_StateIdle)) {
      err = omx_theoradec_component_Deinit(openmaxStandComp);
      if(err!=OMX_ErrorNone) { 
        DEBUG(DEB_LEV_ERR, "In %s Video Decoder Deinit Failed Error=%x\n",__func__,err); 
        return err;
      } 
    }
  }
  // Execute the base message handling
  err =  omx_base_component_MessageHandler(openmaxStandComp,message);

  return err;
}
OMX_ERRORTYPE omx_theoradec_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex) {

  if (nIndex == 0) {
    strcpy((char *)cRole, THEORA_DEC_ROLE);
  }  else {
    return OMX_ErrorUnsupportedIndex;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_theoradec_component_SetConfig(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nIndex,
  OMX_PTR pComponentConfigStructure) {

  OMX_ERRORTYPE err = OMX_ErrorNone;

  if (pComponentConfigStructure == NULL) {
    return OMX_ErrorBadParameter;
  }
  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting configuration %i\n", nIndex);
  /* Check which structure we are being fed and fill its header */
  switch (nIndex) {
    default: // delegate to superclass
      return omx_base_component_SetConfig(hComponent, nIndex, pComponentConfigStructure);
  }
  return err;
}

OMX_ERRORTYPE omx_theoradec_component_GetExtensionIndex(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_STRING cParameterName,
  OMX_OUT OMX_INDEXTYPE* pIndexType) {

  DEBUG(DEB_LEV_FUNCTION_NAME,"In  %s \n",__func__);

  if(strcmp(cParameterName, "OMX.ST.index.config.videoextradata") == 0) {
	    *pIndexType = OMX_VIDEO_CodingTheora;
  } else {
	    return omx_base_component_GetExtensionIndex(hComponent, cParameterName, pIndexType);
  }
  return OMX_ErrorNone;
}


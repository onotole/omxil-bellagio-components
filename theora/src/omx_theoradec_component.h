/**
  @file src/components/theora/omx_theoradec_component.h
  
  This component implements a Theora decoder using libtheora.

  Copyright (C) 2007-2008 STMicroelectronics
  Copyright (C) 2007-2008 Nokia Corporation and/or its subsidiary(-ies).

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

#ifndef _OMX_THEORADEC_COMPONENT_H_
#define _OMX_THEORADEC_COMPONENT_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <omx_base_filter.h>
#include <string.h>

#include <theora/theoradec.h>


#define THEORA_DEC_NAME "OMX.st.video_decoder.theora"
#define THEORA_DEC_ROLE "video_decoder.theora"

/** Video Decoder component private structure.
  */
DERIVEDCLASS(omx_theoradec_component_PrivateType, omx_base_filter_PrivateType)
#define omx_theoradec_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  th_dec_ctx *decoder; \
  th_info info; \
  th_comment comment; \
  th_setup_info *setup; \
  int n_headers;
ENDCLASS(omx_theoradec_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_theoradec_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_theoradec_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_theoradec_component_Init(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_theoradec_component_Deinit(OMX_COMPONENTTYPE *openmaxStandComp);
OMX_ERRORTYPE omx_theoradec_component_MessageHandler(OMX_COMPONENTTYPE*,internalRequestMessageType*);

void omx_theoradec_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* inputbuffer,
  OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_theoradec_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_theoradec_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_theoradec_component_ComponentRoleEnum(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_OUT OMX_U8 *cRole,
  OMX_IN OMX_U32 nIndex);

void SetInternalVideoParameters(OMX_COMPONENTTYPE *openmaxStandComp);

OMX_ERRORTYPE omx_theoradec_component_SetConfig(
  OMX_HANDLETYPE hComponent,
  OMX_INDEXTYPE nIndex,
  OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_theoradec_component_GetExtensionIndex(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_STRING cParameterName,
  OMX_OUT OMX_INDEXTYPE* pIndexType);

#endif

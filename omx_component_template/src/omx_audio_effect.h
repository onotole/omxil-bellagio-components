/**
  src/omx_audio_effect.h

  OpenMAX generic audio effect on PCM stream.

  Copyright (C) 2007-2009 STMicroelectronics
  Copyright (C) 2007-2009 Nokia Corporation and/or its subsidiary(-ies).

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

  $Date$
  Revision $Rev$
  Author $Author$
*/

#ifndef OMX_AUDIO_EFFECT_H_
#define OMX_AUDIO_EFFECT_H_

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Audio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <bellagio/omxcore.h>
#include <bellagio/omx_base_audio_port.h>
#include <bellagio/omx_base_filter.h>

/** Two port component private structure.
* see the define above
*/
DERIVEDCLASS(omx_audio_effect_component_PrivateType, omx_base_filter_PrivateType)
#define omx_audio_effect_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  /** @param gain the volume gain value */ \
  float gain;
ENDCLASS(omx_audio_effect_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_audio_effect_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);
OMX_ERRORTYPE omx_audio_effect_component_Destructor(OMX_COMPONENTTYPE *openmaxStandComp);

void omx_audio_effect_component_BufferMgmtCallback(
  OMX_COMPONENTTYPE *openmaxStandComp,
  OMX_BUFFERHEADERTYPE* inputbuffer,
  OMX_BUFFERHEADERTYPE* outputbuffer);

OMX_ERRORTYPE omx_audio_effect_component_GetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_audio_effect_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_IN  OMX_PTR ComponentParameterStructure);

OMX_ERRORTYPE omx_audio_effect_component_GetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_INOUT OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE omx_audio_effect_component_SetConfig(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nIndex,
  OMX_IN  OMX_PTR pComponentConfigStructure);

#endif

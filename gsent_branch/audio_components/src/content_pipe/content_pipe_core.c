/**
  @file src/content_pipe/content_pipe_core.c

  This file implements the entry point for the getContentPipe function.
  It checks if the content is a local file or a inet resource.

  Copyright (C) 2007, 2008  STMicroelectronics
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

  $Date$
  Revision $Rev$
  Author $Author$
*/

#include <OMX_Types.h>
#include <OMX_Core.h>
#include <OMX_ContentPipe.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern CPresult file_pipe_Constructor(CP_PIPETYPE* pPipe, CPstring szURI);
extern CPresult inet_pipe_Constructor(CP_PIPETYPE* pPipe, CPstring szURI);

OMX_ERRORTYPE OMX_GetContentPipe(
  OMX_OUT OMX_HANDLETYPE *hPipe,
  OMX_IN OMX_STRING szURI)
{
  OMX_ERRORTYPE err = OMX_ErrorContentPipeCreationFailed;
  CPresult res;

  if(strncmp(szURI, "file", 4) == 0) {
    res = file_pipe_Constructor((CP_PIPETYPE*) hPipe, szURI);
    if(res == 0x00000000)
      err = OMX_ErrorNone;
  }

  else if(strncmp(szURI, "inet", 4) == 0) {
    res = inet_pipe_Constructor((CP_PIPETYPE*) hPipe, szURI);
    if(res == 0x00000000)
      err = OMX_ErrorNone;
  }



  return err;
}


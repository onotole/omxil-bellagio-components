/**
  test/omxjpegdectest.c

  OpenMAX Integration Layer JPEG component test program

  This test program operates the OMX IL jpeg decoder component
  which decodes an jpeg file.
  Input file is given as the first argument and output
  is written to another file.

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

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>

#include <OMX_Core.h>
#include <bellagio/omx_comp_debug_levels.h>
#include "omxjpegdectest.h"

#define _DEBUG

void display_help() {
  printf("\n");
  printf("Usage: omxjpegdectest [-o output_file] [-i] infile\n");
  printf("\n");
  printf("       -o output_file: If this option is specified, the decoded frame is written to output_file\n");
  printf("       -i infile: Input Jpeg File Name\n");
  printf("       -h: Displays this help\n");
  printf("\n");
  exit(1);
}

/* Application private date: should go in the component field (segs...) */
appPrivateType* appPriv;

OMX_U32 filesize,size=BUFFER_SIZE;
OMX_S32 nextBuffer = 0;

int flagIsOutputExpected;
int flagIsInputExpected;
int flagOutputReceived;
int flagInputReceived;

OMX_S32 fd;
FILE *infile ,*outfile;
char *input_file;
static OMX_BOOL bEOS=OMX_FALSE;

int main(int argc, char** argv){
  OMX_S32 argn_dec;
  int data_read;
  OMX_STRING compName="OMX.st.image_decoder.jpeg";
  OMX_ERRORTYPE err;

  OMX_CALLBACKTYPE callbacks = { .EventHandler = jpegDecEventHandler,
                       .EmptyBufferDone = jpegDecEmptyBufferDone,
                       .FillBufferDone = jpegDecFillBufferDone,
  };

  OMX_VERSIONTYPE componentVersion;
  OMX_VERSIONTYPE specVersion;
  OMX_UUIDTYPE uuid;
  char buffer[128];
  OMX_STRING name = buffer;

  /* Initialize application private data */
  appPriv = malloc(sizeof(appPrivateType));

  if(argc < 3){
    display_help();
  } else {
    flagIsOutputExpected = 0;
    flagIsInputExpected = 0;
    flagOutputReceived = 0;
    flagInputReceived = 0;

    argn_dec = 1;
    while (argn_dec<argc) {
      if (*(argv[argn_dec]) =='-') {
        if (flagIsOutputExpected) {
          display_help();
        }
        switch (*(argv[argn_dec]+1)) {
        case 'h':
          display_help();
          break;
        case 'o':
          flagIsOutputExpected = 1;
          break;
        case 'i':
          flagIsInputExpected = 1;
          break;
        default:
          display_help();
        }
      } else {
        if (flagIsOutputExpected) {
          appPriv->output_file = malloc(strlen(argv[argn_dec]) + 1);
          strcpy(appPriv->output_file,argv[argn_dec]);
          flagIsOutputExpected = 0;
          flagOutputReceived = 1;
        } else if(flagIsInputExpected) {
          input_file = malloc(strlen(argv[argn_dec]) + 1);
          strcpy(input_file,argv[argn_dec]);
          flagIsInputExpected = 0;
          flagInputReceived = 1;
        }
      }
      argn_dec++;
    }
   if (!flagInputReceived || !flagOutputReceived) {
      display_help();
    }
    DEBUG(DEFAULT_MESSAGES, "Options selected:\n");
    DEBUG(DEFAULT_MESSAGES, "Decode file %s", input_file);
    DEBUG(DEFAULT_MESSAGES, " to ");
    if (flagOutputReceived) {
      DEBUG(DEFAULT_MESSAGES, " %s\n", appPriv->output_file);
    }

  }

  tsem_init(&appPriv->stateSem, 0);
  tsem_init(&appPriv->eosSem, 0);

  fd = open(input_file, O_RDONLY);
  if(fd < 0){
    DEBUG(DEB_LEV_ERR, "Error in opening input file %s\n", input_file);
    exit(1);
  }

  err = OMX_Init();

  /** Ask the core for a handle to the mpeg2 component
   */
  err = OMX_GetHandle(&appPriv->handle, compName, NULL , &callbacks);

  if(appPriv->handle==NULL) {
    DEBUG(DEB_LEV_ERR,"No %s template found\n",compName);
    exit(1);
    }
  /* Get component information. Name, version, etc
   * Think there is a bug in the prototype here. Shouldnt be
   * a OMX_STRING, not a OMX_STRING* ?
   */
  err = OMX_GetComponentVersion(appPriv->handle,
    name,
    &componentVersion,
    &specVersion,
    &uuid);

  DEBUG(DEB_LEV_PARAMS, "Component name is %s\n", name);
  DEBUG(DEB_LEV_PARAMS, "Spec      version is %i.%i.%i.%i\n",
    specVersion.s.nVersionMajor,
    specVersion.s.nVersionMinor,
    specVersion.s.nRevision,
    specVersion.s.nStep);
  DEBUG(DEB_LEV_PARAMS, "Component version is %i.%i.%i.%i\n",
    componentVersion.s.nVersionMajor,
    componentVersion.s.nVersionMinor,
    componentVersion.s.nRevision,
    componentVersion.s.nStep);


  err = OMX_SendCommand(appPriv->handle, OMX_CommandStateSet, OMX_StateIdle, NULL);

  appPriv->pInBuffer[0]=appPriv->pInBuffer[1]=appPriv->pOutBuffer[0]=appPriv->pOutBuffer[1]=NULL;
  /*Allocate Input and output buffers*/
  err = OMX_AllocateBuffer(appPriv->handle, &(appPriv->pInBuffer[0]), 0, NULL, BUFFER_SIZE);
  err = OMX_AllocateBuffer(appPriv->handle, &(appPriv->pInBuffer[1]), 0, NULL, BUFFER_SIZE);
  err = OMX_AllocateBuffer(appPriv->handle, &(appPriv->pOutBuffer[0]), 1, NULL, FRAME_BUFFER_SIZE);
  appPriv->frame_width=FRAME_WIDTH;
  appPriv->frame_height=FRAME_HEIGHT;
  appPriv->outframe_buffer_size=FRAME_BUFFER_SIZE;
  appPriv->cInBufIndex=0;


  /*Wait till state has been changed*/
  tsem_down(&appPriv->stateSem);

  OMX_FillThisBuffer(appPriv->handle, appPriv->pOutBuffer[0]);

  err = OMX_SendCommand(appPriv->handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  /*Wait till state has been changed*/
  tsem_down(&appPriv->stateSem);

  /* Get input file handle */

  data_read = read(fd, appPriv->pInBuffer[0]->pBuffer, size);
  appPriv->pInBuffer[0]->nFilledLen = size;
  data_read = read(fd, appPriv->pInBuffer[1]->pBuffer, size);
  appPriv->pInBuffer[1]->nFilledLen = size;
  err = OMX_EmptyThisBuffer(appPriv->handle, appPriv->pInBuffer[0]);
  err = OMX_EmptyThisBuffer(appPriv->handle, appPriv->pInBuffer[1]);


  tsem_down(&appPriv->eosSem);

  DEBUG(DEB_LEV_SIMPLE_SEQ,"Finished all buffers filesize=%d \n",(int)filesize);

  err = OMX_SendCommand(appPriv->handle, OMX_CommandStateSet,OMX_StateIdle, NULL);

  /*Wait till state has been changed*/
  tsem_down(&appPriv->stateSem);

  DEBUG(DEB_LEV_SIMPLE_SEQ,"State Transitioned to Idle \n");

  err = OMX_SendCommand(appPriv->handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);

  /*Allocate Input and output buffers*/
  err = OMX_FreeBuffer(appPriv->handle, 0, appPriv->pInBuffer[0]);
  err = OMX_FreeBuffer(appPriv->handle, 0, appPriv->pInBuffer[1]);
  err = OMX_FreeBuffer(appPriv->handle, 1, appPriv->pOutBuffer[0]);

  DEBUG(DEB_LEV_SIMPLE_SEQ,"Wait State Transitioning to Loaded \n");
  /*Wait till state has been changed*/
  tsem_down(&appPriv->stateSem);

  /*Free handle*/
  err = OMX_FreeHandle(appPriv->handle);

  /*De-initialize omx core*/
  err = OMX_Deinit();

  close(fd);
  free(appPriv->output_file);
  free(input_file);

  /*Destroy semaphores*/
  tsem_deinit(&appPriv->stateSem);
  tsem_deinit(&appPriv->eosSem);

  free(appPriv);

  return 0;
}

/* Callbacks implementation */

OMX_ERRORTYPE jpegDecEventHandler(
        OMX_HANDLETYPE hComponent,
        OMX_PTR pAppData,
        OMX_EVENTTYPE eEvent,
        OMX_U32 nData1,
        OMX_U32 nData2,
        OMX_PTR pEventData)
{
  if(eEvent==OMX_EventCmdComplete) {
    DEBUG(DEB_LEV_SIMPLE_SEQ,"Event handler has been called \n");
    /*Increment the semaphore indicating command completed*/
    tsem_up(&appPriv->stateSem);
  }
  else if (eEvent == OMX_EventPortSettingsChanged) {
    if (nData2 == 1) {
      OMX_ERRORTYPE err;
      OMX_PARAM_PORTDEFINITIONTYPE param;
      setHeader(&param, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      param.nPortIndex = 1;
      err = OMX_GetParameter(hComponent, OMX_IndexParamPortDefinition, &param);

      appPriv->frame_width=param.format.video.nFrameWidth;
      appPriv->frame_height=param.format.video.nFrameHeight;

      DEBUG(DEB_LEV_PARAMS,"New width=%d height=%d bs=%d\n",(int)appPriv->frame_width,(int)appPriv->frame_height,(int)param.nBufferSize);
      appPriv->outframe_buffer_size=param.nBufferSize;
    } else if (nData2 == 0) {
      OMX_ERRORTYPE err;
      OMX_PARAM_PORTDEFINITIONTYPE param;
      setHeader(&param, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
      param.nPortIndex = 0;
      err = OMX_GetConfig(hComponent, OMX_IndexParamPortDefinition, &param);
    }
  }
  return OMX_ErrorNone;
}


OMX_ERRORTYPE jpegDecEmptyBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer)
{

  OMX_ERRORTYPE err;
  int data_read;
  static int iBufferDropped=0;
  DEBUG(DEB_LEV_FULL_SEQ, "Hi there, I am in the %s callback.\n", __func__);

  data_read = read(fd, pBuffer->pBuffer, size);
  pBuffer->nFilledLen = data_read;
  pBuffer->nOffset = 0;
  if (data_read <= 0) {
    DEBUG(DEB_LEV_SIMPLE_SEQ, "In the %s no more input data available\n", __func__);
    iBufferDropped++;
    if(iBufferDropped>=2) {
      return OMX_ErrorNone;
    }
    pBuffer->nFilledLen=0;
    pBuffer->nFlags = pBuffer->nFlags | OMX_BUFFERFLAG_EOS;
    bEOS=OMX_TRUE;
    err = OMX_EmptyThisBuffer(hComponent, pBuffer);
    return OMX_ErrorNone;
  }
  pBuffer->nFilledLen = data_read;
  if(!bEOS) {
    DEBUG(DEB_LEV_FULL_SEQ, "Empty buffer %p\n", pBuffer);
    err = OMX_EmptyThisBuffer(hComponent, pBuffer);
  } else {
    DEBUG(DEB_LEV_FULL_SEQ, "In %s Dropping Empty This buffer to Audio Dec\n", __func__);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE jpegDecFillBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer)
{
  OMX_S32 buffer_ts;
  buffer_ts = (OMX_S32)pBuffer->nTickCount;

  /* Output data to file */
  outfile=fopen(appPriv->output_file,"w");

  fwrite(pBuffer->pBuffer,sizeof(OMX_U8),pBuffer->nFilledLen,outfile);

  fclose(outfile);

  /*Signal the condition indicating Fill Buffer Done has been called*/
  tsem_up(&appPriv->eosSem);

  return OMX_ErrorNone;
}

void setHeader(OMX_PTR header, OMX_U32 size)
{
  OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
  *((OMX_U32*)header) = size;
  ver->s.nVersionMajor = VERSIONMAJOR;
  ver->s.nVersionMinor = VERSIONMINOR;
  ver->s.nRevision = VERSIONREVISION;
  ver->s.nStep = VERSIONSTEP;
}

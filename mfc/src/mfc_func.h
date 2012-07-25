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
 * mfc_func.h: MFC related functions header file
 */

#ifndef MFC_FUNC_H_
#define MFC_FUNC_H_

/* Initialize the MFC component */
OMX_ERRORTYPE omx_videodec_component_MFCInit(omx_videodec_component_PrivateType* omx_videodec_component_Private);
/* Deinitalize the MFC component */
void omx_videodec_component_MFCDeInit(omx_videodec_component_PrivateType* omx_videodec_component_Private);
/* Allocate the input buffers (compressed stream, in V4L2 this is the OUTPUT queue */
OMX_ERRORTYPE MFCAllocInputBuffers(omx_videodec_component_PrivateType* omx_videodec_component_Private);
/* Allocate the output buffers (decompressed stream, in V4L2 this is the CAPTURE queue */
OMX_ERRORTYPE MFCAllocOutputBuffers(omx_videodec_component_PrivateType* omx_videodec_component_Private);
/* Process the header of the stream. This is needed to determine the parameters of the movie. */
OMX_ERRORTYPE MFCProcessHeader(omx_videodec_component_PrivateType* omx_videodec_component_Private);
/* Start processing (the decoding process) */
OMX_ERRORTYPE MFCStartProcessing(omx_videodec_component_PrivateType* omx_videodec_component_Private);
/* Read header information. It should be executed after the header has been processed. */
OMX_ERRORTYPE MFCReadHeadInfo(omx_videodec_component_PrivateType* omx_videodec_component_Private);
/* Parse and queue an input - if the header hasn't been parsed then this will extract and queue the header */
OMX_ERRORTYPE MFCParseAndQueueInput(omx_videodec_component_PrivateType* omx_videodec_component_Private, OMX_BUFFERHEADERTYPE* pInputBuffer);
/* Process and dequeue an output buffer. This will be the decompressed frame. */
OMX_ERRORTYPE MFCProcessAndDequeueOutput(omx_videodec_component_PrivateType* omx_videodec_component_Private, OMX_BUFFERHEADERTYPE* pOutputBuffer);
/* Queue an output buffer. Data will be written to the buffer. */
OMX_ERRORTYPE MFCQueueOutputBuffer(omx_videodec_component_PrivateType* omx_videodec_component_Private, OMX_U32 bufferIndex);
/* This function returns OMX_TRUE if an input buffer has been successful processed and it is possible to queue
 * another buffer. It is internally used by MFCParseAndQueueInput. */
OMX_BOOL MFCInputBufferProcessed(omx_videodec_component_PrivateType* omx_videodec_component_Private);

#endif /* MFC_FUNC_H_ */

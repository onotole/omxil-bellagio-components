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
 * parser.h - Parser header file
 *
 */

#ifndef PARSER_H_
#define PARSER_H_

#include "omx_videodec_component.h"

/* MPEG4 parser */
OMX_U32 mpeg4_ParseStream(OMX_U8* pBuffer, OMX_U32 bufferSize, OMX_U8* pOutBuffer,
							OMX_U32 outBufferSize, struct mfc_mpeg4_parser_context *pContext,
							OMX_BOOL *pbPreviousFrameEOF, OMX_U32 *pParserCopied, OMX_BOOL getHeader);

/* H264 parser */
OMX_U32 h264_ParseStream(OMX_U8* pBuffer, OMX_U32 bufferSize, OMX_U8* pOutBuffer,
							OMX_U32 outBufferSize, struct mfc_h264_parser_context *pContext,
							OMX_BOOL *pbPreviousFrameEOF, OMX_U32 *pParserCopied, OMX_BOOL getHeader);

/* MPEG2 parser */
OMX_U32 mpeg2_ParseStream(OMX_U8* pBuffer, OMX_U32 bufferSize, OMX_U8* pOutBuffer,
							OMX_U32 outBufferSize, struct mfc_mpeg4_parser_context *pContext,
							OMX_BOOL *pbPreviousFrameEOF, OMX_U32 *pParserCopied, OMX_BOOL getHeader);

#endif /* PARSER_H_ */

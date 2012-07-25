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
 * parser.c - Parser header file
 *
 */

#include <bellagio/omxcore.h>
#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <bellagio/omx_base_video_port.h>
#include <omx_videodec_component.h>
#include <OMX_Video.h>

#include "parser.h"

/* General note for the MPEG4, H264 and MPEG2 parser
 *
 * A frame (VOP) tag can be decoded on its own and is equal to one video frame.
 *
 * A header tag - it cannot be decoded on its own. Headers preceding a frame should
 * be passed to MFC with that frame.
 */

OMX_U32 mpeg4_ParseStream(OMX_U8* pBuffer, OMX_U32 bufferSize, OMX_U8* pOutBuffer,
		OMX_U32 outBufferSize, struct mfc_mpeg4_parser_context *pContext,
		OMX_BOOL *pbPreviousFrameEOF, OMX_U32 *pParserCopied, OMX_BOOL getHeader)
{
	OMX_U32 bufferConsumed;
	OMX_U8* pBufferOrig;
	OMX_U8 tmp;
	OMX_U32 frameLength;

	pBufferOrig = pBuffer;
	bufferConsumed = 0;
	*pbPreviousFrameEOF = OMX_FALSE;

	while (bufferSize-- > 0) {
		switch (pContext->state) {
		case MPEG4_PARSER_NO_CODE:
			if (*pBuffer == 0x0) {
				pContext->state = MPEG4_PARSER_CODE_0x1;
				pContext->tmpCodeStart = bufferConsumed;
			}
			break;
		case MPEG4_PARSER_CODE_0x1:
			if (*pBuffer == 0x0)
				pContext->state = MPEG4_PARSER_CODE_0x2;
			else
				pContext->state = MPEG4_PARSER_NO_CODE;
			break;
		case MPEG4_PARSER_CODE_0x2:
			if (*pBuffer == 0x1) {
				pContext->state = MPEG4_PARSER_CODE_1x1;
				pContext->fourBytesTag = OMX_FALSE;
			} else if ((*pBuffer & 0xFC) == 0x80) {
				pContext->state = MPEG4_PARSER_NO_CODE;
				/* Short header */
				/* It is only valid if the short header it starts a new frame
				 * Otherwise it should be ignored.
				 */
				if (getHeader && pContext->shortHeader == OMX_FALSE) {
					pContext->lastTag = MPEG4_TAG_HEAD;
					pContext->headersCount++;
					pContext->shortHeader = OMX_TRUE;
				} else if (!pContext->seekEnd || (pContext->seekEnd && pContext->shortHeader == OMX_TRUE)) {
					pContext->lastTag = MPEG4_TAG_VOP;
					pContext->vopCount++;
					pContext->shortHeader = OMX_TRUE;
				}
			} else if (*pBuffer == 0x0) {
				/* another o moves the start of the code by 1 byte */
				pContext->tmpCodeStart++;
			} else {
				pContext->state = MPEG4_PARSER_NO_CODE;
			}
			break;
		case MPEG4_PARSER_CODE_1x1:
			tmp = *pBuffer & 0xF0;
			/* Header */
			if (tmp == 0x00 || tmp == 0x01 || tmp == 0x20 || *pBuffer == 0xb0 ||
				*pBuffer == 0xb2 || *pBuffer == 0xb3 || *pBuffer == 0xb5) {
				pContext->state = MPEG4_PARSER_NO_CODE;
				pContext->lastTag = MPEG4_TAG_HEAD;
				pContext->headersCount++;
			/* Frame */
			} else if (*pBuffer == 0xb6) {
				pContext->state = MPEG4_PARSER_NO_CODE;
				pContext->lastTag = MPEG4_TAG_VOP;
				pContext->vopCount++;
			} else
				pContext->state = MPEG4_PARSER_NO_CODE;
			break;
		}

		if (getHeader == OMX_TRUE && pContext->headersCount >= 1 && pContext->vopCount == 1) {
			pContext->codeEnd = pContext->tmpCodeStart;
			pContext->gotEnd = OMX_TRUE;
			break;
		}

		if (pContext->gotStart == OMX_FALSE && pContext->headersCount == 1 && pContext->vopCount == 0) {
			pContext->codeStart = pContext->tmpCodeStart;
			pContext->gotStart = OMX_TRUE;
		}

		if (pContext->gotStart == OMX_FALSE && pContext->headersCount == 0 && pContext->vopCount == 1) {
			pContext->codeStart = pContext->tmpCodeStart;
			pContext->gotStart = OMX_TRUE;
			pContext->seekEnd = OMX_TRUE;
			pContext->headersCount = 0;
			pContext->vopCount = 0;
		}

		if (pContext->seekEnd == OMX_FALSE && pContext->headersCount > 0 && pContext->vopCount == 1) {
			pContext->seekEnd = OMX_TRUE;
			pContext->headersCount = 0;
			pContext->vopCount = 0;
		}

		if (pContext->seekEnd == OMX_TRUE && (pContext->headersCount > 0 || pContext->vopCount > 0)) {
			pContext->codeEnd = pContext->tmpCodeStart;
			pContext->gotEnd = OMX_TRUE;
			if (pContext->headersCount == 0)
				pContext->seekEnd = OMX_TRUE;
			else
				pContext->seekEnd = OMX_FALSE;
			break;
		}

		pBuffer++;
		bufferConsumed++;
	}

	/* Now since we have a start and an end (of the frame/header or the whole input buffer has been processed
	 * we can copy the contents to the output buffer. */

	*pParserCopied = 0;

	if (pContext->gotEnd == OMX_TRUE) {
		frameLength = pContext->codeEnd;
	} else
		frameLength = bufferConsumed;

	if (pContext->codeStart >= 0) {
		frameLength -= pContext->codeStart;
		pBuffer = pBufferOrig + pContext->codeStart;
	} else { // TODO add if gotStart
		memcpy(pOutBuffer, pContext->bytes, -pContext->codeStart);
		*pParserCopied += -pContext->codeStart;
		pOutBuffer += -pContext->codeStart;
		bufferSize -= -pContext->codeStart;
		pBuffer = pBufferOrig;
	}

	if (pContext->gotStart) {
		// TODO check if there is place for the buffer in the outputBuffer
		memcpy(pOutBuffer, pBuffer, frameLength);
		*pParserCopied += frameLength;

		if (pContext->gotEnd) {
			pContext->codeStart = pContext->codeEnd - bufferConsumed;
			pContext->gotStart = OMX_TRUE;
			pContext->gotEnd = OMX_FALSE;
			*pbPreviousFrameEOF = OMX_TRUE;
			if (pContext->lastTag == MPEG4_TAG_VOP) {
				pContext->seekEnd = OMX_TRUE;
				pContext->vopCount = 0;
				pContext->headersCount = 0;
			} else {
				pContext->seekEnd = OMX_FALSE;
				pContext->vopCount = 0;
				pContext->headersCount = 1;
				pContext->shortHeader = OMX_FALSE;
			}
			memcpy(pContext->bytes, pBufferOrig + pContext->codeEnd, bufferConsumed - pContext->codeEnd);
		} else {
			pContext->codeStart = 0;
			*pbPreviousFrameEOF = OMX_FALSE;
		}
	}

	pContext->tmpCodeStart -= bufferConsumed;
	return bufferConsumed;
}

OMX_U32 h264_ParseStream(OMX_U8* pBuffer, OMX_U32 bufferSize, OMX_U8* pOutBuffer,
		OMX_U32 outBufferSize, struct mfc_h264_parser_context *pContext,
		OMX_BOOL *pbPreviousFrameEOF, OMX_U32 *pParserCopied, OMX_BOOL getHeader)
{
	OMX_U32 bufferConsumed;
	OMX_U8* pBufferOrig;
	OMX_U8 tmp;
	OMX_U32 frameLength;

	pBufferOrig = pBuffer;
	bufferConsumed = 0;
	*pbPreviousFrameEOF = OMX_FALSE;

	while (bufferSize-- > 0) {
		switch (pContext->state) {
		case H264_PARSER_NO_CODE:
			if (*pBuffer == 0x0) {
				pContext->state = H264_PARSER_CODE_0x1;
				pContext->tmpCodeStart = bufferConsumed;
			}
			break;
		case H264_PARSER_CODE_0x1:
			if (*pBuffer == 0x0)
				pContext->state = H264_PARSER_CODE_0x2;
			else
				pContext->state = H264_PARSER_NO_CODE;
			break;
		case H264_PARSER_CODE_0x2:
			if (*pBuffer == 0x0) {
				pContext->state = H264_PARSER_CODE_0x3;
				pContext->fourBytesTag = OMX_TRUE;
			} else if (*pBuffer == 0x1) {
				pContext->state = H264_PARSER_CODE_1x1;
				pContext->fourBytesTag = OMX_FALSE;
			} else {
				pContext->state = H264_PARSER_NO_CODE;
			}
			break;
		case H264_PARSER_CODE_0x3:
			if (*pBuffer == 0x1)
				pContext->state = H264_PARSER_CODE_1x1;
			else if (*pBuffer == 0x0)
				pContext->tmpCodeStart++;
			else
				pContext->state = H264_PARSER_NO_CODE;
			break;
		case H264_PARSER_CODE_1x1:
			tmp = *pBuffer & 0x1F;
			/* Slice */
			if (tmp == 1 || tmp == 5) {
				pContext->state = H264_PARSER_CODE_SLICE;
			/* Header */
			} else if (tmp == 6 || tmp == 7 || tmp == 8) {
				pContext->state = H264_PARSER_NO_CODE;
				pContext->lastTag = H264_TAG_HEAD;
				pContext->headersCount++;
			}
			else
				pContext->state = H264_PARSER_NO_CODE;
			break;
		case H264_PARSER_CODE_SLICE:
				if ((*pBuffer & 0x80) == 0x80) {
					pContext->firstSliceCount++;
					pContext->lastTag = H264_TAG_SLICE;
				}
				pContext->state = H264_PARSER_NO_CODE;
			break;
		}

		if (getHeader == OMX_TRUE && pContext->headersCount >= 1 && pContext->firstSliceCount == 1) {
			pContext->codeEnd = pContext->tmpCodeStart;
			pContext->gotEnd = OMX_TRUE;
			break;
		}

		if (pContext->gotStart == OMX_FALSE && pContext->headersCount == 1 && pContext->firstSliceCount == 0) {
			pContext->codeStart = pContext->tmpCodeStart;
			pContext->gotStart = OMX_TRUE;
		}

		if (pContext->gotStart == OMX_FALSE && pContext->headersCount == 0 && pContext->firstSliceCount == 1) {
			pContext->codeStart = pContext->tmpCodeStart;
			pContext->gotStart = OMX_TRUE;
			pContext->seekEnd = OMX_TRUE;
			pContext->headersCount = 0;
			pContext->firstSliceCount = 0;
		}

		if (pContext->seekEnd == OMX_FALSE && pContext->headersCount > 0 && pContext->firstSliceCount == 1) {
			pContext->seekEnd = OMX_TRUE;
			pContext->headersCount = 0;
			pContext->firstSliceCount = 0;
		}

		if (pContext->seekEnd == OMX_TRUE && (pContext->headersCount > 0 || pContext->firstSliceCount > 0)) {
					pContext->codeEnd = pContext->tmpCodeStart;
					pContext->gotEnd = OMX_TRUE;
					if (pContext->headersCount == 0)
						pContext->seekEnd = OMX_TRUE;
					else
						pContext->seekEnd = OMX_FALSE;
					break;
		}
		pBuffer++;
		bufferConsumed++;
	}

	*pParserCopied = 0;

	if (pContext->gotEnd == OMX_TRUE) {
		frameLength = pContext->codeEnd;
	} else
		frameLength = bufferConsumed;

	if (pContext->codeStart >= 0) {
		frameLength -= pContext->codeStart;
		pBuffer = pBufferOrig + pContext->codeStart;
	} else { // TODO add if gotStart
		memcpy(pOutBuffer, pContext->bytes, -pContext->codeStart);
		*pParserCopied += -pContext->codeStart;
		pOutBuffer += -pContext->codeStart;
		bufferSize -= -pContext->codeStart;
		pBuffer = pBufferOrig;
	}

	if (pContext->gotStart) {
		// TODO check if there is place for the buffer in the outputBuffer
		memcpy(pOutBuffer, pBuffer, frameLength);
		*pParserCopied += frameLength;

		if (pContext->gotEnd) {
			pContext->codeStart = pContext->codeEnd - bufferConsumed;
			pContext->gotStart = OMX_TRUE;
			pContext->gotEnd = OMX_FALSE;
			*pbPreviousFrameEOF = OMX_TRUE;
			if (pContext->lastTag == H264_TAG_SLICE) {
				pContext->seekEnd = OMX_TRUE;
				pContext->firstSliceCount = 0;
				pContext->headersCount = 0;
			} else {
				pContext->seekEnd = OMX_FALSE;
				pContext->firstSliceCount = 0;
				pContext->headersCount = 1;
			}
			memcpy(pContext->bytes, pBufferOrig + pContext->codeEnd, bufferConsumed - pContext->codeEnd);
		} else {
			pContext->codeStart = 0;
			*pbPreviousFrameEOF = OMX_FALSE;
		}
	}

	pContext->tmpCodeStart -= bufferConsumed;
	return bufferConsumed;
}


OMX_U32 mpeg2_ParseStream(OMX_U8* pBuffer, OMX_U32 bufferSize, OMX_U8* pOutBuffer,
		OMX_U32 outBufferSize, struct mfc_mpeg4_parser_context *pContext,
		OMX_BOOL *pbPreviousFrameEOF, OMX_U32 *pParserCopied, OMX_BOOL getHeader)
{
	OMX_U32 bufferConsumed;
	OMX_U8* pBufferOrig;
	OMX_U8 tmp;
	OMX_U32 frameLength;

	pBufferOrig = pBuffer;
	bufferConsumed = 0;
	*pbPreviousFrameEOF = OMX_FALSE;

	while (bufferSize-- > 0) {
		switch (pContext->state) {
		case MPEG4_PARSER_NO_CODE:
			if (*pBuffer == 0x0) {
				pContext->state = MPEG4_PARSER_CODE_0x1;
				pContext->tmpCodeStart = bufferConsumed;
			}
			break;
		case MPEG4_PARSER_CODE_0x1:
			if (*pBuffer == 0x0)
				pContext->state = MPEG4_PARSER_CODE_0x2;
			else
				pContext->state = MPEG4_PARSER_NO_CODE;
			break;
		case MPEG4_PARSER_CODE_0x2:
			if (*pBuffer == 0x1) {
				pContext->state = MPEG4_PARSER_CODE_1x1;
			} else if (*pBuffer == 0x0) {
				/* We still have two zeroes */
				pContext->tmpCodeStart++;
			} else {
				pContext->state = MPEG4_PARSER_NO_CODE;
			}
			break;
		case MPEG4_PARSER_CODE_1x1:
			/* Header */
			if (*pBuffer == 0xb3 || *pBuffer == 0xb8) {
				pContext->state = MPEG4_PARSER_NO_CODE;
				pContext->lastTag = MPEG4_TAG_HEAD;
				pContext->headersCount++;
			/* Video Object Plane ( = Frame ) */
			} else if (*pBuffer == 0x00) {
				pContext->state = MPEG4_PARSER_NO_CODE;
				pContext->lastTag = MPEG4_TAG_VOP;
				pContext->vopCount++;
			} else
				pContext->state = MPEG4_PARSER_NO_CODE;
			break;
		}

		if (getHeader == OMX_TRUE && pContext->headersCount >= 1 && pContext->vopCount == 1) {
			pContext->codeEnd = pContext->tmpCodeStart;
			pContext->gotEnd = OMX_TRUE;
			break;
		}

		if (pContext->gotStart == OMX_FALSE && pContext->headersCount == 1 && pContext->vopCount == 0) {
			pContext->codeStart = pContext->tmpCodeStart;
			pContext->gotStart = OMX_TRUE;
		}

		if (pContext->gotStart == OMX_FALSE && pContext->headersCount == 0 && pContext->vopCount == 1) {
			pContext->codeStart = pContext->tmpCodeStart;
			pContext->gotStart = OMX_TRUE;
			pContext->seekEnd = OMX_TRUE;
			pContext->headersCount = 0;
			pContext->vopCount = 0;
		}

		if (pContext->seekEnd == OMX_FALSE && pContext->headersCount > 0 && pContext->vopCount == 1) {
			pContext->seekEnd = OMX_TRUE;
			pContext->headersCount = 0;
			pContext->vopCount = 0;
		}

		if (pContext->seekEnd == OMX_TRUE && (pContext->headersCount > 0 || pContext->vopCount > 0)) {
			pContext->codeEnd = pContext->tmpCodeStart;
			pContext->gotEnd = OMX_TRUE;
			if (pContext->headersCount == 0)
				pContext->seekEnd = OMX_TRUE;
			else
				pContext->seekEnd = OMX_FALSE;
			break;
		}

		pBuffer++;
		bufferConsumed++;
	}

	*pParserCopied = 0;

	if (pContext->gotEnd == OMX_TRUE) {
		frameLength = pContext->codeEnd;
	} else
		frameLength = bufferConsumed;


	if (pContext->codeStart >= 0) {
		frameLength -= pContext->codeStart;
		pBuffer = pBufferOrig + pContext->codeStart;
	} else { // TODO add if gotStart
		memcpy(pOutBuffer, pContext->bytes, -pContext->codeStart);
		*pParserCopied += -pContext->codeStart;
		pOutBuffer += -pContext->codeStart;
		bufferSize -= -pContext->codeStart;
		pBuffer = pBufferOrig;
	}

	if (pContext->gotStart) {
		// TODO check if there is place for the buffer in the outputBuffer
		memcpy(pOutBuffer, pBuffer, frameLength);
		*pParserCopied += frameLength;

		if (pContext->gotEnd) {
			pContext->codeStart = pContext->codeEnd - bufferConsumed;
			pContext->gotStart = OMX_TRUE;
			pContext->gotEnd = OMX_FALSE;
			*pbPreviousFrameEOF = OMX_TRUE;
			if (pContext->lastTag == MPEG4_TAG_VOP) {
				pContext->seekEnd = OMX_TRUE;
				pContext->vopCount = 0;
				pContext->headersCount = 0;
			} else {
				pContext->seekEnd = OMX_FALSE;
				pContext->vopCount = 0;
				pContext->headersCount = 1;
			}
			memcpy(pContext->bytes, pBufferOrig + pContext->codeEnd, bufferConsumed - pContext->codeEnd);
		} else {
			pContext->codeStart = 0;
			*pbPreviousFrameEOF = OMX_FALSE;
		}
	}

	pContext->tmpCodeStart -= bufferConsumed;
	return bufferConsumed;
}

/*
 * samsung-proprietary.h
 *
 *  Created on: Jan 4, 2012
 *      Author: Kamil Debski <k.debski@samsung.com>
 */

#ifndef SAMSUNGPROPRIETARY_H_
#define SAMSUNGPROPRIETARY_H_

typedef struct {
	OMX_U8 *yPlane;
	OMX_U8 *uvPlane;
	OMX_U32 bufferIndex;
	OMX_U32 yPlaneSize;
	OMX_U32 uvPlaneSize;
} SAMSUNG_NV12MT_BUFFER;

#endif /* SAMSUNGPROPRIETARY_H_ */

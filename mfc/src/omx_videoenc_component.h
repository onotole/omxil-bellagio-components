#ifndef OMX_VIDEOENC_COMPONENT_H
#define OMX_VIDEOENC_COMPONENT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

//#include <OMX_Types.h>
//#include <OMX_Component.h>
//#include <OMX_Core.h>
//#include <pthread.h>
//#include <stdlib.h>
//#include <string.h>
#include <bellagio/omx_base_filter.h>

#define VIDEO_ENC_BASE_NAME "OMX.samsung.video_encoder"
#define VIDEO_ENC_MPEG4_NAME "OMX.samsung.video_encoder.mpeg4"
#define VIDEO_ENC_H264_NAME "OMX.samsung.video_encoder.avc"
#define VIDEO_ENC_H263_NAME "OMX.samsung.video_encoder.h263"
#define VIDEO_ENC_MPEG4_ROLE "video_encoder.mpeg4"
#define VIDEO_ENC_H264_ROLE "video_encoder.avc"
#define VIDEO_ENC_H263_ROLE "video_encoder.h263"

#define MFC_ENC_DEVICE_NAME "/dev/video11"
#define MFC_CAP_OUT_BUF_COUNT 8

#define MFC_STATE_STREAMING 1
#define MFC_STATE_EOI 2

DERIVEDCLASS(omx_videoenc_component_PrivateType, omx_base_filter_PrivateType)
#define omx_videoenc_component_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
  OMX_VIDEO_CODINGTYPE video_coding_type; \
  OMX_S32 mfcFileHandle; \
  OMX_U32 mfcInBufCount; \
  OMX_U8 *mfcInBufAddr[2 * MFC_CAP_OUT_BUF_COUNT]; \
  int mfcInBufBusy[MFC_CAP_OUT_BUF_COUNT]; \
  OMX_U8 *mfcOutBufAddr; \
  int mfcOutBytesUsed; \
  int mfcOutBufBusy; \
  OMX_U32 mfcPlaneSize[2]; \
  OMX_U32 mfcState;
ENDCLASS(omx_videoenc_component_PrivateType)

/* Component private entry points declaration */
OMX_ERRORTYPE omx_videoenc_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName);

#endif

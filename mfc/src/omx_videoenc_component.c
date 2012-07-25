#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <poll.h>
#include <bellagio/omxcore.h>
#include <bellagio/omx_base_video_port.h>

#include "omx_videoenc_component.h"

#undef DEBUG_LEVEL
#define DEBUG_LEVEL 255

#define MFC_MIN_STREAM_SIZE (1024)
#define MFC_MAX_STREAM_SIZE (2*1024*1024)

/** Maximum Number of Video Component Instances */
#define MAX_COMPONENT_VIDEORNC 4

/** Counter of Video Component Instance*/
static OMX_U32 noVideoEncInstance = 0;

OMX_ERRORTYPE omx_videoenc_component_GetParameter(
  OMX_IN OMX_HANDLETYPE hComponent,
  OMX_IN OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

  OMX_COMPONENTTYPE *openmaxStandComp = hComponent;
  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Getting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamStandardComponentRole:
    {
      OMX_PARAM_COMPONENTROLETYPE * pComponentRole;
      pComponentRole = ComponentParameterStructure;
      if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) {
        break;
      }
      if (omx_videoenc_component_Private->video_coding_type == OMX_VIDEO_CodingMPEG4) {
        strcpy((char *)pComponentRole->cRole, VIDEO_ENC_MPEG4_ROLE);
      } else if (omx_videoenc_component_Private->video_coding_type == OMX_VIDEO_CodingAVC) {
        strcpy((char *)pComponentRole->cRole, VIDEO_ENC_H264_ROLE);
      } else if (omx_videoenc_component_Private->video_coding_type == OMX_VIDEO_CodingH263) {
                strcpy((char *)pComponentRole->cRole, VIDEO_ENC_H263_ROLE);
      } else {
        strcpy((char *)pComponentRole->cRole,"\0");
      }
      break;
    }
  default: /*Call the base component function*/
    return omx_base_component_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

static inline void UpdatePorts(OMX_COMPONENTTYPE *openmaxStandComp) {
  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  omx_base_video_PortType *inPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[0];
  omx_base_video_PortType *outPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[1];
  OMX_U32 width = inPort->sPortParam.format.video.nFrameWidth;
  OMX_U32 height = inPort->sPortParam.format.video.nFrameHeight;
  outPort->sPortParam.format.video.nFrameWidth = width;
  outPort->sPortParam.format.video.nFrameHeight = height;

  inPort->sVideoParam.eColorFormat = inPort->sPortParam.format.video.eColorFormat;

  switch(inPort->sVideoParam.eColorFormat) {
    case OMX_COLOR_FormatYUV420Planar:
      if(width && height) {
        inPort->sPortParam.nBufferSize = width * height * 3/2;
      }
      break;
    default:
      if(width && height) {
        inPort->sPortParam.nBufferSize = width * height * 3;
      }
      break;
  }
}

OMX_ERRORTYPE omx_videoenc_component_SetParameter(
  OMX_IN  OMX_HANDLETYPE hComponent,
  OMX_IN  OMX_INDEXTYPE nParamIndex,
  OMX_INOUT OMX_PTR ComponentParameterStructure) {

  OMX_COMPONENTTYPE *openmaxStandComp = hComponent;
  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  if (ComponentParameterStructure == NULL) {
    return OMX_ErrorBadParameter;
  }

  DEBUG(DEB_LEV_SIMPLE_SEQ, "   Setting parameter %i\n", nParamIndex);
  /* Check which structure we are being fed and fill its header */
  switch(nParamIndex) {
  case OMX_IndexParamPortDefinition:
    {
      eError = omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
      if(eError == OMX_ErrorNone) {
        UpdatePorts(openmaxStandComp);
      }
      break;
    }
  case OMX_IndexParamStandardComponentRole:
    {
      OMX_PARAM_COMPONENTROLETYPE *pComponentRole;
      pComponentRole = ComponentParameterStructure;
      if (omx_videoenc_component_Private->state != OMX_StateLoaded && omx_videoenc_component_Private->state != OMX_StateWaitForResources) {
        DEBUG(DEB_LEV_ERR, "In %s Incorrect State=%x lineno=%d\n",__func__,omx_videoenc_component_Private->state,__LINE__);
        return OMX_ErrorIncorrectStateOperation;
      }

      if ((eError = checkHeader(ComponentParameterStructure, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone) {
        break;
      }

      if (!strcmp((char *)pComponentRole->cRole, VIDEO_ENC_MPEG4_ROLE)) {
        omx_videoenc_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG4;
      } else if (!strcmp((char *)pComponentRole->cRole, VIDEO_ENC_H264_ROLE)) {
        omx_videoenc_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
      } else if (!strcmp((char *)pComponentRole->cRole, VIDEO_ENC_H263_ROLE)) {
        omx_videoenc_component_Private->video_coding_type = OMX_VIDEO_CodingH263;
      } else {
        return OMX_ErrorBadParameter;
      }
      break;
    }
  default: /*Call the base component function*/
    return omx_base_component_SetParameter(hComponent, nParamIndex, ComponentParameterStructure);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE mfc_set_codec(omx_videoenc_component_PrivateType* omx_videoenc_component_Private) {
  struct v4l2_format fmt;
  int ret;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  memset(&fmt, 0, sizeof(struct v4l2_format));

  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  fmt.fmt.pix_mp.plane_fmt[0].sizeimage = MFC_MAX_STREAM_SIZE;
  switch(omx_videoenc_component_Private->video_coding_type) {
  case OMX_VIDEO_CodingMPEG4 :
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MPEG4;
    break;
  case OMX_VIDEO_CodingMPEG2 :
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MPEG2;
    break;
  case OMX_VIDEO_CodingAVC :
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
    break;
  case OMX_VIDEO_CodingH263 :
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H263;
    break;
  default:
    DEBUG(DEB_LEV_ERR, "Codecs other than H.264, MPEG-4 and H.263 are not supported -- codec not found\nAnd its number is %d\n", (int)omx_videoenc_component_Private->video_coding_type);
    return OMX_ErrorComponentNotFound;
  }

  ret = ioctl(omx_videoenc_component_Private->mfcFileHandle, VIDIOC_S_FMT, &fmt);
  if (ret != 0) {
    DEBUG(DEB_LEV_ERR, "S_FMT on capture failed\n");
    return OMX_ErrorInsufficientResources;
  }

  return OMX_ErrorNone;
}

inline int align(int v, int a) {
  return ((v + a - 1) / a) * a;
}

OMX_ERRORTYPE mfc_set_format(omx_videoenc_component_PrivateType* omx_videoenc_component_Private) {
  struct v4l2_format fmt;
  int ret;
  omx_base_video_PortType *inPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[0];
  OMX_U32 width = inPort->sPortParam.format.video.nFrameWidth;
  OMX_U32 height = inPort->sPortParam.format.video.nFrameHeight;
  OMX_U32 aWidth = align(width, 16);
  OMX_U32 aHeight = align(height, 16);

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  memset(&fmt, 0, sizeof(struct v4l2_format));

  fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
  fmt.fmt.pix_mp.width = width;
  fmt.fmt.pix_mp.height = height;

  fmt.fmt.pix_mp.num_planes = 2;
  fmt.fmt.pix_mp.plane_fmt[0].bytesperline = aWidth;
  fmt.fmt.pix_mp.plane_fmt[0].sizeimage = align(aWidth * aHeight, 2048);
  fmt.fmt.pix_mp.plane_fmt[1].bytesperline = aWidth;
  fmt.fmt.pix_mp.plane_fmt[1].sizeimage = align(aWidth * align(height >> 1, 8), 2048);

  DEBUG(DEB_LEV_FULL_SEQ, "Set format %dx%d bytesperline=%d, sizes=%d,%d\n", width, height, fmt.fmt.pix_mp.plane_fmt[0].bytesperline, fmt.fmt.pix_mp.plane_fmt[0].sizeimage, fmt.fmt.pix_mp.plane_fmt[1].sizeimage);
  ret = ioctl(omx_videoenc_component_Private->mfcFileHandle, VIDIOC_S_FMT, &fmt);
  DEBUG(DEB_LEV_FULL_SEQ, "Res format %dx%d bytesperline=%d, sizes=%d,%d\n", fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height, fmt.fmt.pix_mp.plane_fmt[0].bytesperline, fmt.fmt.pix_mp.plane_fmt[0].sizeimage, fmt.fmt.pix_mp.plane_fmt[1].sizeimage);

  if (ret != 0) {
    DEBUG(DEB_LEV_ERR, "S_FMT on capture failed\n");
    return OMX_ErrorInsufficientResources;
  }

  omx_videoenc_component_Private->mfcPlaneSize[0] = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
  omx_videoenc_component_Private->mfcPlaneSize[1] = fmt.fmt.pix_mp.plane_fmt[1].sizeimage;

  return OMX_ErrorNone;
}

int v4l_req_bufs(int fd, int isCapture, int count) {
  int ret;
  struct v4l2_requestbuffers reqbuf;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s(fd=%d,port=%d,count=%d)\n", __func__, fd, isCapture, count);

  memset(&reqbuf, 0, sizeof reqbuf);
  reqbuf.count = count;
  reqbuf.type = isCapture ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbuf.memory = V4L2_MEMORY_MMAP;

  ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
  if (ret != 0) {
    DEBUG(DEB_LEV_ERR, "Failed to request %d buffers for device %d:%d", count, fd, isCapture);
    return -1;
  }

  return reqbuf.count;
}

int v4l_stream_set(int fd, int op) {
  int ret1, ret2;
  int buf_type;
  int ctrl = op ? VIDIOC_STREAMON : VIDIOC_STREAMOFF;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s(fd=%d,op=%d)\n", __func__, fd, op);

  buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  ret1 = ioctl(fd, ctrl, &buf_type);

  buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  ret2 = ioctl(fd, ctrl, &buf_type);

  return (ret1 != 0) && (ret2 != 0);
}

OMX_ERRORTYPE mfc_allocOutBuffer(omx_videoenc_component_PrivateType* omx_videoenc_component_Private) {
  int nbufs;
  struct v4l2_buffer qbuf;
  struct v4l2_plane planes[2];
  int ret;
  int fd = omx_videoenc_component_Private->mfcFileHandle;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  nbufs = v4l_req_bufs(fd, 1, 1);
  if (nbufs <= 0) {
    return OMX_ErrorInsufficientResources;
  }

  memset(&qbuf, 0, sizeof qbuf);
  qbuf.index = 0;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory = V4L2_MEMORY_MMAP;
  qbuf.m.planes = planes;
  qbuf.length = 2;

  ret = ioctl(fd, VIDIOC_QUERYBUF, &qbuf);
  if (ret != 0) {
    DEBUG(DEB_LEV_ERR, "Error(%d) querying capture buffer\n", ret);
    return OMX_ErrorInsufficientResources;
  }
  omx_videoenc_component_Private->mfcOutBufAddr = mmap(NULL, qbuf.m.planes[0].length, PROT_READ | PROT_WRITE,MAP_SHARED, fd, qbuf.m.planes[0].m.mem_offset);
  if (MAP_FAILED == omx_videoenc_component_Private->mfcOutBufAddr) {
    DEBUG(DEB_LEV_ERR, "Failed mmap capture buffer\n");
    return OMX_ErrorInsufficientResources;
  }
  DEBUG(DEB_LEV_FULL_SEQ, "Mmaped cap buffer to %p, len=%d\n", omx_videoenc_component_Private->mfcOutBufAddr, qbuf.m.planes[0].length);

  return OMX_ErrorNone;
}

OMX_ERRORTYPE mfc_allocInBuffers(omx_videoenc_component_PrivateType* omx_videoenc_component_Private) {
  int nbufs;
  struct v4l2_buffer qbuf;
  int i;
  struct v4l2_plane planes[2];
  int ret;
  int fd = omx_videoenc_component_Private->mfcFileHandle;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  omx_videoenc_component_Private->mfcInBufCount = v4l_req_bufs(fd, 0, MFC_CAP_OUT_BUF_COUNT);
  if (omx_videoenc_component_Private->mfcInBufCount <= 0) {
    return OMX_ErrorInsufficientResources;
  }

  for (i = 0; i < omx_videoenc_component_Private->mfcInBufCount; ++i) {
    memset(&qbuf, 0, sizeof qbuf);
    qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    qbuf.memory = V4L2_MEMORY_MMAP;
    qbuf.m.planes = planes;
    qbuf.length = 2;
    qbuf.index = i;
    ret = ioctl(fd, VIDIOC_QUERYBUF, &qbuf);
    if (ret != 0) {
      DEBUG(DEB_LEV_ERR, "Error(%d) querying output buffer %i\n", ret, i);
      return OMX_ErrorInsufficientResources;
    }
    omx_videoenc_component_Private->mfcInBufAddr[2 * i] = mmap(NULL, qbuf.m.planes[0].length, PROT_READ | PROT_WRITE,MAP_SHARED, fd, qbuf.m.planes[0].m.mem_offset);
    if (MAP_FAILED == omx_videoenc_component_Private->mfcInBufAddr[2 * i]) {
      DEBUG(DEB_LEV_ERR, "Failed mmap output buffer %d plane 0\n", i);
      return OMX_ErrorInsufficientResources;
    }
    DEBUG(DEB_LEV_FULL_SEQ, "Mmaped out buffer[%d:0] to %p, len=%d\n", i, omx_videoenc_component_Private->mfcInBufAddr[2 * i], qbuf.m.planes[0].length);
    omx_videoenc_component_Private->mfcInBufAddr[2 * i + 1] = mmap(NULL, qbuf.m.planes[1].length, PROT_READ | PROT_WRITE,MAP_SHARED, fd, qbuf.m.planes[1].m.mem_offset);
    if (MAP_FAILED == omx_videoenc_component_Private->mfcInBufAddr[2 * i + 1]) {
      DEBUG(DEB_LEV_ERR, "Failed mmap output buffer %d plane 1\n", i);
      return OMX_ErrorInsufficientResources;
    }
    DEBUG(DEB_LEV_FULL_SEQ, "Mmaped out buffer[%d:1] to %p, len=%d\n", i, omx_videoenc_component_Private->mfcInBufAddr[2 * i + 1], qbuf.m.planes[1].length);
  }
  return OMX_ErrorNone;
}

int mfc_v4l_deq(omx_videoenc_component_PrivateType* omx_videoenc_component_Private, int isCapture) {
  struct v4l2_buffer buf;
  struct v4l2_plane planes[2];
  int ret;
  int fd = omx_videoenc_component_Private->mfcFileHandle;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s(port=%d)\n", __func__, isCapture);

  memset(&buf, 0, sizeof buf);
  buf.type = isCapture ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.m.planes = planes;
  buf.length = 2;

  ret = ioctl(fd, VIDIOC_DQBUF, &buf);
  if (ret != 0) {
    DEBUG(DEB_LEV_ERR, "Failed dequeue buffer\n");
    return -1;
  }

  if (isCapture) {
    omx_videoenc_component_Private->mfcOutBufBusy = 0;
    omx_videoenc_component_Private->mfcOutBytesUsed = buf.m.planes[0].bytesused;
    if (buf.m.planes[0].bytesused == 0) {
      ret = v4l_stream_set(fd, 0);
      if (ret != 0) {
        DEBUG(DEB_LEV_ERR, "Failed stream off\n");
        return -1;
      }
      omx_videoenc_component_Private->mfcState &= ~MFC_STATE_STREAMING;
    }
  } else {
    omx_videoenc_component_Private->mfcInBufBusy[buf.index] = 0;
  }

  DEBUG(DEB_LEV_FUNCTION_NAME, "Out %s(port=%d), idx=%d, bytesused=%d\n", __func__, isCapture, buf.index, buf.m.planes[0].bytesused);

  return buf.index;
}

int mfc_v4l_enq(omx_videoenc_component_PrivateType* omx_videoenc_component_Private, int isCapture, int idx, int isEOS) {
  struct v4l2_buffer buf;
  struct v4l2_plane planes[2];
  int ret;
  int fd = omx_videoenc_component_Private->mfcFileHandle;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s(port=%d,idx=%d,isEOS=%d)\n", __func__, isCapture, idx, isEOS);

  memset(&buf, 0, sizeof buf);
  memset(planes, 0, sizeof planes);
  buf.type = isCapture ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = idx;
  buf.m.planes = planes;
  buf.length = 2;

  if (!isCapture) {
    if (!isEOS) {
        planes[0].bytesused = omx_videoenc_component_Private->mfcPlaneSize[0];
        planes[1].bytesused = omx_videoenc_component_Private->mfcPlaneSize[1];
    } else {
	    planes[0].bytesused = 0;
	    planes[1].bytesused = 0;
    }
  }

  ret = ioctl(fd, VIDIOC_QBUF, &buf);
  if (ret != 0) {
    DEBUG(DEB_LEV_ERR, "Failed enqueue buffer\n");
    return -1;
  }

  if (!isCapture) {
    omx_videoenc_component_Private->mfcInBufBusy[idx] = 1;
    if (!(omx_videoenc_component_Private->mfcState & MFC_STATE_STREAMING)) {
      ret = v4l_stream_set(fd, 1);
      if (ret != 0) {
        DEBUG(DEB_LEV_ERR, "Failed stream on\n");
        return -1;
      }
      omx_videoenc_component_Private->mfcState |= MFC_STATE_STREAMING;
    }
  } else {
    omx_videoenc_component_Private->mfcOutBufBusy = 1;
  }

  return idx;
}

OMX_ERRORTYPE mfc_open(omx_videoenc_component_PrivateType* omx_videoenc_component_Private) {
  OMX_ERRORTYPE ret;
  int ret2;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  omx_videoenc_component_Private->mfcFileHandle = open(MFC_ENC_DEVICE_NAME, O_RDWR, 0);
  if (omx_videoenc_component_Private->mfcFileHandle < 0) {
    DEBUG(DEB_LEV_ERR, "Cannot open MFC device %s\n", MFC_ENC_DEVICE_NAME);
    return OMX_ErrorInsufficientResources;
  }

  ret = mfc_set_format(omx_videoenc_component_Private);
  if (ret != OMX_ErrorNone)
    return ret;
  ret = mfc_set_codec(omx_videoenc_component_Private);
  if (ret != OMX_ErrorNone)
    return ret;
  ret = mfc_allocInBuffers(omx_videoenc_component_Private);
  if (ret != OMX_ErrorNone)
    return ret;
  ret = mfc_allocOutBuffer(omx_videoenc_component_Private);
  if (ret != OMX_ErrorNone)
    return ret;
  ret2 = mfc_v4l_enq(omx_videoenc_component_Private, 1, 0, 0);
  if (ret2 < 0)
    return OMX_ErrorHardware;

  return ret;
}

OMX_ERRORTYPE mfc_close(omx_videoenc_component_PrivateType* omx_videoenc_component_Private) {
  int i;
  omx_base_video_PortType *inPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[0];
  OMX_U32 aWidth = align(inPort->sPortParam.format.video.nFrameWidth, 16);
  OMX_U32 aHeight = align(inPort->sPortParam.format.video.nFrameHeight, 16);
  OMX_U32 lumaSize = align(aWidth * aHeight, 2048);
  OMX_U32 chromaSize = align(aWidth * align(inPort->sPortParam.format.video.nFrameHeight >> 1, 8), 2048);

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  for (i = 0; i < omx_videoenc_component_Private->mfcInBufCount; ++i) {
    munmap(omx_videoenc_component_Private->mfcInBufAddr[2 * i], lumaSize);
    munmap(omx_videoenc_component_Private->mfcInBufAddr[2 * i + 1], chromaSize);
  }
  munmap(omx_videoenc_component_Private->mfcOutBufAddr, MFC_MAX_STREAM_SIZE);
  close(omx_videoenc_component_Private->mfcFileHandle);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE mfc_start(omx_videoenc_component_PrivateType* omx_videoenc_component_Private) {
  return OMX_ErrorNone;
}

OMX_ERRORTYPE mfc_stop(omx_videoenc_component_PrivateType* omx_videoenc_component_Private) {
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_videoenc_component_MessageHandler(OMX_COMPONENTTYPE* openmaxStandComp,internalRequestMessageType *message) {
  omx_videoenc_component_PrivateType* omx_videoenc_component_Private = (omx_videoenc_component_PrivateType*)openmaxStandComp->pComponentPrivate;
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_STATETYPE eCurrentState = omx_videoenc_component_Private->state;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s\n", __func__);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateExecuting ) && (eCurrentState == OMX_StateIdle)) {
      err = mfc_start(omx_videoenc_component_Private);
      if (err != OMX_ErrorNone) {
        return OMX_ErrorNotReady;
      }
    }
    else if ((message->messageParam == OMX_StateIdle ) && (eCurrentState == OMX_StateLoaded)) {
      omx_videoenc_component_Private->mfcFileHandle = -1;
      err = mfc_open(omx_videoenc_component_Private);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Video Encoder Init Failed Error=%x\n",__func__,err);
        return err;
      }
    } else if ((message->messageParam == OMX_StateLoaded) && (eCurrentState == OMX_StateIdle)) {
      err = mfc_close(omx_videoenc_component_Private);
      if(err!=OMX_ErrorNone) {
        DEBUG(DEB_LEV_ERR, "In %s Video Encoder Deinit Failed Error=%x\n",__func__,err);
        return err;
      }
    }
  }
  // Execute the base message handling
  err =  omx_base_component_MessageHandler(openmaxStandComp,message);

  if (message->messageType == OMX_CommandStateSet){
    if ((message->messageParam == OMX_StateIdle  ) && (eCurrentState == OMX_StateExecuting)) {
     mfc_stop(omx_videoenc_component_Private);
    }
  }
  return err;
}

int mfc_poll_status(int fd, int timeout)
{
  struct pollfd p;
  int ret;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s(%d,%d)\n", __func__, fd, timeout);

  p.fd = fd;
  p.events = POLLIN | POLLOUT;
  ret = poll(&p, 1, timeout);
  if (ret < 0) {
    DEBUG(DEB_LEV_ERR, "Error(%d) while polling MFC", ret);
    return 0;
  }

  return p.revents;
}

void convertYUV420PtoNV12M(OMX_U8 *yuvBuf, OMX_U8 *nvBuf[2], OMX_U32 width, OMX_U32 height)
{
  OMX_U32 i, j;
  OMX_U8 *pYuv, *pYuv2;
  OMX_U8 *pNv;
  OMX_U32 nvSkip;
  int halfWidth = width >> 1;
  int halfHeight = height >> 1;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s(%p,%p:%p,%ux%u)\n", __func__, yuvBuf, nvBuf[0], nvBuf[1], width, height);

  pYuv = yuvBuf;
  pNv = nvBuf[0];
  nvSkip = align(width, 16);

  for (i = 0; i < height; ++i) {
    memcpy(pNv, pYuv, width);
    pYuv += width;
    pNv += nvSkip;
  }

  pNv = nvBuf[1];
  pYuv2 = pYuv + halfWidth * halfHeight;
  nvSkip -= width;
  for (i = 0; i < halfHeight; ++i) {
    for (j = 0; j < halfWidth; ++j) {
      *pNv++ = *pYuv++;
      *pNv++ = *pYuv2++;
    }
    pNv += nvSkip;
  }
}

void omx_videoenc_component_BufferMgmtCallback(OMX_COMPONENTTYPE* openmaxStandComp, OMX_BUFFERHEADERTYPE* inBuffer, OMX_BUFFERHEADERTYPE* outBuffer)
{
  omx_videoenc_component_PrivateType *omx_videoenc_component_Private = (omx_videoenc_component_PrivateType *)openmaxStandComp->pComponentPrivate;
  int freeBuf = MFC_CAP_OUT_BUF_COUNT;
  omx_base_video_PortType *inPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[0];
  omx_base_video_PortType *outPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[1];
  OMX_U32 width = inPort->sPortParam.format.video.nFrameWidth;
  OMX_U32 height = inPort->sPortParam.format.video.nFrameHeight;
  OMX_U32 frameSize = width * height * 3 / 2;

  DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, inbuf=%d, outbuf=%d inFlags=%d\n", __func__, inBuffer->nFilledLen, outBuffer->nFilledLen, inBuffer->nFlags);

  int pollStatus = 0;

  if (inBuffer->nFilledLen >= frameSize && !(omx_videoenc_component_Private->mfcState & MFC_STATE_EOI)) {
    for (freeBuf = 0; freeBuf < MFC_CAP_OUT_BUF_COUNT; ++freeBuf) {
      if (!omx_videoenc_component_Private->mfcInBufBusy[freeBuf]) {
        DEBUG(DEB_LEV_FULL_SEQ, "Found free buf at %d\n", freeBuf);
        break;
      }
    }
  }

  if (omx_videoenc_component_Private->mfcState & MFC_STATE_STREAMING)
    pollStatus = mfc_poll_status(omx_videoenc_component_Private->mfcFileHandle, freeBuf < MFC_CAP_OUT_BUF_COUNT ? 0 : 1000);

  DEBUG(DEB_LEV_FULL_SEQ, "Poll status = %d\n", pollStatus);
  if (pollStatus & POLLOUT) {
    freeBuf = mfc_v4l_deq(omx_videoenc_component_Private, 0);
  }

  if (!(omx_videoenc_component_Private->mfcState & MFC_STATE_EOI)) {
    if (freeBuf < MFC_CAP_OUT_BUF_COUNT) {
      int isEOS = 0;
      if (inBuffer->nFilledLen >= frameSize) {
        convertYUV420PtoNV12M(inBuffer->pBuffer + inBuffer->nOffset, &omx_videoenc_component_Private->mfcInBufAddr[2*freeBuf], width, height);
        if (!(inBuffer->nFlags & OMX_BUFFERFLAG_EOS)) {
          inBuffer->nFilledLen -= frameSize;
          inBuffer->nOffset += frameSize;
        }
      }
      if (inBuffer->nFlags & OMX_BUFFERFLAG_EOS) {
        isEOS = 1;
        /* BufferMgmtFunction after receiving inBuffer with EOS flag wants to finish execution, which is not what we want
           - MFC has to release all remaining encoded frames.
           Clearing this flag and not decreasing inBuffer->nFilledLen will prevent this.
        */
        inBuffer->nFlags &= ~OMX_BUFFERFLAG_EOS;
        omx_videoenc_component_Private->mfcState |= MFC_STATE_EOI;
      } else {
        DEBUG(DEB_LEV_FULL_SEQ, "Returning in buffer with filledLength=%d\n", inBuffer->nFilledLen);
        inBuffer->nOffset = 0;
        inBuffer->nFilledLen = 0;
      }
      mfc_v4l_enq(omx_videoenc_component_Private, 0, freeBuf, isEOS);
    }
  }

  if (pollStatus & POLLIN) {
    mfc_v4l_deq(omx_videoenc_component_Private, 1);
    outBuffer->nOffset = 0;
    if (omx_videoenc_component_Private->mfcOutBytesUsed == 0) {
      /* Caller checks only inBuffer for EOS */
      inBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
      inBuffer->nFilledLen = 0;
      outBuffer->nFilledLen = 0;
    } else {
      if (outBuffer->nAllocLen < omx_videoenc_component_Private->mfcOutBytesUsed) {
        DEBUG(DEB_LEV_ERR, "Outbut buffer too small%d < %d\n", outBuffer->nAllocLen, omx_videoenc_component_Private->mfcOutBytesUsed);
      } else {
        memcpy(outBuffer->pBuffer, omx_videoenc_component_Private->mfcOutBufAddr, omx_videoenc_component_Private->mfcOutBytesUsed);
        outBuffer->nFilledLen = omx_videoenc_component_Private->mfcOutBytesUsed;
      }
      mfc_v4l_enq(omx_videoenc_component_Private, 1, 0, 0);
    }
    DEBUG(DEB_LEV_FULL_SEQ, "Returning out buffer %p+%d\n", outBuffer->pBuffer, outBuffer->nFilledLen);
  }
}

/** The Constructor of the video encoder component
  * @param openmaxStandComp the component handle to be constructed
  * @param cComponentName is the name of the constructed component
  */
OMX_ERRORTYPE omx_videoenc_component_Constructor(OMX_COMPONENTTYPE *openmaxStandComp,OMX_STRING cComponentName)
{
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  omx_videoenc_component_PrivateType* omx_videoenc_component_Private;
  omx_base_video_PortType *inPort,*outPort;

  if (!openmaxStandComp->pComponentPrivate) {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, allocating component\n", __func__);
    openmaxStandComp->pComponentPrivate = calloc(1, sizeof(omx_videoenc_component_PrivateType));
    if(openmaxStandComp->pComponentPrivate == NULL) {
      return OMX_ErrorInsufficientResources;
    }
  } else {
    DEBUG(DEB_LEV_FUNCTION_NAME, "In %s, Error Component %x Already Allocated\n", __func__, (int)openmaxStandComp->pComponentPrivate);
  }

  omx_videoenc_component_Private = openmaxStandComp->pComponentPrivate;

  eError = omx_base_filter_Constructor(openmaxStandComp, cComponentName);

  omx_videoenc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nStartPortNumber = 0;
  omx_videoenc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts = 2;

  /** Allocate Ports and call port constructor. */
  omx_videoenc_component_Private->ports = calloc(omx_videoenc_component_Private->sPortTypesParam[OMX_PortDomainVideo].nPorts, sizeof(omx_base_PortType *));
  if (!omx_videoenc_component_Private->ports)
      return OMX_ErrorInsufficientResources;

  omx_videoenc_component_Private->ports[0] = calloc(1, sizeof(omx_base_video_PortType));
  omx_videoenc_component_Private->ports[1] = calloc(1, sizeof(omx_base_video_PortType));
  if (!omx_videoenc_component_Private->ports[0] || !omx_videoenc_component_Private->ports[1])
      return OMX_ErrorInsufficientResources;

  base_video_port_Constructor(openmaxStandComp, &omx_videoenc_component_Private->ports[0], 0, OMX_TRUE);
  base_video_port_Constructor(openmaxStandComp, &omx_videoenc_component_Private->ports[1], 1, OMX_FALSE);

  //common parameters related to input port
  inPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[0];
  inPort->sPortParam.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
  inPort->sPortParam.nBufferSize = 1;
  inPort->sPortParam.format.video.xFramerate = 25;

  //common parameters related to output port
  outPort = (omx_base_video_PortType *)omx_videoenc_component_Private->ports[1];
//
  outPort->sPortParam.nBufferSize = MFC_MAX_STREAM_SIZE;
  outPort->sPortParam.format.video.xFramerate = 25;

  /** now it's time to know the video coding type of the component */
  if(!strcmp(cComponentName, VIDEO_ENC_MPEG4_NAME)) {
    omx_videoenc_component_Private->video_coding_type = OMX_VIDEO_CodingMPEG4;
  } else if(!strcmp(cComponentName, VIDEO_ENC_H264_NAME)) {
    omx_videoenc_component_Private->video_coding_type = OMX_VIDEO_CodingAVC;
  } else if(!strcmp(cComponentName, VIDEO_ENC_H263_NAME)) {
      omx_videoenc_component_Private->video_coding_type = OMX_VIDEO_CodingH263;
  } else if (!strcmp(cComponentName, VIDEO_ENC_BASE_NAME)) {
    omx_videoenc_component_Private->video_coding_type = OMX_VIDEO_CodingUnused;
  } else {
    // IL client specified an invalid component name
    return OMX_ErrorInvalidComponentName;
  }

  openmaxStandComp->GetParameter = omx_videoenc_component_GetParameter;
  openmaxStandComp->SetParameter = omx_videoenc_component_SetParameter;
  omx_videoenc_component_Private->BufferMgmtCallback = omx_videoenc_component_BufferMgmtCallback;
  omx_videoenc_component_Private->messageHandler = omx_videoenc_component_MessageHandler;

  ++noVideoEncInstance;

  if(noVideoEncInstance > MAX_COMPONENT_VIDEORNC) {
    return OMX_ErrorInsufficientResources;
  }
  return eError;
}

/**
  @file src/content_pipe/content_pipe_test.c

  This file implements a simple test for content pipes

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <OMX_Types.h>
#include <OMX_Core.h>
#include <OMX_ContentPipe.h>

/* Size of the buffers requested to the pipe */
#define BUFFER_SIZE 1024

char*  szSrcURI;
char*  szDstURI;

void test_pipe(void)
{

  CP_PIPETYPE *inPipe, *outPipe;
  CPbyte buffer[BUFFER_SIZE];
  CPresult inRes, outRes;
  OMX_ERRORTYPE err;

  err = OMX_GetContentPipe((OMX_HANDLETYPE*) &inPipe, szSrcURI);
  if(OMX_ErrorNone != err)
    fprintf(stderr, "Error retrieving content pipe");

  err = OMX_GetContentPipe((OMX_HANDLETYPE*) &outPipe, szDstURI);
  if(OMX_ErrorNone != err)
    fprintf(stderr, "Error retrieving content pipe");

  /* Open input pipe */
  inRes = inPipe->Open((CPhandle*) inPipe, szSrcURI, CP_AccessRead);
  if(inRes != 0x00000000)
    fprintf(stderr, "Error opening content pipe for reading\n");

  /* Open output pipe */
  outRes = outPipe->Create((CPhandle*) outPipe, szDstURI);
  if(outRes != 0x00000000)
    fprintf(stderr, "Error opening content pipe for writing\n");

  while(1) {

    inRes = inPipe->Read((CPhandle*) inPipe, buffer, BUFFER_SIZE);
    if(inRes != 0x00000000) {
      fprintf(stderr, "Error reading from content pipe\n");
      break;
    }

    outRes = outPipe->Write((CPhandle*) outPipe, buffer, BUFFER_SIZE);
    if(outRes != 0x00000000) {
      fprintf(stderr, "Error writing to content pipe\n");
      break;
    }

  }

  /* Close output pipe */
  outRes = outPipe->Close((CPhandle*) outPipe);
  if(outRes != 0x00000000)
    fprintf(stderr, "Error opening content pipe for reading\n");

  /* Close input pipe */
  inRes = inPipe->Close((CPhandle*) inPipe);
  if(inRes != 0x00000000)
    fprintf(stderr, "Error opening content pipe for writing\n");
}

static void usage(void) {
  printf("\n");
  printf("Usage: content_pipe [options] <srcURI> <dstURI>\n");
  printf("\nOptions:\n");
  printf("       -h        : Displays this help\n");
  printf("\n");
  exit(1);
}

static void options(int argc, char** argv)
{
  int i;

  for(i = 1; i < argc; i++) {
    if(argv[i][0] == '-') {
      switch(argv[i][1]) {
      case 'h':
      default:
	usage();
	break;
      }
    } else break;
  }

  if((argc - i) == 2) {
    szSrcURI = argv[i++];
    szDstURI = argv[i++];
  } else usage();

}

int main(int argc, char** argv)
{
  /* process command line options */
  options(argc, argv);

  /* test the pipe */
  test_pipe();

  return EXIT_SUCCESS;
}


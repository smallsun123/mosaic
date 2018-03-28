
#ifndef _SOURCE_H_
#define _SOURCE_H_

#include "pkt.h"

typedef struct InputSource{
	char url[MAX_PATH];

	int open;
	
	int abort_request;
	pthread_t read_tid;
	
	AVFormatContext *ic;
	struct SwsContext *sws;
	struct SwrContext *swr;

	int videoindex;
	int audioindex;
	AVStream *videostream;
	AVStream *audiostream;

	AVCodecContext *c;
	AVCodecContext *ac;
	
	AVPacketQueue *videoq;
	AVPacketQueue *audioq;
} InputSource;

int initinputsource(InputSource *src);

int openinput(InputSource *src);
int closeinput(InputSource *src);

int streamopen(InputSource *src);
int streamclose(InputSource *src);

#endif

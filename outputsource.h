#ifndef _OUTPUT_SOURCE_H_
#define _OUTPUT_SOURCE_H_

#include "pkt.h"

typedef struct Encode_X264{
	enum AVPixelFormat pix_fmt;
	char profile[MAX_PATH];;
	char level[MAX_PATH];
	char preset[MAX_PATH];
	int fps;
	int bitrate;
	int width;
	int height;
} Encode_X264;

typedef struct Encode_Aac{
	enum AVSampleFormat sample_fmt;
	int bit_rate; 
	int sample_rate;
	int channel;
} Encode_Aac;

typedef struct OutPutSource{
	char url[MAX_PATH];

	int open;
	int abort_request;
	pthread_t write_tid;

	Encode_X264 x264;
	Encode_Aac aac;
	
	AVFormatContext *oc;
	AVCodecContext *c;

	AVCodecContext *ac;

	int videoindex;
	int audioindex;
	AVStream *videostream;
	AVStream *audiostream;
	
	AVPacketQueue *dataq;
} OutPutSource;

int initoutputsource(OutPutSource *src);

AVStream* new_stream(OutPutSource *src, enum AVCodecID codecID);

int setvideoparam(OutPutSource *src, enum AVPixelFormat pix_fmt, char *profile, char *level, char *preset, int fps, int bitrate, int width, int height);
int setaudioparam(OutPutSource *src, enum AVSampleFormat sample_fmt, int bitrate, int samplerate, int channel);

int openoutput(OutPutSource *src);
int closeoutput(OutPutSource *src);

int outputstreamopen(OutPutSource *src);
int outputstreamclose(OutPutSource *src);

#endif

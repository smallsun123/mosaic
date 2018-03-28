#ifndef _IMAGEMOSAIC_H_
#define _IMAGEMOSAIC_H_

#include "pkt.h"
#include "inputsource.h"
#include "outputsource.h"

typedef struct ImageMosaic{

	int abort_request;
	pthread_t mosaic_tid;
	
	AVFrame **frame;
	int nframe;

	InputSource **in;
	int ninput;
	
	OutPutSource *out;
	
}ImageMosaic;

int caloffset(int mode, int seq, int dstheight, int dstylinesize, int dstuvlinesize, int *yoffset, int *uvoffset);

int initimagemosaic(ImageMosaic *mosaic);
int deinitimagemosaic(ImageMosaic *mosaic);


AVFrame* initframe(enum AVPixelFormat pix_fmt, int width, int height);
AVPacket* mosaicframe(ImageMosaic *mosaic, enum AVPixelFormat pix_fmt, int width, int height);

int startimagemosaic(ImageMosaic *mosaic);
int stopimagemosaic(ImageMosaic *mosaic);

#endif

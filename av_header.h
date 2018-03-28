#ifndef _AV_HEADER_H_
#define _AV_HEADER_H_

#include <libavformat/avformat.h>
#include <libavformat/avformat.h>

#include <libavdevice/avdevice.h>

#include <libavcodec/avcodec.h>

#include <libavutil/avutil.h>
#include <libavutil/log.h>
#include <libavutil/rational.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include "libavutil/audio_fifo.h"

#include "libswscale/swscale.h"

#include "libswresample/swresample.h"

#if CONFIG_AVFILTER
#include <libavfilter/avfilter.h>
#include <libavfilter/avfiltergraph.h>
#endif

#include <pthread.h>

#include <signal.h>  

#define MAX_PATH 1024

#endif

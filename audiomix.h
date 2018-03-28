#ifndef _AUDIO_MIX_H_
#define _AUDIO_MIX_H_

#include "pkt.h"
#include "inputsource.h"
#include "outputsource.h"


typedef struct AudioMix{
	int abort_request;
	pthread_t audiomix_tid;

	AVFrame **frame;
	int nframe;

	InputSource **in;
	int ninput;
	
	OutPutSource *out;
}AudioMix;


int initaudiomix(AudioMix *mix);
int deinitaudiomix(AudioMix *mix);

AVFrame* initaudioframe(enum AVSampleFormat sample_fmt, int nb_samples, int channel_layout, int sample_rate);
AVPacket* audiomixframe(AudioMix *mix, enum AVSampleFormat sample_fmt, int channel_layout, int sample_rate);

int startaudiomix(AudioMix *mix);
int stopaudiomix(AudioMix *mix);



#endif

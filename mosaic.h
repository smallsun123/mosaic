#ifndef _MOSAIC_H_
#define _MOSAIC_H_

#include "config.h"
#include "pkt.h"
#include "inputsource.h"
#include "outputsource.h"
#include "imagemosaic.h"
#include "audiomix.h"

void term_init(void);

void ffmpeg_init();

int init_inputsource(int num);
int deinit_inputsource(int num);

int init_outputsource(OutPutSource *out);

int main(int argc, void **argv);

#endif

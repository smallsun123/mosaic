#include "mosaic.h"


OutPutSource out;
ImageMosaic mosaic;
AudioMix audiomix;

static int abort_request = 0;

InputSource **g_input = NULL;

char g_szConfigPath[MAX_PATH];

static void
sigterm_handler(int sig)
{
    abort_request = 1;
	return;
}

void 
term_init(void)
{
    signal(SIGINT , sigterm_handler); /* Interrupt (ANSI).    */
    signal(SIGTERM, sigterm_handler); /* Termination (ANSI).  */
	return;
}

void ffmpeg_init(){

	avcodec_register_all();
	
    av_register_all();
    avformat_network_init();
	avfilter_register_all();
	
	return NULL;
}

int init_inputsource(int num){

	int i;
	char buf[100] = {0};
	char tmpurl[100]={0};
	
	g_input = malloc(sizeof(void*)*num);
	
	for(i = 0; i < num; ++i){
		g_input[i] = malloc(sizeof(InputSource));
		initinputsource(g_input[i]);
		g_input[i]->videoq = avpacketqueue_create();
		g_input[i]->audioq = avpacketqueue_create();

		memset(tmpurl, 0, 100);
		memset(buf, 0, 100);
		sprintf(buf, "url%d", i);
		
		sprintf(tmpurl, "%s", GetIniKeyString("INPUT", buf, g_szConfigPath));
		if(strlen(tmpurl) <= 0){
			av_log(NULL, AV_LOG_WARNING, "[INPUT] -- %s=null\n", buf);
			return -1;
		}
		memcpy(g_input[i]->url, tmpurl, (strlen(tmpurl)-1)*sizeof(char));
	}
	return 0;
}

int deinit_inputsource(int num){

	int i;
	for(i = 0; i < num; ++i){
		av_log(NULL, AV_LOG_WARNING, "==============streamclose %d==============\n", i);
		streamclose(g_input[i]);
	}

	for(i = 0; i < num; ++i){
		free(g_input[i]);
	}

	free(g_input);
	
	return 0;
}

int init_outputsource(OutPutSource *out){

	int i;
	char tmpurl[100]={0};
	
	initoutputsource(out);
	out->dataq = avpacketqueue_create();

	sprintf(tmpurl, "%s", GetIniKeyString("OUTPUT", "url", g_szConfigPath));
	if(strlen(tmpurl) <= 0){
		av_log(NULL, AV_LOG_WARNING, "[OUTPUT] -- url=null\n");
		return -1;
	}
	memcpy(out->url, tmpurl, (strlen(tmpurl)-1)*sizeof(char));

	setvideoparam(out, AV_PIX_FMT_YUV420P, "high", "3.0", "faster", 25, 500, 640, 480);
	setaudioparam(out, AV_SAMPLE_FMT_FLT, 140, 48000, 1);
	
	return 0;
}

int 
main(int argc, void **argv)
{
	int ninput = 0, i=0;
	if(argc < 2){
		printf("invalide args input ...\n");
		return 0;
	}

	strcpy(g_szConfigPath, *(argv+1));
	
	ffmpeg_init();

	term_init();

	ninput = GetIniKeyInt("INPUT", "count", g_szConfigPath);
	if(ninput <= 0){
		av_log(NULL, AV_LOG_WARNING, "[INPUT] -- count=%d\n", ninput);
		return 0;
	}

	mosaic.ninput = ninput;
	audiomix.ninput = ninput;

	if(init_inputsource(ninput) < 0){
		return 0;
	}
	
	if(init_outputsource(&out) < 0){
		deinit_inputsource(ninput);
		return 0;
	}

	initimagemosaic(&mosaic);
	initaudiomix(&audiomix);
	

	mosaic.out = &out;
	for(i = 0; i < ninput; ++i){
		mosaic.in[i] = g_input[i];
	}
	
	audiomix.out = &out;
	for(i = 0; i < ninput; ++i){
		audiomix.in[i] = g_input[i];
	}

	packet_queue_start(out.dataq);

	av_log(NULL, AV_LOG_WARNING, "==============openoutput==============\n");
	outputstreamopen(&out);

	av_log(NULL, AV_LOG_WARNING, "==============startimagemosaic==============\n");
	startimagemosaic(&mosaic);

	av_log(NULL, AV_LOG_WARNING, "==============startaudiomix==============\n");
	startaudiomix(&audiomix);

	for(i = 0; i < ninput; ++i){
		av_log(NULL, AV_LOG_WARNING, "==============streamopen %d==============\n", i);
		streamopen(g_input[i]);
	}

	for(i = 0; i < ninput; ++i){
		av_log(NULL, AV_LOG_WARNING, "==============packet_queue_start %d==============\n", i);
		packet_queue_start(g_input[i]->videoq);
		packet_queue_start(g_input[i]->audioq);
	}

	for(;;){
		if(abort_request)
			break;
		
		av_usleep(1000);
	}

	for(i = 0; i < ninput; ++i){
		av_log(NULL, AV_LOG_WARNING, "==============packet_queue_abort %d==============\n", i);
		packet_queue_abort(g_input[i]->videoq);
		packet_queue_abort(g_input[i]->audioq);
	}

	packet_queue_abort(out.dataq);

	av_log(NULL, AV_LOG_WARNING, "==============stopimagemosaic ==============\n");
	stopimagemosaic(&mosaic);

	av_log(NULL, AV_LOG_WARNING, "==============stopaudiomix ==============\n");
	stopaudiomix(&audiomix);

	av_log(NULL, AV_LOG_WARNING, "==============outputstreamclose ==============\n");
	outputstreamclose(&out);


	deinit_inputsource(ninput);
	
	return 0;
}


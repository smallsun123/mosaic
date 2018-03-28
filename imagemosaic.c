#include "imagemosaic.h"


int 
caloffset(int mode, int seq, int dstheight, int dstylinesize, int dstuvlinesize, int *yoffset, int *uvoffset)
{
	switch(mode){
		case 2:{
			switch(seq){
				case 0:{
					*yoffset = dstheight/4 * dstylinesize;
					*uvoffset = dstheight/8 * dstuvlinesize;
				}break;
				case 1:{
					*yoffset = dstheight/4 * dstylinesize + dstylinesize/2;
					*uvoffset = dstheight/8 * dstuvlinesize + dstuvlinesize/2;
				}break;
				default:break;
			};
		}break;
		case 3:{
			switch(seq){
				case 0:{
					*yoffset = dstylinesize/4;
					*uvoffset = dstuvlinesize/4;
				}break;
				case 1:{
					*yoffset = dstheight/2 * dstylinesize;
					*uvoffset = dstheight/4 * dstuvlinesize;
				}break;
				case 2:{
					*yoffset = dstheight/2 * dstylinesize + dstylinesize/2;
					*uvoffset = dstheight/4 * dstuvlinesize + dstuvlinesize/2;
				}break;
				default:break;
			};
		}break;
		case 4:{
			switch(seq){
				case 0:{
					*yoffset = 0;
					*uvoffset = 0;
				}break;
				case 1:{
					*yoffset = dstylinesize/2;
					*uvoffset = dstuvlinesize/2;
				}break;
				case 2:{
					*yoffset = dstheight/2 * dstylinesize;
					*uvoffset = dstheight/4 * dstuvlinesize;
				}break;
				case 3:{
					*yoffset = dstheight/2 * dstylinesize + dstylinesize/2;
					*uvoffset = dstheight/4 * dstuvlinesize + dstuvlinesize/2;
				}break;
				default:break;
			};
		}break;
		default:break;
	}
	return 0;
}


int 
initimagemosaic(ImageMosaic *mosaic)
{
	int i=0;

	mosaic->nframe = mosaic->ninput;
	mosaic->frame = malloc(sizeof(void*)*(mosaic->nframe));
	for(i = 0; i < mosaic->ninput; ++i){
		*(mosaic->frame+i) = NULL;
	}

	mosaic->in = malloc(sizeof(void*)*(mosaic->ninput));
	for(i = 0; i < mosaic->ninput; ++i){
		*(mosaic->in+i) = NULL;
	}
	
	mosaic->out = NULL;

	mosaic->abort_request = 0;
	mosaic->mosaic_tid = 0;
	
	return 0;
}

int 
deinitimagemosaic(ImageMosaic *mosaic)
{
	int i;
	if(mosaic->nframe){
		for(i = 0; i < mosaic->nframe; ++i){
			av_frame_free(&(mosaic->frame[i]));
		}
		free(mosaic->frame);
		mosaic->frame = NULL;
	}

	if(mosaic->ninput){
		free(mosaic->in);
		mosaic->in = NULL;
	}

	return 0;
}


AVFrame* 
initframe(enum AVPixelFormat pix_fmt, int width, int height)
{
	int i;
	AVFrame *frame = av_frame_alloc();

	frame->width = width;
	frame->height = height;
	frame->format = (enum AVSampleFormat)pix_fmt;

	if (av_frame_get_buffer(frame, 32) < 0) {
		av_log(NULL, AV_LOG_ERROR, "initframe --- allocate video frame buffer failed\n");

		av_frame_free(&frame);
		return NULL;
	}

	for (i = 0; i < 3; ++i) {
		uint8_t* pdata = frame->data[i];
		int linesize = frame->linesize[i];
		int height = frame->height;
		if (i > 0) {
			height = height/2;	
			memset(pdata, 1, linesize * height);
		} else {
			memset(pdata, 0, linesize * height);
		}
	}	

	return frame;
}


AVPacket* 
mosaicframe(ImageMosaic *mosaic, enum AVPixelFormat pix_fmt, int width, int height)
{
	int i, ret, got_out = 0, serial, plane;
	int64_t pts=0, dts=0;
	AVPacket *pkt = NULL;
	AVPacket *pkt1 = NULL;
	AVFrame *frame = initframe(pix_fmt, width, height);
	
	if(!frame)
		return NULL;

	
	for(i = 0; i < mosaic->ninput; ++i){
		
		if(!mosaic->in[i]){
			av_log(NULL, AV_LOG_INFO, "------input null-------\n");
			continue;
		}
			
		ret = packet_queue_get(mosaic->in[i]->videoq, &pkt1, 1, &serial);
		if(ret <= 0){
			//av_log(NULL, AV_LOG_INFO, "------packet_queue_get null-------\n");
			//av_usleep(500);
			continue;
		}

		if(i == 0){
			pts = pkt1->pts;
			dts = pkt1->dts;
		}
		
		AVFrame *iframe = av_frame_alloc();

		ret = avcodec_decode_video2(mosaic->in[i]->c, iframe, &got_out, pkt1);
		if (ret < 0) {
			av_packet_free(&pkt1);
			av_frame_free(&iframe);
			av_log(NULL, AV_LOG_ERROR, "avcodec_decode_video2 error: %d, %s\n", ret, av_err2str(AVERROR(ret)));
			continue;
        } else {
            if (got_out) {
                frame->pts = pkt1->pts;
				frame->pkt_dts = pkt1->dts;
            } else {
				av_packet_free(&pkt1);
				av_frame_free(&iframe);
				av_log(NULL, AV_LOG_ERROR, "avcodec_decode_video2 error: %d, %s\n", ret, av_err2str(AVERROR(ret)));
				continue;
			}
        }

		if(!(mosaic->frame[i]))
			mosaic->frame[i] = initframe(pix_fmt, width/2, height/2);

		if(!(mosaic->in[i]->sws)){
			mosaic->in[i]->sws = sws_getCachedContext(mosaic->in[i]->sws, 
					    iframe->width, iframe->height, iframe->format, 
					    width/2, height/2, pix_fmt,
					    SWS_BICUBIC, NULL, NULL, NULL);
		}
		
		ret = sws_scale(mosaic->in[i]->sws, (const uint8_t * const *)iframe->data, iframe->linesize, 0, 
					    iframe->height, mosaic->frame[i]->data, mosaic->frame[i]->linesize);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "sws_scale error: %d, %s\n", ret, av_err2str(AVERROR(ret)));
        }

		mosaic->frame[i]->pts = pkt1->pts;
		mosaic->frame[i]->pkt_dts = pkt1->dts;

		av_packet_free(&pkt1);
		av_frame_free(&iframe);
	}

	for (i = 0; i < mosaic->ninput; ++i) {

		if(!(mosaic->frame) || !(mosaic->frame[i])){
			av_log(NULL, AV_LOG_INFO, "------frame null-------\n");
			continue;
		}

		int yoffset=0, uvoffset=0;
		caloffset(mosaic->ninput, i, frame->height, frame->linesize[0], frame->linesize[1], &yoffset, &uvoffset);

		//av_log(NULL, AV_LOG_INFO, "===seq=%d, yoffset=%d, uvoffset=%d===\n", i, yoffset, uvoffset);

	    for (plane = 0; plane < 3; ++plane) {
			int offset = 0, height, h;
			
			uint8_t* src = mosaic->frame[i]->data[plane];
			int srclinesize = mosaic->frame[i]->linesize[plane];

			height = mosaic->frame[i]->height;
    		if (plane > 0) {
    			height /= 2;
			}

			if(plane == 0)
				offset = yoffset;
			else
				offset = uvoffset;
			
			uint8_t* dst = frame->data[plane] + offset;
			int dstlinesize = frame->linesize[plane];

			for (h = 0; h < height; ++h) {
				memcpy(dst, src, srclinesize);
    			src += srclinesize;
				dst += dstlinesize;
			}
	    }
    }

	pkt = av_packet_alloc();
	if(!pkt){
		av_log(NULL, AV_LOG_INFO, "------pkt null-------\n");
		av_frame_free(&frame);
		return NULL;
		
	}
	av_init_packet(pkt);

	got_out = 0;
	ret = avcodec_encode_video2(mosaic->out->c, pkt, frame, &got_out);
	if(ret < 0 || !got_out){
		av_log(NULL, AV_LOG_ERROR, "Failed to encode video output: %s\n", av_err2str(AVERROR(ret)));
		
		av_frame_free(&frame);
		av_packet_free(&pkt);
		return NULL;
	}
	pkt->dts = dts;
	pkt->pts = pts;
	pkt->stream_index = mosaic->out->videoindex;
	av_frame_free(&frame);

	return pkt;
}

static void*
mosaic_thread(void *arg)
{
	int i=0, open=1;
	ImageMosaic *mosaic = (ImageMosaic*)arg;

	for(;;){
		open=1;
		for(i = 0; i < mosaic->ninput; ++i){
			if(!mosaic->in[i]->open){
				open = 0;
			}
		}
		
		if(open)
			break;

		if(mosaic->abort_request)
			return NULL;
		av_usleep(1000);
	}

/*
	AVStream *videoStream = mosaic->out->videostream;
	enum AVPixelFormat pix_fmt = videoStream->codec->pix_fmt;
	int width = videoStream->codec->width;
	int height = videoStream->codec->height;
*/

	enum AVPixelFormat pix_fmt = mosaic->out->c->pix_fmt;
	int width = mosaic->out->c->width;
	int height = mosaic->out->c->height;

	AVPacket *pkt = NULL;
	
	for(;;){
		if(mosaic->abort_request)
			break;

		pkt = mosaicframe(mosaic, pix_fmt, width, height);
		if(!pkt)
			continue;

		//av_log(NULL, AV_LOG_INFO, "packet_queue_put() --video-- pkt->key=%d, pkt->size=%d, pkt->dts=%lld\n", 
			//pkt->flags, pkt->size, pkt->dts);
		
		packet_queue_put(mosaic->out->dataq, pkt);
	}

	return NULL;
}


int 
startimagemosaic(ImageMosaic *mosaic)
{
	int ret;
	ret = pthread_create(&mosaic->mosaic_tid, NULL, mosaic_thread, (void*)mosaic);
	if(ret)
	{
		av_log(NULL, AV_LOG_ERROR, "Failed to start thread: %s\n", av_err2str(AVERROR(ret)));
        ret = AVERROR(ret);
	}
	return ret;
}

int 
stopimagemosaic(ImageMosaic *mosaic)
{
	int ret;
	mosaic->abort_request = 1;
	ret = pthread_join(mosaic->mosaic_tid, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "pthread join error: %s\n", av_err2str(AVERROR(ret)));
        ret = AVERROR(ret);
    }

	deinitimagemosaic(mosaic);

	return ret;
}


#include "inputsource.h"

int 
initinputsource(InputSource *src)
{
	memset(src, 0, sizeof(InputSource));

	src->ic = NULL;
	src->sws = NULL;
	src->swr = NULL;
	src->videostream = NULL;
	src->audiostream = NULL;
	src->videoq = NULL;
	src->audioq = NULL;
	src->c = NULL;
	src->ac = NULL;
	src->open = 0;
	
	return 0;
}

int 
openinput(InputSource *src)
{
	int ret=0, err=0, i=0;
	AVDictionary* options = NULL;
	//av_dict_set(&options, "timeout", "10", 0);

	src->ic = avformat_alloc_context();
    if (!src->ic) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
        ret = AVERROR(ENOMEM);
        goto fail;
    }

	err = avformat_open_input(&src->ic, src->url, NULL, &options);
    if (err < 0) {
        av_log(NULL, AV_LOG_FATAL, "open input url:%s failed\n", src->url);
        ret = -1;
        goto fail;
    }

	err = avformat_find_stream_info(src->ic, NULL);
	if (err < 0) {
        av_log(NULL, AV_LOG_FATAL, "%s: could not find stream info\n", src->url);
        ret = -1;
        goto fail;
    }

	for (i=0; i < src->ic->nb_streams; i++) {
        AVStream *stream = src->ic->streams[i];
		AVCodecContext *c;
		AVCodec *codec;
		AVDictionary *opts = NULL;

		c = avcodec_alloc_context3(NULL);
	    if (!c){
			ret = -1;
        	goto fail;
		}

		ret = avcodec_parameters_to_context(c, src->ic->streams[i]->codecpar);
	    if (ret < 0)
	        goto fail;

		av_codec_set_pkt_timebase(c, src->ic->streams[i]->time_base);

		codec = avcodec_find_decoder(c->codec_id);

		c->codec_id = codec->id;

		if (c->codec_type == AVMEDIA_TYPE_VIDEO || c->codec_type == AVMEDIA_TYPE_AUDIO)
        	av_dict_set(&opts, "refcounted_frames", "1", 0);
		
        if (c->codec_type == AVMEDIA_TYPE_VIDEO || c->codec_type == AVMEDIA_TYPE_AUDIO) {
        	if (c->codec_type == AVMEDIA_TYPE_AUDIO) {
        		src->audioindex = i;
        		src->audiostream = stream;
				src->ac = c;
        	} else {
        		src->videoindex = i;
        		src->videostream = stream;
				src->c = c;
        	}

			//av_log(NULL, AV_LOG_WARNING, "src ===> index = %d, codec_id = %d\n", i, c->codec_id);
		
			//if (c->codec_id == AV_CODEC_ID_AAC)
				//continue;

        	/* Open decoder */
			int ret = avcodec_open2(c, codec, &opts);
        	if (ret < 0) {
        		av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u for input %s\n", i, src->url);
        		ret = -1;
        		goto fail;
        	}			
        }
	}

	av_dump_format(src->ic, 0, src->url, 0);

	return ret;
	
fail:
    if(src && src->ic)
        avformat_close_input(&src->ic);

	if(src && src->c)
		avcodec_free_context(&src->c);

	if(src && src->ac)
		avcodec_free_context(&src->ac);
	
	return ret;
}

int 
closeinput(InputSource *src)
{
	if(src->c)
		avcodec_free_context(&src->c);

	if(src->ac)
		avcodec_free_context(&src->ac);
	
	if(src->ic){
		avformat_close_input(&src->ic);
		src->ic = NULL;
	}

	if(src->sws){
		sws_freeContext(src->sws);
		src->sws = NULL;
	}

	if(src->swr){
		swr_free(&src->swr);
		src->swr = NULL;
	}

	if(src->videoq){
		avpacketqueue_destroy(src->videoq);
		src->videoq = NULL;
	}

	if(src->audioq){
		avpacketqueue_destroy(src->audioq);
		src->audioq = NULL;
	}
	return 0;
}

static void*
read_thread(void *arg)
{
	InputSource *src = (InputSource*)arg;
	int ret;
	AVPacket *pkt;

	
	if((ret = openinput(src)) < 0)
		return NULL;

	src->open = 1;

	for(;;){
		if(src->abort_request)
			break;

		pkt = av_packet_alloc();
		av_init_packet(pkt);

		ret = av_read_frame(src->ic, pkt);
        if (ret < 0) {
            if (src->ic->pb && src->ic->pb->error){
				av_log(NULL, AV_LOG_ERROR, "read_stop! read_thread error %d, %s\n", ret, av_err2str(AVERROR(ret)));
				break;
			}
			av_log(NULL, AV_LOG_ERROR, "read_thread error %d, %s\n", ret, av_err2str(AVERROR(ret)));
           	av_usleep(500);
			continue;
        }

		//av_log(NULL, AV_LOG_INFO, "av_read_frame() ---- pkt->index=%d, pkt->key=%d, pkt->size=%d, pkt->dts=%lld\n", 
			//pkt->stream_index,pkt->flags, pkt->size, pkt->dts);

		if (pkt->stream_index == src->videoindex && src->videoq){
			//av_log(NULL, AV_LOG_ERROR, "read_thread ----> packet_queue_put -->video\n");
			packet_queue_put(src->videoq, pkt);
		} else if (pkt->stream_index == src->audioindex && src->audioq){
			//av_log(NULL, AV_LOG_ERROR, "read_thread ----> packet_queue_put -->audio\n");
			packet_queue_put(src->audioq, pkt);
		} else {
			//av_log(NULL, AV_LOG_INFO, "read_thread ----> av_packet_free\n");
			av_packet_free(&pkt);
		}
	}

	return NULL;
}


int 
streamopen(InputSource *src)
{
	int ret;
	ret = pthread_create(&src->read_tid, NULL, read_thread, (void*)src);
	if(ret)
	{
		av_log(NULL, AV_LOG_ERROR, "Failed to start thread: %s\n", av_err2str(AVERROR(ret)));
        ret = AVERROR(ret);
	}
	return ret;
}

int 
streamclose(InputSource *src)
{
	int ret;
	src->abort_request = 1;
	ret = pthread_join(src->read_tid, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "pthread join error: %s\n", av_err2str(AVERROR(ret)));
        ret = AVERROR(ret);
    }

	closeinput(src);
	
	return ret;
}



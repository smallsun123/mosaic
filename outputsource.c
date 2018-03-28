#include "outputsource.h"
#include "imagemosaic.h"

extern ImageMosaic mosaic;

int 
initoutputsource(OutPutSource *src)
{
	memset(src, 0, sizeof(OutPutSource));

	src->oc = NULL;
	src->videostream = NULL;
	src->audiostream = NULL;
	src->dataq = NULL;
	src->abort_request = 0;
	src->open = 0;
	src->c = NULL;
	src->ac = NULL;
	
	return 0;
}

int 
setvideoparam(OutPutSource *src, enum AVPixelFormat pix_fmt, char *profile, char *level, char *preset, int fps, int bitrate, int width, int height)
{
	memcpy(src->x264.profile, profile, strlen(profile)*sizeof(char));
	memcpy(src->x264.level, level, strlen(level)*sizeof(char));
	memcpy(src->x264.preset, preset, strlen(preset)*sizeof(char));

	src->x264.pix_fmt = pix_fmt;
	src->x264.fps = fps;
	src->x264.bitrate = bitrate;
	src->x264.width = width;
	src->x264.height = height;

	return 0;
}

int 
setaudioparam(OutPutSource *src, enum AVSampleFormat sample_fmt, int bitrate, int samplerate, int channel)
{
	src->aac.sample_rate = samplerate;
	src->aac.bit_rate = bitrate;
	src->aac.sample_fmt = sample_fmt;
	src->aac.channel = channel;
	
	return 0;
}

AVStream* 
new_stream(OutPutSource *src, enum AVCodecID codecID)
{
	AVCodec* codec = NULL;
	AVStream* stream = NULL;
	AVDictionary *options = NULL;
	AVCodecContext *c = NULL;

	if(codecID == AV_CODEC_ID_H264){
		codec = avcodec_find_encoder(codecID);
		//codec = avcodec_find_encoder_by_name("libx264");
	} else {
		//codec = avcodec_find_encoder_by_name("libfdk_aac");
		codec = avcodec_find_encoder(codecID);
	}
	if (!codec) {
		av_log(NULL, AV_LOG_ERROR, "[%s] find codecID = %d failed\n", src->url, codecID);
		goto fail;
	}
	
	stream = avformat_new_stream(src->oc, NULL);
	if (stream == NULL) {
		av_log(NULL, AV_LOG_ERROR, "[%s] add new stream failed\n", src->url);
		goto fail;
	}

	c = avcodec_alloc_context3(codec);
    if (!c) {
        av_log(NULL, AV_LOG_ERROR, "Could not alloc an encoding context\n");
        goto fail;
    }

	switch(codec->type){
	case AVMEDIA_TYPE_VIDEO:
		
		src->c = c;
		
		c->bit_rate = src->x264.bitrate * 1000;
		c->width 	= src->x264.width;
		c->height 	= src->x264.height;
		c->time_base = (AVRational){1, src->x264.fps};
		c->gop_size  = 2 * src->x264.fps;
		c->pix_fmt  = src->x264.pix_fmt;

		stream->avg_frame_rate.den = 1;
		stream->avg_frame_rate.num = src->x264.fps;

		av_dict_set(&options, "preset", src->x264.preset, 0);
		av_dict_set(&options, "profile", src->x264.profile, 0);
		av_dict_set(&options, "level", src->x264.level, 0);
		av_dict_set(&options, "tune", "zerolatency", 0);
		break;
	case AVMEDIA_TYPE_AUDIO:

		src->ac = c;
		
		c->codec_id    = codecID;
		c->sample_fmt  = src->aac.sample_fmt;
		c->bit_rate    = src->aac.bit_rate * 1000;
		c->sample_rate = src->aac.sample_rate;
		c->channels    = src->aac.channel;
		c->channel_layout = av_get_default_channel_layout(c->channels);
		c->time_base   = (AVRational){ 1, c->sample_rate };
		
		break;
	default:
		av_log(NULL, AV_LOG_ERROR, "[%s] add new stream input error\n", src->url);
		goto fail;
	}

	stream->time_base = c->time_base;

	c->flags |= CODEC_FLAG_GLOBAL_HEADER;

	if (src->oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if (avcodec_open2(c, codec, &options)< 0) {
		av_log(NULL, AV_LOG_ERROR, "[%s] %s, open codec failed\n", 
			src->url, codecID == AV_CODEC_ID_H264 ? "v" : "a");
		goto fail;
	}

	if(avcodec_copy_context(stream->codec, c) < 0){
		av_log(NULL, AV_LOG_ERROR, "Could not copy the stream codec parameters\n");
        goto fail;
	}
	
    if (avcodec_parameters_from_context(stream->codecpar, c) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not copy the stream parameters\n");
        goto fail;
    }

	return stream;

fail:
	if(stream)
		ff_free_stream(src->oc, &stream);

	if(c)
		avcodec_free_context(&c);
	return NULL;
}


int 
openoutput(OutPutSource *src)
{
	int ret;
	AVDictionary* options = NULL;
	
	ret = avformat_alloc_output_context2(&src->oc, NULL, "flv", src->url);
    if (!src->oc) {
        av_log(NULL, AV_LOG_ERROR, "alloc output context2 failed, url:%s\n", src->url);
		ret = AVERROR(ENOMEM);
        goto fail;
    }

	src->videostream = new_stream(src, AV_CODEC_ID_H264);
	src->videoindex = src->videostream->index;

	src->audiostream = new_stream(src, AV_CODEC_ID_AAC);
	src->audioindex = src->audiostream->index;

	if (!src->videostream || !src->audiostream) {
		av_log(NULL, AV_LOG_ERROR, "[%s] add stream failed\n", src->url);
		ret = -1;
        goto fail;
	}

	av_dump_format(src->oc, 0, src->url, 1);

	if ((ret = avio_open(&src->oc->pb, src->url, AVIO_FLAG_WRITE)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "[%s] avio_open failed\n", src->url);
        goto fail;
	}

	av_dict_set(&options, "chunksize", "1024", 0);
	if ((ret = avformat_write_header(src->oc, &options)) < 0) {
		av_log(NULL, AV_LOG_ERROR,
               "Could not write header for output file %s "
               "(incorrect codec parameters ?): %s\n",
               src->url, av_err2str(ret));
		goto fail;
	}

	return ret;
	
fail:
	if(src && src->oc && src->oc->pb)
		avio_close(src->oc->pb);

	if(src && src->videostream)
		ff_free_stream(src->oc,&src->videostream);

	if(src && src->audiostream)
		ff_free_stream(src->oc,&src->audiostream);
	
	if (src && src->oc)
        avformat_free_context(src->oc);

	return ret;
}

int 
closeoutput(OutPutSource *src)
{
	int ret;
	if ((ret = av_write_trailer(src->oc)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error writing trailer of %s: %s\n", src->url, av_err2str(ret));
    }

	if(src->c)
		avcodec_free_context(&src->c);

	if(src->ac)
		avcodec_free_context(&src->ac);
	
	if(src && src->oc && src->oc->pb)
		avio_closep(&src->oc->pb);
	
	if (src && src->oc)
        avformat_free_context(src->oc);

	if(src->dataq){
		avpacketqueue_destroy(src->dataq);
		src->dataq = NULL;
	}

	return 0;
}

static void*
write_thread(void *arg)
{
	int ret, serial;
	AVPacket *pkt;
	OutPutSource *out = (OutPutSource*)arg;

	if((ret = openoutput(out)) < 0)
		return NULL;

	out->open = 1;

	for(;;){
		if(out->abort_request)
			break;
		
		ret = packet_queue_get(out->dataq, &pkt, 1, &serial);
		if(ret < 0){
			if(out->dataq->abort_request){
				av_usleep(1000);
				continue;
			}
			av_log(NULL, AV_LOG_ERROR, "packet_queue_get error: %d\n", ret);
			continue;
		}

/*
		if(pkt->stream_index == 0){
			AVFrame *frame = av_frame_alloc();

			ret = avcodec_decode_video2(mosaic.in[0]->c, frame, &got_frame, pkt);
			if (ret < 0) {
				av_packet_free(&pkt);
				av_frame_free(&frame);
				av_log(NULL, AV_LOG_ERROR, "avcodec_decode_video2 error: %d %s\n", ret, av_err2str(AVERROR(ret)));
				continue;
	        } else {
	            if (got_frame) {
	                frame->pts = frame->pkt_dts;
	            }
	        }
			av_packet_free(&pkt);

			pkt = av_packet_alloc();
			av_init_packet(pkt);

			ret = avcodec_encode_video2(out->c, pkt, frame, &got_frame);
			if(ret < 0){
				av_packet_free(&pkt);
				av_frame_free(&frame);
				av_log(NULL, AV_LOG_ERROR, "avcodec_encode_video2 error: %d %s\n", ret, av_err2str(AVERROR(ret)));
				continue;
			}
			av_frame_free(&frame);
		}
*/

		//av_log(NULL, AV_LOG_INFO, "av_write_frame() ---- pkt->index=%d, pkt->key=%d, pkt->size=%d, pkt->dts=%lld\n", 
					//pkt->stream_index,pkt->flags, pkt->size, pkt->dts);


		if (pkt->pts != AV_NOPTS_VALUE)
        	pkt->pts = av_rescale_q(pkt->pts,
                                     mosaic.in[0]->ic->streams[pkt->stream_index]->time_base,
                                     out->oc->streams[pkt->stream_index]->time_base);
	    if (pkt->dts != AV_NOPTS_VALUE)
	        pkt->dts = av_rescale_q(pkt->dts,
	                                     mosaic.in[0]->ic->streams[pkt->stream_index]->time_base,
	                                     out->oc->streams[pkt->stream_index]->time_base);
	    if (pkt->duration)
	        pkt->duration = av_rescale_q(pkt->duration,
	                                          mosaic.in[0]->ic->streams[pkt->stream_index]->time_base,
	                                          out->oc->streams[pkt->stream_index]->time_base);
	
		ret = av_interleaved_write_frame(out->oc, pkt);
		if(ret < 0){
			av_log(NULL, AV_LOG_ERROR, "av_write_frame error: %s\n", av_err2str(AVERROR(ret)));
			av_packet_free(&pkt);
			continue;
		}
		
		av_packet_free(&pkt);
	}

	return NULL;
}

int 
outputstreamopen(OutPutSource *src)
{
	int ret;
	ret = pthread_create(&src->write_tid, NULL, write_thread, (void*)src);
	if(ret)
	{
		av_log(NULL, AV_LOG_ERROR, "Failed to start thread: %s\n", av_err2str(AVERROR(ret)));
        ret = AVERROR(ret);
	}
	return ret;
}

int 
outputstreamclose(OutPutSource *src)
{
	int ret;
	src->abort_request = 1;
	ret = pthread_join(src->write_tid, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "pthread join error: %s\n", av_err2str(AVERROR(ret)));
        ret = AVERROR(ret);
    }

	return closeoutput(src);
}


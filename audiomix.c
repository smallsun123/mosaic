#include "audiomix.h"


static void 
_mix16(const int16_t* in, float dInBufferVolume, int16_t* out, size_t insize, size_t outsize)
{
	int i, isize, osize;
	float inval=0.0, outval=0.0;
	float mixedval=0.0;
	dInBufferVolume = 1.0;

	isize = insize >> 1;
	osize = outsize >> 1;
	
	size_t totalsize = osize;

	//av_log(NULL, AV_LOG_INFO, "====== insize=%d, outsize=%d, totalsize=%d ====\n", insize, outsize, totalsize);
	
	for (i = 0; i < totalsize; ++i)
	{
		if(i < isize)
			inval = ((float)in[i]) / 32768.0f;
		else
			inval = 0.0;

		if(i < osize)
			outval = ((float)out[i]) / 32768.0f;
		else
			outval = 0.0;

		mixedval = inval + outval * dInBufferVolume;
		
		if (mixedval < -1.0f)
			mixedval = -1.0f;
		
		if (mixedval > +1.0f)
			mixedval = +1.0f;
		
		out[i] = (int16_t)(mixedval / 2 * 32768.0f);
	}
}

int 
initaudiomix(AudioMix *mix)
{
	int i = 0;

	mix->nframe = mix->ninput;
	mix->frame = malloc(sizeof(void*)*(mix->nframe));
	for(i = 0; i < mix->ninput; ++i){
		*(mix->frame+i) = NULL;
	}
	
	mix->in = malloc(sizeof(void*)*(mix->ninput));
	for(i = 0; i < mix->ninput; ++i){
		*(mix->in+i) = NULL;
	}
	
	mix->out = NULL;

	mix->abort_request = 0;
	mix->audiomix_tid = 0;

	return 0;
}

int 
deinitaudiomix(AudioMix *mix)
{
	int i;
	if(mix->nframe){
		for(i = 0; i < mix->nframe; ++i){
			av_frame_free(&(mix->frame[i]));
		}
		free(mix->frame);
		mix->frame = NULL;
	}
	
	if(mix->ninput){
		free(mix->in);
		mix->in = NULL;
	}

	return 0;
}

AVFrame* 
initaudioframe(enum AVSampleFormat sample_fmt, int nb_samples, int channel_layout, int sample_rate)
{
	int ret;
	AVFrame *frame = av_frame_alloc();
	
	frame->nb_samples = nb_samples;
	frame->channel_layout = channel_layout;
	frame->format = sample_fmt;
	frame->sample_rate = sample_rate;
	frame->channels = av_get_channel_layout_nb_channels(channel_layout);

	//av_log(NULL, AV_LOG_ERROR, "====== nb_samples=%d, channels=%d, channel_layout=%d, sample_rate=%d =====\n", 
			//nb_samples, frame->channels, channel_layout, sample_rate);

	if(!frame->nb_samples || !frame->channel_layout || !frame->sample_rate){
		av_frame_free(&frame);
		return NULL;
	}
	if ((ret = av_frame_get_buffer(frame, 0)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "allocate audio frame error: %d, %s\n", ret, av_err2str(AVERROR(ret)));

		av_frame_free(&frame);
		return NULL;
	}

	return frame;
}

AVPacket* 
audiomixframe(AudioMix *mix, enum AVSampleFormat sample_fmt, int channel_layout, int sample_rate)
{
	int i, ret, serial, got_out, max_samples=0, max_seq=0;
	AVPacket *pkt, *pkt1;
	AVFrame *out=NULL;
	int64_t pts=0, dts=0;
	for(i = 0; i < mix->ninput; ++i){

		int sample=0;
		
		if(!mix->in[i]){
			av_log(NULL, AV_LOG_INFO, "------input null-------\n");
			continue;
		}

		ret = packet_queue_get(mix->in[i]->audioq, &pkt1, 1, &serial);
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

		ret = avcodec_decode_audio4(mix->in[i]->ac, iframe, &got_out, pkt1);
		if (ret < 0) {
			av_packet_free(&pkt1);
			av_frame_free(&iframe);
			av_log(NULL, AV_LOG_ERROR, "avcodec_decode_audio4 error: %d, %s\n", ret, av_err2str(AVERROR(ret)));
			continue;
        } else {
            if (got_out) {
				iframe->pts = pkt1->pts;
				iframe->pkt_dts = pkt1->dts;
            } else {
				av_packet_free(&pkt1);
				av_frame_free(&iframe);
				av_log(NULL, AV_LOG_ERROR, "avcodec_decode_audio4 error: %d, %s\n", ret, av_err2str(AVERROR(ret)));
				continue;
			}
        }

		if(!mix->in[i]->swr){
			mix->in[i]->swr = swr_alloc();

			AVFrame tmp;
			tmp.sample_rate = sample_rate;
			tmp.channel_layout = channel_layout;
			tmp.format = sample_fmt;

			ret = swr_config_frame(mix->in[i]->swr, &tmp, iframe);
			if (ret) {
				av_packet_free(&pkt1);
				av_frame_free(&iframe);
				swr_free(&(mix->in[i]->swr));
				mix->in[i]->swr = NULL;
				av_log(NULL, AV_LOG_ERROR, "swr_config_frame error: %d, %s\n", ret, av_err2str(AVERROR(ret)));
				continue;
			}
			swr_init(mix->in[i]->swr);
		}

		swr_convert_frame(mix->in[i]->swr, NULL, iframe);

		//av_log(NULL, AV_LOG_ERROR, "iframe->nb_samples=%d, iframe->sample_rate=%d, sample_rate=%d\n", 
			//iframe->nb_samples, iframe->sample_rate, sample_rate);

		sample = iframe->nb_samples*sample_rate/iframe->sample_rate;

		if(sample > max_samples){
			max_samples = sample;
			max_seq = i;
		}

		//av_log(NULL, AV_LOG_INFO, "sample=%d, max_samples=%d, layout=%d, rate=%d\n", 
			//sample, max_samples,channel_layout, sample_rate);

		if(!(mix->frame[i]))
			mix->frame[i] = initaudioframe(sample_fmt, sample, channel_layout, sample_rate);

		if(!mix->frame[i]){
			av_packet_free(&pkt1);
			av_frame_free(&iframe);
			continue;
		}

		ret = av_frame_make_writable(mix->frame[i]);
		if(ret < 0){
			av_packet_free(&pkt1);
			av_frame_free(&iframe);
			continue;
		}


		//av_log(NULL, AV_LOG_INFO, "mixsrc-%d ----> sample=%d, layout=%d, rate=%d\n", 
			//i, mix->frame[i]->nb_samples, mix->frame[i]->channel_layout, mix->frame[i]->sample_rate);
		
		av_packet_free(&pkt1);
		av_frame_free(&iframe);
	}


	//av_log(NULL, AV_LOG_INFO, "mixsrc-alloc ----> sample=%d, layout=%d, rate=%d\n", 
			//max_samples, channel_layout, sample_rate);
	
	out = initaudioframe(sample_fmt, max_samples, channel_layout, sample_rate);
	if(!out){
		return NULL;
	}

	ret = av_frame_make_writable(out);
	if(ret < 0){
		return NULL;
	}

	swr_convert_frame(mix->in[max_seq]->swr, out, NULL);

	//av_log(NULL, AV_LOG_INFO, "mixsrc-dst ----> sample=%d, layout=%d, rate=%d\n", 
			//out->nb_samples, out->channel_layout, out->sample_rate);

	
	for (i = 0; i < mix->ninput; ++i) {
		if(i == max_seq || !mix->frame[i] || !mix->in[i] || !mix->in[i]->swr)
			continue;

/*
		av_log(NULL, AV_LOG_INFO, "isample_fmt=%d, inb_samples=%d, ichannels=%d, osample_fmt=%d, onb_samples=%d, ochannels=%d\n", 
			av_get_bytes_per_sample(mix->frame[i]->format), mix->frame[i]->nb_samples, mix->frame[i]->channels,
			av_get_bytes_per_sample(out->format), out->nb_samples, out->channels);
*/

		ret = swr_convert_frame(mix->in[i]->swr, mix->frame[i], NULL);
		if(ret != 0){
			av_log(NULL, AV_LOG_ERROR, "swr_convert_frame error: %d, %s\n", ret, av_err2str(AVERROR(ret)));
			continue;
		}
		
		_mix16((const int16_t*)(mix->frame[i]->data[0]),
			1, (int16_t*)(out->data[0]),
			av_get_bytes_per_sample(mix->frame[i]->format)*(mix->frame[i]->nb_samples)*(mix->frame[i]->channels),
			av_get_bytes_per_sample(out->format)*(out->nb_samples)*(out->channels));
	}


	pkt = av_packet_alloc();
	if(!pkt){
		av_log(NULL, AV_LOG_INFO, "------pkt null-------\n");
		av_frame_free(&out);
		return NULL;
		
	}
	av_init_packet(pkt);


	got_out = 0;
	ret = avcodec_encode_audio2(mix->out->ac, pkt, out, &got_out);
	if(ret < 0 || !got_out){
		av_log(NULL, AV_LOG_ERROR, "Failed to encode audio output: %s\n", av_err2str(AVERROR(ret)));
		av_frame_free(&out);
		av_packet_free(&pkt);
		return NULL;
	}
	pkt->dts = dts;
	pkt->pts = pts;
	pkt->stream_index = mix->out->audioindex;
	av_frame_free(&out);
	
	return pkt;
}



static void*
audiomix_thread(void *arg)
{
	int i=0, open=1;
	AudioMix *mix = (AudioMix*)arg;

	for(;;){
		open=1;
		for(i = 0; i < mix->ninput; ++i){
			if(!mix->in[i]->open){
				open = 0;
			}
		}
		
		if(open)
			break;

		if(mix->abort_request)
			return NULL;
		av_usleep(1000);
	}

	enum AVSampleFormat sample_fmt = mix->out->ac->sample_fmt;
	int nbsample_rate = mix->out->ac->sample_rate;
	int nbchannel_layout = mix->out->ac->channel_layout;
	
	AVPacket *pkt = NULL;
	
	for(;;){
		if(mix->abort_request)
			break;

		pkt = audiomixframe(mix, sample_fmt, nbchannel_layout, nbsample_rate);
		if(!pkt)
			continue;

		//av_log(NULL, AV_LOG_INFO, "packet_queue_put() --audio-- pkt->key=%d, pkt->size=%d, pkt->dts=%lld\n", 
			//pkt->flags, pkt->size, pkt->dts);
		
		packet_queue_put(mix->out->dataq, pkt);
	}
	
	return NULL;
}


int
startaudiomix(AudioMix *mix)
{
	int ret;
	ret = pthread_create(&mix->audiomix_tid, NULL, audiomix_thread, (void*)mix);
	if(ret)
	{
		av_log(NULL, AV_LOG_ERROR, "Failed to start thread: %s\n", av_err2str(AVERROR(ret)));
        ret = AVERROR(ret);
	}
	return ret;
}


int
stopaudiomix(AudioMix *mix)
{
	int ret;
	mix->abort_request = 1;
	ret = pthread_join(mix->audiomix_tid, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "pthread join error: %s\n", av_err2str(AVERROR(ret)));
        ret = AVERROR(ret);
    }

	deinitaudiomix(mix);

	return ret;
}


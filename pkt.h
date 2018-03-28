
#ifndef _AVPACKET_H_
#define _AVPACKET_H_

#include "list.h"
#include "av_header.h"

typedef struct AVPacketNode {
	struct list_head 	node;
    AVPacket 			*pkt;
    int 				serial;
} AVPacketNode;


typedef struct AVPacketQueue {
	struct list_head 	list;
    AVPacketNode 		*first_pkt, *last_pkt;
    int 				nb_packets;
    int 				size;
    int64_t 			duration;
    int 				abort_request;
    int 				serial;
    pthread_mutex_t 	mutex;
    pthread_cond_t 		cond;
} AVPacketQueue;


AVPacketNode* avpacketnode_create(AVPacket* pkt);
int avpacketnode_destroy(AVPacketNode* node);

AVPacketQueue* avpacketqueue_create();
int avpacketqueue_flush(AVPacketQueue* queue);
int avpacketqueue_destroy(AVPacketQueue* queue);

void packet_queue_start(AVPacketQueue *q);
void packet_queue_abort(AVPacketQueue *q);

int packet_queue_put_private(AVPacketQueue* q, AVPacket* pkt);
int packet_queue_put(AVPacketQueue *q, AVPacket *pkt);

int packet_queue_get(AVPacketQueue *q, AVPacket **pkt, int block, int *serial);

#endif
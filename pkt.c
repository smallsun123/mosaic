#include "pkt.h"

AVPacketNode* 
avpacketnode_create(AVPacket* pkt)
{	
	AVPacketNode *node = NULL;
	node = (AVPacketNode*)malloc((size_t)sizeof(*node));
	if(node == NULL)
		return NULL;

	memset(node, 0, (size_t)sizeof(*node));
	
	INIT_LIST_HEAD(&node->node);

	node->pkt = pkt;

	return node;
}

int 
avpacketnode_destroy(AVPacketNode* node)
{
	if(!node)
		return 0;
	if(node->pkt){
		av_packet_free(&node->pkt);
		node->pkt = NULL;
	}
	
    free(node);
	node = NULL;
	
	return 0;
}

AVPacketQueue* 
avpacketqueue_create()
{
	AVPacketQueue *queue = NULL;
	queue = (AVPacketQueue*)malloc((size_t)sizeof(*queue));
	if(queue == NULL)
		return NULL;

	memset(queue, 0, (size_t)sizeof(*queue));

	INIT_LIST_HEAD(&queue->list);

	queue->first_pkt = queue->last_pkt = NULL;

	pthread_mutex_init(&queue->mutex, NULL);
	pthread_cond_init(&queue->cond, NULL);

	queue->abort_request = 1;

	return queue;
}

int 
avpacketqueue_flush(AVPacketQueue* queue)
{
	struct list_head *pos, *npos;
	AVPacketNode* node;
	if(queue){
		pthread_mutex_lock(&queue->mutex);
		
		list_for_each_safe(pos, npos, &queue->list){
			node = list_entry(pos, struct AVPacketNode, node);
			list_del(pos);
			avpacketnode_destroy(node);
		}

		queue->last_pkt = NULL;
	    queue->first_pkt = NULL;
		queue->nb_packets = 0;
	    queue->size = 0;
	    queue->duration = 0;

		pthread_mutex_unlock(&queue->mutex);
	}
	return 0;
}

int 
avpacketqueue_destroy(AVPacketQueue* queue)
{
	avpacketqueue_flush(queue);
	
	pthread_cond_destroy(&queue->cond);
	pthread_mutex_destroy(&queue->mutex);
	
	free(queue);
	queue = NULL;

	return 0;
}

void 
packet_queue_start(AVPacketQueue *q)
{
	pthread_mutex_lock(&q->mutex);
    q->abort_request = 0;
    pthread_mutex_unlock(&q->mutex);
}

void 
packet_queue_abort(AVPacketQueue *q)
{
	pthread_mutex_lock(&q->mutex);
    q->abort_request = 1;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}


int 
packet_queue_put_private(AVPacketQueue* q, AVPacket* pkt)
{
	AVPacketNode *pktnode;

    if (q->abort_request){
		if(pkt)
			av_packet_free(&pkt);
		return -1;
	}

	pktnode = avpacketnode_create(pkt);

    if (!q->first_pkt)
        q->first_pkt = pktnode;
    
    q->last_pkt = pktnode;

	list_add_tail(&(pktnode->node), &(q->list));
	
    q->nb_packets++;
    q->size += pktnode->pkt->size;
    q->duration += pktnode->pkt->duration;
	
    pthread_cond_signal(&q->cond);
    return 0;
}

int 
packet_queue_put(AVPacketQueue *q, AVPacket *pkt)
{
	int ret;

    pthread_mutex_lock(&q->mutex);
    ret = packet_queue_put_private(q, pkt);
    pthread_mutex_unlock(&q->mutex);

	return ret;
}

int 
packet_queue_get(AVPacketQueue *q, AVPacket **pkt, int block, int *serial)
{
    int ret = -1;
    pthread_mutex_lock(&q->mutex);
    for (;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

		struct list_head *pos, *npos;
		AVPacketNode *node = NULL;
		list_for_each_safe(pos, npos, &q->list){
			node = list_entry(pos, struct AVPacketNode, node);
			list_del(pos);
			break;
		}

        if (node) {
            
            q->nb_packets--;
            q->size -= node->pkt->size;
            q->duration -= node->pkt->duration;
            *pkt = node->pkt;
            if (serial)
                *serial = node->serial;

			node->pkt = NULL;
			avpacketnode_destroy(node);
            
			q->first_pkt = pos == npos ? NULL : list_entry(npos, struct AVPacketNode, node);
			if(!q->first_pkt){
				q->last_pkt = NULL;
			}
			ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            pthread_cond_wait(&q->cond, &q->mutex);
        }
    }
    pthread_mutex_unlock(&q->mutex);
    return ret;
}



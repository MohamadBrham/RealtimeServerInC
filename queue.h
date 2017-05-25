#ifndef QUEUE_H
#define QUEUE_H
struct queue_node{
	int fd;
	struct queue_node* next;
};

struct Queue{
	struct queue_node* front;
	struct queue_node* rear	;
	int nb_nodes;
	int max;
	
};

void init(struct Queue* q,int max){
	struct queue_node *head = (struct queue_node *)malloc(sizeof(struct queue_node));
	head->fd = 0;
	head->next = NULL;
	q->front = head;
	q->rear = head;
	q->nb_nodes = 0;
	q->max = max;
}

void setMax(struct Queue* q,int max){
	q->max = max;
}
int QueueIsEmpty(struct Queue* q){
	if(q->nb_nodes == 0)
		return 1;
	return 0;
}
int QueueIsFull(struct Queue* q){
	if(q->nb_nodes == q->max)
		return 1;
	return 0;
}
int QueueInsert(struct Queue *q,int fd){
	if(QueueIsFull(q)){
		return -1;
	}
	struct queue_node* node = (struct queue_node*)malloc(sizeof(struct queue_node));
	if(!node){
		printf("Error Allocation.\n");
		return -1;
	}
	node->fd = fd;
	node->next = NULL;
	q->rear->next = node;
	q->rear = node;
	q->nb_nodes++;
	return 0;
}
int i=1;
int QueueRetrieve(struct Queue *q){
	if(QueueIsEmpty(q)){
		return -1;
	}
	struct queue_node* node = q->front;
	q->front = q->front->next;
	int fd = q->front->fd;
	q->front->fd = 0;
	free(node);
	q->nb_nodes--;
	i++;
	return fd;
}





#endif
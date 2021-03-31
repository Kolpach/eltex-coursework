#ifndef QUEUE_H
#define QUEUE_H
#include "constants.h"
struct q_elem
{
	unsigned int T;
	char msg[msgMaxSize];
	int msgLen;
	struct q_elem *next = 0;
};

void push(struct q_elem *);
struct q_elem * pop(struct queue *que);

struct queue{
	int size;
	struct q_elem *top;
	struct q_elem *back;
};

#endif
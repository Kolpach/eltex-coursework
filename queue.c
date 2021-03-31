#include "queue.h"
void push(struct queue *que, struct q_elem *elem) {
	if(que->size == 0) {
		que->top = elem;
	}
	else {
		que->back->next = elem;
	}
	que->back = elem;
	++(que->size);
}
struct q_elem * pop(struct queue *que){
	if(que->size == 0)
		return 0;

	struct q_elem *loc;
	loc = que->top;
	que->top = que->top->next;
	--(que->size);
	return loc;
}
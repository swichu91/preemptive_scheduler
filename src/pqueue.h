/*
 * pqueue.h
 *
 *  Created on: Mar 31, 2016
 *      Author: mateusz
 */

#ifndef PQUEUE_H_
#define PQUEUE_H_

typedef struct {
    int priority;
    void *data;
} node_t;

typedef struct {
    node_t *nodes;
    int len;
    int size;
} heap_t;

void push (heap_t *h, int priority, void *data);
void *pop (heap_t *h);

#endif /* PQUEUE_H_ */

/*
 * simple_rtos_preemptive.h
 *
 *  Created on: Apr 11, 2016
 *      Author: mateusz
 */

#ifndef SIMPLE_RTOS_PREEMPTIVE_H_
#define SIMPLE_RTOS_PREEMPTIVE_H_

#include <stdint.h>
#include "pqueue.h"
#include <ucontext.h>
#include <sys/time.h>

#define SCHEDULER_MAX_THREAD_NR	10
#define USE_IDLE_HOOK 1

typedef struct _thread thread_t;

typedef struct _thread {
	uint32_t wait_cnt;
	ucontext_t context;
	char* name;
	int priority;
	uint8_t index;
	void (*foo)(void*);
	void* data;
	thread_t* handler;

} thread_t;

typedef struct _scheduler {
	uint8_t current_thread_indx;
	uint8_t threads_number;
	heap_t* thread_ready_list;

	thread_t *thread_list[SCHEDULER_MAX_THREAD_NR];

	struct sigaction sa;
	struct itimerval it;
	struct itimerval it_cp;

} scheduler_t;



__attribute__ ((weak)) void task_idle_hook(void);


#endif /* SIMPLE_RTOS_PREEMPTIVE_H_ */

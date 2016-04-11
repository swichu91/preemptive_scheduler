/*
 ============================================================================
 Name        : simple_rtos_preemptive.c
 Author      : Mateusz Piesta
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include "simple_rtos_preemptive.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/time.h>
#include <unistd.h>
#include "pqueue.h"
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <assert.h>

#include <sys/stat.h>
#include <fcntl.h>

//Forward declarations
void handler(int signal);
void scheduler(void);
void task_create(void (*foo)(void*), char* name, int priority, void* data,
		thread_t* handler);
void task_wait(uint32_t ms);

scheduler_t sched;

/* Simple differential time measure */
uint32_t timestamp_ms(struct timeval* start, struct timeval* last) {
	uint32_t ret = 0;

	ret = (((start->tv_sec * 1000000) + start->tv_usec)
			- ((last->tv_sec * 1000000) + last->tv_usec)) / 1000;

	memcpy(last, start, sizeof(struct timeval));

	return ret;
}

/**
 * Test tasks
 */
void task_test3(void* param) {
	static struct timeval start, last;

	while (1) {

		gettimeofday(&start, NULL);

		fprintf(stdout, "test task3:%d\n", timestamp_ms(&start, &last));
		fflush(stdout);

		task_wait(1000);

	}

}

void task_test2(void* param) {
	static struct timeval start, last;

	while (1) {

		gettimeofday(&start, NULL);

		fprintf(stdout, "test task2:%d\n", timestamp_ms(&start, &last));
		fflush(stdout);

		task_wait(1000);

	}

}

void task_test1(void* param) {
	static struct timeval start, last;

	while (1) {

		gettimeofday(&start, NULL);

		fprintf(stdout, "test task:%d\n", timestamp_ms(&start, &last));
		fflush(stdout);
		task_wait(1000);
	}

}

#if (USE_IDLE_HOOK == 1)
/*
 * Can be redefined by user
 */
__attribute__ ((weak)) void task_idle_hook(void) {

}
#endif

static void task_idle(void* param) {

	/*create tasks here only for test purposes*/
	task_create(task_test1, "test1", 10, NULL, NULL);
	task_create(task_test2, "test2", 10, NULL, NULL);
	task_create(task_test3, "test3", 10, NULL, NULL);
	while (1) {
		task_idle_hook();
	}

}

void task_enter_critical(void) {
	//pause timer, similar to EnterCriticalSection

	//make copy of current timer values
	sched.it_cp.it_value.tv_usec = sched.it.it_value.tv_usec;
	sched.it_cp.it_interval.tv_usec = sched.it.it_interval.tv_usec;

	sched.it.it_value.tv_usec = 0;
	sched.it.it_interval.tv_usec = 0;

}

void task_exit_critical(void) {
	//similar to ExitCriticalSection
	sched.it.it_value.tv_usec = sched.it_cp.it_value.tv_usec;
	sched.it.it_interval.tv_usec = sched.it_cp.it_interval.tv_usec;
}

/* lowest priority value means the highest logically */
void task_create(void (*foo)(void*), char* name, int priority, void* data,
		thread_t* handler) {
	assert(foo != NULL);

	uint8_t i;

	task_enter_critical();

	for (i = 0; i < SCHEDULER_MAX_THREAD_NR; i++) {
		if (sched.thread_list[i] == 0) {
			sched.threads_number++;

			sched.thread_list[i] = calloc(1, sizeof(thread_t));
			sched.thread_list[i]->data = data;
			sched.thread_list[i]->foo = foo;
			sched.thread_list[i]->index = i;
			sched.thread_list[i]->name = name;
			sched.thread_list[i]->priority = priority;
			sched.thread_list[i]->wait_cnt = 100;

			if (handler != NULL) {
				handler = sched.thread_list[i];
			}

			static ucontext_t ctx;

			getcontext(&ctx);
			ctx.uc_stack.ss_sp = (void *) malloc(SIGSTKSZ);
			ctx.uc_stack.ss_size = SIGSTKSZ;
			ctx.uc_stack.ss_flags = 0;
			makecontext(&ctx, (void (*)()) foo, 0);
			sched.thread_list[i]->context = ctx;

			break;
		}

	}

	task_exit_critical();

}

void task_wait(uint32_t ms) {
	sched.thread_list[sched.current_thread_indx]->wait_cnt = ms;
	scheduler();

}

void task_kill(void) {

	if (sched.thread_list[sched.current_thread_indx] != NULL) {

		task_enter_critical();

		free(sched.thread_list[sched.current_thread_indx]);

		if (sched.threads_number) {
			sched.threads_number--;
		}
		memset(sched.thread_list[sched.current_thread_indx], 0,
				sizeof(*sched.thread_list[sched.current_thread_indx]));
		sched.thread_list[sched.current_thread_indx] = NULL;
		task_exit_critical();
		scheduler();

	}

}

void task_delete(thread_t* handler) {
	free(handler);
	sched.threads_number--;
}

void scheduler_start(scheduler_t* sched) {
	sched->thread_ready_list = calloc(1, sizeof(heap_t));

	if (sched->thread_ready_list == NULL) {
		fprintf(stderr, "calloc failed");
	}

	/* Initialize the data structures for the interval timer. */
	sched->sa.sa_flags = SA_RESTART;
	sigfillset(&sched->sa.sa_mask);
	sched->sa.sa_handler = handler;
	sched->it.it_interval.tv_sec = 0;
	sched->it.it_interval.tv_usec = 1000;
	sched->it.it_value = sched->it.it_interval;

	sigaction(SIGPROF, &sched->sa, NULL);
	setitimer(ITIMER_PROF, &sched->it, NULL);

	sched->current_thread_indx = 0;
	sched->threads_number = 0;

	/* Create idle task, it runs when there is no more tasks in 'ready' state.
	 *  It has the lowest priority.
	 */
	sched->threads_number++;

	sched->thread_list[0] = calloc(1, sizeof(thread_t));
	sched->thread_list[0]->data = NULL;
	sched->thread_list[0]->foo = task_idle;
	sched->thread_list[0]->index = 0;
	sched->thread_list[0]->name = "idle";
	sched->thread_list[0]->priority = 15;
	sched->thread_list[0]->wait_cnt = 0;

	static ucontext_t ctx;

	getcontext(&ctx);
	ctx.uc_stack.ss_sp = (void *) malloc(SIGSTKSZ);
	ctx.uc_stack.ss_size = SIGSTKSZ;
	ctx.uc_stack.ss_flags = 0;
	makecontext(&ctx, (void (*)()) task_idle, 0);
	sched->thread_list[0]->context = ctx;
	setcontext(&ctx);

}

void scheduler(void) {
	thread_t* thread_to_start;
	uint8_t i;

	for (i = 0; i < SCHEDULER_MAX_THREAD_NR/*sched.threads_number*/; i++) {
		if ((sched.thread_list[i] != NULL) && sched.thread_list[i]->wait_cnt) {
			sched.thread_list[i]->wait_cnt--;
		} else if (sched.thread_list[i] != NULL) // place thread into ready_list
		{
			push(sched.thread_ready_list, sched.thread_list[i]->priority,
					sched.thread_list[i]);
		}
	}

	if ((thread_to_start = pop(sched.thread_ready_list)) != NULL) {
		sched.current_thread_indx = thread_to_start->index;
		setcontext(&thread_to_start->context);

	}

}

ucontext_t irq_ctx;
char irq_stack[SIGSTKSZ];

void irq_handler_scheduler(void* param) {
	//calls scheduler
	scheduler();
}

/* This is the signal handler which simply set the variable. */
void handler(int signal) {
	getcontext(&irq_ctx);
	irq_ctx.uc_stack.ss_sp = (void *) irq_stack;
	irq_ctx.uc_stack.ss_size = SIGSTKSZ;
	irq_ctx.uc_stack.ss_flags = 0;
	makecontext(&irq_ctx, (void (*)()) irq_handler_scheduler, 0);

	swapcontext(&sched.thread_list[sched.current_thread_indx]->context,
			&irq_ctx);

}

int main(void) {

	scheduler_start(&sched);

	/* Program should not get here as scheduler took control over program's execution flow*/
	while (1)
		;

	return 0;
}

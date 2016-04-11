/*
 ============================================================================
 Name        : simple_rtos_preemptive.c
 Author      : Mateusz Piesta
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

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

FILE* fd;

#define SCHEDULER_MAX_THREAD_NR	10

//Forward declarations
void handler(int signal);
void scheduler(void);

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

scheduler_t sched;

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
		memset(sched.thread_list[sched.current_thread_indx],0,sizeof(*sched.thread_list[sched.current_thread_indx]));
		sched.thread_list[sched.current_thread_indx] = NULL;
		task_exit_critical();
		scheduler();

	}

}

void task_delete(thread_t* handler) {
	free(handler);
	sched.threads_number--;
}
static uint32_t test_cnt;

uint32_t timestamp_ms(struct timeval* start, struct timeval* last) {
	uint32_t ret = 0;

	ret = (((start->tv_sec * 1000000) + start->tv_usec)
			- ((last->tv_sec * 1000000) + last->tv_usec)) / 1000;

	memcpy(last, start, sizeof(struct timeval));

	return ret;
}

void task_test3(void* param) {
	static struct timeval start, last;

	while (1) {

		gettimeofday(&start, NULL);

		fprintf(fd,"test task3:%d\n", timestamp_ms(&start, &last));
		fflush(stdout);
		//task_kill();

		task_wait(1000);

	}

}


void task_test2(void* param) {
	static struct timeval start, last;

	while (1) {

		gettimeofday(&start, NULL);

		fprintf(fd,"test task2:%d\n", timestamp_ms(&start, &last));
		fflush(stdout);
		//task_kill();

		task_wait(1000);

	}

}

void task_test1(void* param) {
	static struct timeval start, last;
	//task_create(task_test2,"test2",14,NULL,NULL);

	while (1) {

		gettimeofday(&start, NULL);

		fprintf(fd,"test task:%d\n", timestamp_ms(&start, &last));
		fflush(stdout);
		task_wait(1000);
	}

}
__attribute__ ((weak)) void task_idle_hook(void) {
	test_cnt++;

}

void task_idle(void* param) {

	//testowo
	task_create(task_test1, "test1", 10, NULL, NULL);
	task_create(task_test2, "test2", 10, NULL, NULL);
	task_create(task_test3, "test3", 10, NULL, NULL);
	while (1) {
		task_idle_hook();
	}

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

/* The contexts. */
static ucontext_t uc[3];

ucontext_t irq_ctx;
char irq_stack[SIGSTKSZ];

void irq_handler_scheduler(void* param) {
	//calls scheduler
	//printf("scheduler start!");
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

#if 0
	/* Install the timer and get the context we can manipulate. */
	if (getcontext(&uc[1]) == -1
			|| getcontext(&uc[2]) == -1)
	abort();

	/* Create a context with a separate stack which causes the
	 function f to be call with the parameter 1.
	 Note that the uc_link points to the main context
	 which will cause the program to terminate once the function
	 return. */
	uc[1].uc_link = &uc[0];
	uc[1].uc_stack.ss_sp = st1;
	uc[1].uc_stack.ss_size = sizeof st1;
	makecontext(&uc[1], (void (*)(void)) f1, 0, NULL);

	/* Similarly, but 2 is passed as the parameter to f. */
	uc[2].uc_link = &uc[0];
	uc[2].uc_stack.ss_sp = st2;
	uc[2].uc_stack.ss_size = sizeof st2;
	makecontext(&uc[2], (void (*)(void)) f2, 0, NULL);

	/* Start running. */
	swapcontext(&uc[0], &uc[1]);
	putchar('\n');
#endif



	fd=fopen("log.txt","rw");



	scheduler_start(&sched);

	while (1) {
		; //usleep(1000000);
	}

	return 0;
}

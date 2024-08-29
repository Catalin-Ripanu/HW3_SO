// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "helpers.h"
#include "os_threadpool.h"

int working_threads;
int queue_len;
int max_tasks;
pthread_cond_t threads_idle;

/* Functia care genereaza starea in care thread-urile asteapta */

void thread_pool_wait(os_threadpool_t *thpool_p)
{
	pthread_mutex_lock(&thpool_p->taskLock);
	while (queue_len || working_threads)
		pthread_cond_wait(&threads_idle, &thpool_p->taskLock);
	pthread_mutex_unlock(&thpool_p->taskLock);
}

/* Functia care construieste un task prin alocare dinamica */

os_task_t *task_create(void *arg, void (*f)(void *))
{
	os_task_t *thread_task = (os_task_t *)malloc(sizeof(os_task_t));

	if (!thread_task) {
		printf("[ERROR] [%s] Not enought memory\n", __func__);
		return NULL;
	}

	thread_task->task = f;
	thread_task->argument = arg;
	return thread_task;
}

/* Procedura care adauga o functie / un task in coada */

void add_task_in_queue(os_threadpool_t *tp, os_task_t *t)
{
	pthread_mutex_lock(&tp->taskLock);

	/* Se verifica daca se mai poate adauga in coada */

	if (tp->tasks && tp->tasks->task && queue_len < max_tasks) {
		os_task_queue_t *new_node = (os_task_queue_t *)(malloc(sizeof(os_task_queue_t)));

		if (!new_node) {
			printf("[ERROR] [%s] Not enought memory\n", __func__);
			exit(1);
		}
		new_node->task = t;
		new_node->next = NULL;
		os_task_queue_t *node = tp->tasks;
		os_task_queue_t *prev = node;

		for (; node;) {
			if (node) {
				prev = node;
				node = node->next;
			} else {
				break;
			}
		}

		/* Se adauga in coada */

		prev->next = new_node;
	} else {
		tp->tasks = (os_task_queue_t *)malloc(sizeof(os_task_queue_t));
		tp->tasks->task = t;
		tp->tasks->next = NULL;
	}

	/* Se mareste lungimea cozii */

	queue_len++;
	pthread_mutex_unlock(&tp->taskLock);
}

/* Functia care obtine un element din coada */

os_task_queue_t *get_task_queue(os_threadpool_t *tp)
{
	os_task_queue_t *curr_node = tp->tasks;

	if (tp->tasks && tp->tasks->next) {
		tp->tasks = tp->tasks->next;
		queue_len--;
		curr_node->next = NULL;
	} else {
		queue_len = 0;
		tp->tasks = NULL;
	}
	return curr_node;
}

/* Functia care construieste un thread_pool */

os_threadpool_t *threadpool_create(unsigned int nTasks, unsigned int nThreads)
{
	os_threadpool_t *thread_pool = (os_threadpool_t *)malloc(sizeof(os_threadpool_t));

	if (!thread_pool) {
		printf("[ERROR] [%s] Not enought memory\n", __func__);
		return NULL;
	}
	working_threads = 0;
	queue_len = 0;
	max_tasks = nTasks;
	thread_pool->num_threads = nThreads;
	thread_pool->should_stop = 0;
	pthread_mutex_init(&(thread_pool->taskLock), NULL);
	pthread_cond_init(&threads_idle, NULL);

	/* Se activeaza thread-urile */

	thread_pool->threads = (pthread_t *)malloc(thread_pool->num_threads * sizeof(pthread_t));
	for (int i = 0; i < thread_pool->num_threads; i++)
		pthread_create(&thread_pool->threads[i], NULL, thread_loop_function, thread_pool);
	return thread_pool;
}

/* Functia care modeleaza logica thread-urilor lucratoare */

void *thread_loop_function(void *args)
{
	os_threadpool_t *threadpool = (os_threadpool_t *)args;

	while (!threadpool->should_stop) {
		pthread_mutex_lock(&threadpool->taskLock);
		working_threads++;
		pthread_mutex_unlock(&threadpool->taskLock);
		if (!threadpool->should_stop) {
			void (*job_func)(void *argument);
			void *job_args;

			/* Operatia de extragere trebuie sincronizata */
			/* intrucat mai multe thread-uri actioneaza asupra cozii in acelasi timp */

			pthread_mutex_lock(&threadpool->taskLock);
			os_task_queue_t *curr_task = get_task_queue(threadpool);

			pthread_mutex_unlock(&threadpool->taskLock);

			/* Daca s-a obtinut un task, se proceseaza si se elibereaza memoria */

			if (curr_task && curr_task->task) {
				job_func = curr_task->task->task;
				job_args = curr_task->task->argument;
				job_func(job_args);
				free(curr_task->task->argument);
				free(curr_task->task);
				free(curr_task);
				curr_task = NULL;
			}
			pthread_mutex_lock(&threadpool->taskLock);

			/* Daca nu mai sunt thread-uri care sa prelucreze */
			/* se trezeste un alt thread pentru a rezolva task-urile ramase */

			working_threads--;
			if (!working_threads)
				pthread_cond_signal(&threads_idle);
			pthread_mutex_unlock(&threadpool->taskLock);
		}
	}
	pthread_mutex_lock(&threadpool->taskLock);
	threadpool->num_threads--;
	pthread_mutex_unlock(&threadpool->taskLock);
	return NULL;
}

/* Functia care se ocupa de oprirea threadpool-ului si de eliberarea resurselor aferente */

void threadpool_stop(os_threadpool_t *tp, int (*processingIsDone)(os_threadpool_t *))
{
	if (processingIsDone(tp)) {
		pthread_mutex_lock(&tp->taskLock);
		tp->should_stop = 1;
		pthread_cond_broadcast(&threads_idle);
		pthread_mutex_unlock(&tp->taskLock);
		os_task_queue_t *node = tp->tasks;

		/* Se elibereaza memoria aferenta task-urilor */

		while (node) {
			os_task_queue_t *aux = node->next;

			free(node->task->argument);
			free(node->task);
			free(node);
			node = aux;
		}

		/* Se asteapta thread-urile facute */

		for (int i = 0; i < tp->num_threads; ++i)
			pthread_join(tp->threads[i], NULL);
		free(tp->threads);
		pthread_cond_destroy(&threads_idle);
		pthread_mutex_destroy(&tp->taskLock);
		free(tp);
	}
}

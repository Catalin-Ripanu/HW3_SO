// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include "helpers.h"

#include "os_graph.h"
#include "os_list.h"

#define MAX_TASK 1000
#define MAX_THREAD 4

int sum;
os_graph_t *graph;
os_threadpool_t *thread_pool;
pthread_mutex_t lock_process;
pthread_mutex_t lock_task_func;

/* Functia care construieste cate un task pentru fiecare nod din graf */

void process_graph(int node_idx)
{
	pthread_mutex_lock(&lock_process);
	if (!graph->visited[node_idx]) {
		graph->visited[node_idx] = 1;
		os_node_t *graph_node = os_create_node(graph->nodes[node_idx]->nodeID, graph->nodes[node_idx]->nodeInfo);

		os_task_t *task = task_create(graph_node, task_func);

		add_task_in_queue(thread_pool, task);
	}
	pthread_mutex_unlock(&lock_process);
}

/* Functia care verifica daca toate nodurile au fost procesate */

int process_done(os_threadpool_t *tp)
{
	if (!tp->tasks) {
		for (int i = 0; i < graph->nCount; i++)
			for (int j = 0; j < graph->nodes[i]->cNeighbours; j++) {
				if (!graph->visited[graph->nodes[i]->neighbours[j]])
					return 0;
			}
	}
	return 1;
}

/* Functia care modeleaza un task generic dat threadpool-ului */

void task_func(void *arg)
{
	os_node_t *graph_node = (os_node_t *)arg;

	pthread_mutex_lock(&lock_task_func);
	sum += graph_node->nodeInfo;
	pthread_mutex_unlock(&lock_task_func);

	/* Se adauga toti vecinii nodului anterior procesat */

	for (int i = 0; i < graph->nodes[graph_node->nodeID]->cNeighbours; i++)
		process_graph(graph->nodes[graph_node->nodeID]->neighbours[i]);
}

/* Functia care se ocupa de initializari, de executii si de eliberarea resurselor alocate */

int main(int argc, char *argv[])
{
	sum = 0;
	if (argc != 2) {
		printf("Usage: ./%s input_file\n", __func__);
		exit(1);
	}

	FILE *input_file = fopen(argv[1], "r");

	if (input_file == NULL) {
		printf("[Error] Can't open file\n");
		return -1;
	}

	graph = create_graph_from_file(input_file);
	if (graph == NULL) {
		printf("[Error] Can't read the graph from file\n");
		return -1;
	}

	pthread_mutex_init(&lock_process, NULL);
	pthread_mutex_init(&lock_task_func, NULL);

	thread_pool = threadpool_create(MAX_TASK, MAX_THREAD);
	for (int i = 0; i < graph->nCount; i++)
		process_graph(i);

	thread_pool_wait(thread_pool);
	threadpool_stop(thread_pool, process_done);

	pthread_mutex_destroy(&lock_process);
	pthread_mutex_destroy(&lock_task_func);

	printf("%d", sum);
	return 0;
}

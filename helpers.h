#ifndef __SO_HELPERS_H__
#define __SO_HELPERS_H__
#include "os_threadpool.h"

void thread_pool_wait(os_threadpool_t *thpool_p);
void task_func(void *arg);
os_task_queue_t *get_task_queue(os_threadpool_t *tp);

#endif

#include "os_threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* === TASK === */

/* Creates a task that thread must execute */

/**
 * @brief Create a new task with the given argument and function.
 * 
 * @attention The argument must be malloc-ed by the caller because it 
 * will be freed by the threadpool.
 * 
 * @param arg Argument of the task
 * @param f Function of the task
 * 
 * @return os_task_t* Pointer to the created task
 */
os_task_t *task_create(void *arg, void (*f)(void *))
{
    os_task_t *t = (os_task_t *)malloc(sizeof(os_task_t));
    t->argument = arg;
    t->task = f;

    return t;
}

/* Add a new task to threadpool task queue */

/**
 * @brief Add a new task to threadpool task queue.
 * 
 * @param tp Threadpool
 * @param t Task
 */
void add_task_in_queue(os_threadpool_t *tp, os_task_t *t)
{
    os_task_queue_t *new_task = (os_task_queue_t *)malloc(sizeof(os_task_queue_t));
    new_task->task = t;
    new_task->next = NULL;

    // Lock the task queue
    pthread_mutex_lock(&tp->taskLock);
    os_task_queue_t *task_queue = tp->tasks;
    // Add the new task at the end of the queue
    if (task_queue == NULL) { // If the queue is empty
        tp->tasks = new_task;
    } else {
        while (task_queue->next != NULL) {
            task_queue = task_queue->next;
        }
        task_queue->next = new_task;
    }
    pthread_mutex_unlock(&tp->taskLock);
}

/* Get the head of task queue from threadpool */

/**
 * @brief Get the head of task queue from threadpool.
 * 
 * @param tp Threadpool
 * 
 * @return os_task_t* Pointer to the task
 */
os_task_t *get_task(os_threadpool_t *tp)
{
    // Lock the task queue
    pthread_mutex_lock(&tp->taskLock);
    os_task_queue_t *task_queue = tp->tasks;
    if (task_queue == NULL) {
        pthread_mutex_unlock(&tp->taskLock);
        return NULL; // There is no task in the queue
    }

    // Get the head of the queue and remove it
    os_task_t *task = task_queue->task;
    tp->tasks = task_queue->next;
    pthread_mutex_unlock(&tp->taskLock);

    return task;
}

/* === THREAD POOL === */

/* Initialize the new threadpool */

/**
 * @brief Create a new threadpool with the given number of maximum tasks and threads.
 * 
 * @param nTasks Number of maximum tasks
 * @param nThreads Number of maximum threads
 * 
 * @return os_threadpool_t* Pointer to the created threadpool
 */
os_threadpool_t *threadpool_create(unsigned int nTasks, unsigned int nThreads)
{
    os_threadpool_t *tp = (os_threadpool_t *)malloc(sizeof(os_threadpool_t));
    tp->should_stop = 0;
    tp->num_threads = nThreads;
    tp->threads = (pthread_t *)malloc(nThreads * sizeof(pthread_t));
    tp->tasks = NULL;
    pthread_mutex_init(&tp->taskLock, NULL);

    for (int i = 0; i < nThreads; i++) {
        // Creathe the threads which will run the tasks
        pthread_create(&tp->threads[i], NULL, thread_loop_function, tp);
    }

    return tp;
}

/* Loop function for threads */

/**
 * @brief Function that threads will execute. It gets tasks from the task queue
 * and executes them. If there is no task in the queue, it waits until a task
 * is added. If the threadpool is signaled to stop, it stops.
 * 
 * @attention Any remaining tasks in the queue will still be executed after the
 * threadpool is signaled to stop.
 * 
 * @param args Threadpool
 * 
 * @return void* NULL
 */
void *thread_loop_function(void *args)
{
    os_threadpool_t *parrent_tp = (os_threadpool_t *)args;

    // Continuously get tasks from the queue and execute them
    while (1) {
        os_task_t *task = get_task(parrent_tp);
        if (task == NULL) {
            pthread_mutex_lock(&parrent_tp->taskLock);
            if (parrent_tp->should_stop) { // Check if the threadpool should stop
                pthread_mutex_unlock(&parrent_tp->taskLock);
                break;
            }
            pthread_mutex_unlock(&parrent_tp->taskLock);
            continue;
        }

        // Execute the task and then free it
        task->task(task->argument);
        free(task->argument);
        free(task);
    }

    return NULL;
}

/* Stop the thread pool once a condition is met */

/**
 * @brief Stop the threadpool once a condition is met.
 * The condition is checked by the given function.
 * 
 * @attention The function will wait until the condition is met.
 * Any remaining tasks in the queue will still be executed after the
 * threadpool is signaled to stop.
 * 
 * @param tp Threadpool
 * @param processingIsDone Function that checks if the processing is done
 */
void threadpool_stop(os_threadpool_t *tp, int (*processingIsDone)(os_threadpool_t *))
{
    while (!processingIsDone(tp)) {
        // Wait until the condition is met
    }
    
    pthread_mutex_lock(&tp->taskLock);
    tp->should_stop = 1; // Signal the threadpool to stop
    pthread_mutex_unlock(&tp->taskLock);

    // Wait for all threads to stop
    for (int i = 0; i < tp->num_threads; i++) {
        pthread_join(tp->threads[i], NULL);
    }

    // Free the threadpool
    pthread_mutex_destroy(&tp->taskLock);
    free(tp->threads);
    free(tp);
}

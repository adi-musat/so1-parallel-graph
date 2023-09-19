#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "os_list.h"

#define MAX_TASK 100
#define MAX_THREAD 4

int sum = 0;
os_graph_t *graph;
os_threadpool_t *tp;
pthread_mutex_t sum_lock = PTHREAD_MUTEX_INITIALIZER; /* Prevent race conditions when multiple threads access the sum */
pthread_mutex_t visited_lock = PTHREAD_MUTEX_INITIALIZER; /* Prevent race conditions when multiple threads access the visited vector */


/** 
 * @brief Process a node and add its neighbours in the queue.
 * Task argument (node index) is malloc-ed because it will be freed
 * by the threadpool.
 * 
 * @param arg Node index
 */
void processNode(void *arg)
{
    unsigned int nodeIdx = *(unsigned int *)arg;
    os_node_t *node = graph->nodes[nodeIdx];

    pthread_mutex_lock(&sum_lock);
    sum += node->nodeInfo;
    pthread_mutex_unlock(&sum_lock);

    for (int i = 0; i < node->cNeighbours; i++) {

        pthread_mutex_lock(&visited_lock);
        if (graph->visited[node->neighbours[i]] == 0) {
            graph->visited[node->neighbours[i]] = 1;

            pthread_mutex_unlock(&visited_lock);
            int *nodeIdx = malloc(sizeof(int)); // Allocate memory for the node index
            *nodeIdx = node->neighbours[i];

            os_task_t *task = task_create((void *)nodeIdx, processNode); // Create a new task for the neighbour
            add_task_in_queue(tp, task); // Add the task in the threadpool queue
        } else {
            pthread_mutex_unlock(&visited_lock);
        }
    }
}


/**
 * @brief Check if the graph traversal is finished.
 * 
 * @param tp Threadpool **unused**
 * @return 1 if the graph traversal is finished, 0 otherwise
 */
int checkGraphFinished(os_threadpool_t *tp)
{
    int finished = 1;
    
    // Check if all nodes were visited
    pthread_mutex_lock(&visited_lock);
    for (int i = 0; i < graph->nCount; i++)
    {
        if (graph->visited[i] == 0) {
            finished = 0;
            break;
        }
    }
    pthread_mutex_unlock(&visited_lock);

    return finished;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: ./main input_file\n");
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

    // TODO: create thread pool and traverse the graf
    // Create threadpool
    tp = threadpool_create(MAX_TASK, MAX_THREAD);

    /* Create tasls for each node. This allows to process all the nodes from all 
    the connected components of the graph */
    for (int i = 0; i < graph->nCount; i++)
    {
        pthread_mutex_lock(&visited_lock);
        if (graph->visited[i] == 0) {
            graph->visited[i] = 1;

            pthread_mutex_unlock(&visited_lock);
            int *nodeIdx = malloc(sizeof(int)); // Allocate memory for the node index
            *nodeIdx = i;

            os_task_t *task = task_create((void *)nodeIdx, processNode); // Create a new task for the node
            add_task_in_queue(tp, task); // Add the task in the threadpool queue
        } else {
            pthread_mutex_unlock(&visited_lock);
        }
    }

    // Stop the threadpool when the graph traversal is finished
    threadpool_stop(tp, checkGraphFinished);

    pthread_mutex_destroy(&sum_lock);
    pthread_mutex_destroy(&visited_lock);

    printf("%d", sum);
    return 0;
}

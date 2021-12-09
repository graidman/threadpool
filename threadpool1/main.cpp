#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include<pthread.h>

#define LL_ADD(item,list)do{\
item->prev = NULL;  \
item->next = list;  \
if(list != NULL)    list->prev = item;  \
list = item;    \
}while(0)

#define LL_REMOVE(item,list)do{\
if(item->prev != NULL)  item->prev->next = item->next;\
if(item->next != NULL)  item->next->prev = item->prev;\
if(list == item) list = item->next; \
item->prev = item->next = NULL; \
}while(0)

typedef struct NWORKER {//职员
    pthread_t id;
    int terminate;

    struct NTHREADPOOL* pool;

    struct NWORKER* prev;
    struct NWORKER* next;
}nworker;

typedef struct NJOB {//任务
    void (*job_func)(struct NJOB* job);//任务函数
    void* user_data;

    struct NJOB* prev;
    struct NJOB* next;
}njob;

typedef struct NTHREADPOOL {//线程池
    struct NWORKER* workers;
    struct NJOB* wait_jobs;

    pthread_cond_t cond;
    pthread_mutex_t mtx;
}nthreadpool;

void* thread_callback(void* arg) {
    nworker* worker = (nworker*)arg;

    //jobs
    //epoll_wait();

    while (1) {
        pthread_mutex_lock(&worker->pool->mtx);//锁住mtx
        while (worker->pool->wait_jobs == NULL) {//当等待队列为空时
            if (worker->terminate)   break;
            pthread_cond_wait(&worker->pool->cond, &worker->pool->mtx);//没有想要的就等
        }
        if (worker->terminate) {
            pthread_mutex_unlock(&worker->pool->mtx);//解锁
            break;
        }

        njob* job = worker->pool->wait_jobs;//从队列中取一个任务
        if (job) {
            LL_REMOVE(job, worker->pool->wait_jobs);
        }
        pthread_mutex_unlock(&worker->pool->mtx);//解锁

        if (job == NULL) continue;

        job->job_func(job);//回调

    }

    free(worker);
    //pthread
}

int thread_pool_create(nthreadpool* pool, int num_thread) {//创建线程池
    if (pool == NULL)    return -1;//pool为空就返回-1

    if (num_thread < 1)   num_thread = 1;
    memset(pool, 0, sizeof(nthreadpool));//重置内存

    //cond
    pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;//创建互斥量cond
    memcpy(&pool->cond, &blank_cond, sizeof(pthread_cond_t));//把blank_cond复制到pool->cond

    //mutex
    pthread_mutex_t blank_mtx = PTHREAD_MUTEX_INITIALIZER;
    memcpy(&pool->mtx, &blank_mtx, sizeof(pthread_mutex_t));

    //thread count
    //(thread,worker)
    int idx = 0;
    for (idx = 0; idx < num_thread; idx++) {
        nworker* worker = (nworker*)malloc(sizeof(nworker));//分配内存
        if (worker == NULL) {
            perror("malloc");
            return idx;
        }
        memset(worker, 0, sizeof(nworker));

        worker->pool = pool;

        int ret = pthread_create(&worker->id, NULL, thread_callback, worker);//创建线程
        if (ret) {
            perror("pthread_create");
            free(worker);
            return idx;
        }
        LL_ADD(worker, pool->workers);
    }
    return idx;
}

int thread_pool_destroy(nthreadpool* pool) {//线程池销毁
    nworker* worker = NULL;
    for (worker = pool->workers; worker != NULL; worker = worker->next) {
        worker->terminate = 1;
    }

    pthread_mutex_lock(&pool->mtx);
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mtx);
}

int thread_pool_push_job(nthreadpool* pool, njob* job) {
    pthread_mutex_lock(&pool->mtx);
    LL_ADD(job, pool->wait_jobs);
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mtx);
}

#define TASK_COUNT  1000

void counter(njob* job) {
    if (job == NULL) return;
    int idx = *(int*)job->user_data;

    printf("idx:%d,selfid:%lu\n", idx, pthread_self());
    free(job->user_data);
    free(job);
}

int main()
{
    nthreadpool pool = { 0 };
    int num_thread = 50;

    thread_pool_create(&pool, num_thread);
    int i = 0;
    for (i = 0; i < TASK_COUNT; i++) {
        njob* job = (njob*)malloc(sizeof(njob));
        if (job == NULL) exit(1);
        job->job_func = counter;
        job->user_data = malloc(sizeof(int));
        *(int*)job->user_data = i;

        thread_pool_push_job(&pool, job);
    }
    getchar();
}

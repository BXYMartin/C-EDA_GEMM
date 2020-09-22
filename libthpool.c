//
//  main.c
//  libthpool
//
//  Created by BXYMartin on 2020/11/12.
//  Copyright © 2020 BXYMartin. All rights reserved.
//

#include <stdio.h>
#include<unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "libthpool.h"

//pthread_t g_manage_thread;
//pthread_t *g_work_threads;

/*线程池管理*/
struct threadpool_t{
    pthread_mutex_t lock;                 /* 锁住整个结构体 */
    pthread_mutex_t thread_counter;       /* 用于使用忙线程数时的锁 */
    pthread_cond_t  queue_not_full;       /* 条件变量，任务队列不为满 */
    pthread_cond_t  queue_not_empty;      /* 任务队列不为空 */
    
    pthread_t *threads;                   /* 存放线程的tid,实际上就是管理了线 数组 */
    pthread_t admin_tid;                  /* 管理者线程tid */
    libthpool_taskdata_t *task_queue;        /* 任务队列 */
    
    /*线程池信息*/
    int min_thr_num;                      /* 线程池中最小线程数 */
    int max_thr_num;                      /* 线程池中最大线程数 */
    int live_thr_num;                     /* 线程池中存活的线程数 */
    int busy_thr_num;                     /* 忙线程，正在工作的线程 */
    int wait_exit_thr_num;                /* 需要销毁的线程数 */
    
    /*任务队列信息*/
    int queue_front;                      /* 队头 */
    int queue_rear;                       /* 队尾 */
    int queue_size;
    
    /* 存在的任务数 */
    int queue_max_size;                   /* 队列能容纳的最大任务数 */
    /*线程池状态*/
    int shutdown;                         /* true为关闭 */
};

#define DEFAULT_CHECK_INTERVAL 0
#define DEFAULT_THREAD_NUM 1
#define MIN_WAIT_TASK_NUM  0

#define MIN_THREAD_NUM  1
#define MAX_THREAD_NUM  2
#define MAX_QUEUE_SIZE  30

#ifndef ESRCH
#define ESRCH 3
#endif

/*工作线程*/
void * work_thread(void *threadpool);

/*线程是否存活*/
static int is_thread_alive(pthread_t tid)
{
    int kill_rc = pthread_kill(tid, 0);     //发送0号信号，测试是否存活
    if (kill_rc == ESRCH)  //线程不存在
    {
        return 0;
    }
    return 1;
}


/*管理线程*/
int g_queue_size,g_live_thr_num,g_busy_thr_num=0;
static void * admin_thread(void *threadpool)
{
    int i;
    threadpool_t *pool = (threadpool_t *)threadpool;
    while (!pool->shutdown)
    {
        //printf("admin -----------------\n");
        sleep(DEFAULT_CHECK_INTERVAL);               /*隔一段时间再管理*/
        pthread_mutex_lock(&(pool->lock));               /*加锁*/
        g_queue_size = pool->queue_size;               /*任务数*/
        g_live_thr_num = pool->live_thr_num;           /*存活的线程数*/
        pthread_mutex_unlock(&(pool->lock));             /*解锁*/
        
        pthread_mutex_lock(&(pool->thread_counter));
        g_busy_thr_num = pool->busy_thr_num;           /*忙线程数*/
        pthread_mutex_unlock(&(pool->thread_counter));
        
        //printf("admin busy live -%d--%d-\n", g_busy_thr_num, g_live_thr_num);
        /*创建新线程 实际任务数量大于 最小正在等待的任务数量，存活线程数小于最大线程数*/
        if (g_queue_size >= MIN_WAIT_TASK_NUM && g_live_thr_num <= pool->max_thr_num)
        {
            //printf("admin add-----------\n");
            pthread_mutex_lock(&(pool->lock));
            int add=0;
            
            /*一次增加 DEFAULT_THREAD_NUM 个线程*/
            for (i=0; i<pool->max_thr_num && add<DEFAULT_THREAD_NUM
                 && pool->live_thr_num < pool->max_thr_num; i++)
            {
                if (pool->threads[i] == 0 || !is_thread_alive(pool->threads[i]))
                {
                    pthread_create(&(pool->threads[i]), NULL, work_thread, (void *)pool);
                    add++;
                    pool->live_thr_num++;
                    //printf("new thread start-----------------------\n");
                }
            }
            
            pthread_mutex_unlock(&(pool->lock));
        }
        
        /*销毁多余的线程 忙线程x2 都小于 存活线程，并且存活的大于最小线程数*/
        if ((g_busy_thr_num*2) < g_live_thr_num  &&  g_live_thr_num > pool->min_thr_num)
        {
            // printf("admin busy --%d--%d----\n", busy_thr_num, live_thr_num);
            /*一次销毁DEFAULT_THREAD_NUM个线程*/
            pthread_mutex_lock(&(pool->lock));
            pool->wait_exit_thr_num = DEFAULT_THREAD_NUM;
            pthread_mutex_unlock(&(pool->lock));
            
            for (i=0; i<DEFAULT_THREAD_NUM; i++)
            {
                //通知正在处于空闲的线程，自杀
                pthread_cond_signal(&(pool->queue_not_empty));
                //printf("admin cler --\n");
            }
        }
        
    }
    
    return NULL;
}

int libthpool_state(threadpool_t *pool)
{
    int flag = 0;
    pthread_mutex_lock(&(pool->lock));
    if(pool->queue_size == 0 && pool->busy_thr_num == 0 && pool->live_thr_num == MIN_THREAD_NUM){
        flag = 1;
    }
    pthread_mutex_unlock(&(pool->lock));
    return flag;
}
/*释放线程池*/
int threadpool_free(threadpool_t *pool)
{
    if (pool == NULL)
        return -1;
    if (pool->task_queue){
        free(pool->task_queue);
        pool->task_queue = NULL;
    }
    if (pool->threads)
    {
        free(pool->threads);
        pthread_mutex_lock(&(pool->lock));               /*先锁住再销毁*/
        pthread_mutex_destroy(&(pool->lock));
        pthread_mutex_lock(&(pool->thread_counter));
        pthread_mutex_destroy(&(pool->thread_counter));
        pthread_cond_destroy(&(pool->queue_not_empty));
        pthread_cond_destroy(&(pool->queue_not_full));
    }
    free(pool);
    pool = NULL;
    
    return 0;
}


/*工作线程*/
void *work_thread(void *threadpool)
{
    threadpool_t *pool = (threadpool_t *)threadpool;
    libthpool_taskdata_t task;
    
    while(1)
    {
        pthread_mutex_lock(&(pool->lock));
        
        /* 无任务则阻塞在 “任务队列不为空” 上，有任务则跳出 */
        while ((pool->queue_size == 0) && (!pool->shutdown))
        {
            //printf("thread 0x%x is waiting \n", (unsigned int)pthread_self());
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->lock));
            
            /* 判断是否需要清除线程,自杀功能 */
            if (pool->wait_exit_thr_num > 0)
            {
                pool->wait_exit_thr_num--;
                /* 判断线程池中的线程数是否大于最小线程数，是则结束当前线程 */
                if (pool->live_thr_num > pool->min_thr_num)
                {
                    //printf("thread 0x%x is exiting \n", (unsigned int)pthread_self());
                    pool->live_thr_num--;
                    pthread_mutex_unlock(&(pool->lock));
                    pthread_exit(NULL);//结束线程
                }
            }
        }
        
        /* 线程池开关状态 */
        if (pool->shutdown) //关闭线程池
        {
            pthread_mutex_unlock(&(pool->lock));
            //printf("thread 0x%x is exiting \n", (unsigned int)pthread_self());
            pthread_exit(NULL); //线程自己结束自己
        }
        
        //否则该线程可以拿出任务
        task.task_func = pool->task_queue[pool->queue_front].task_func; //出队操作
        task.arg = pool->task_queue[pool->queue_front].arg;
        
        pool->queue_front = (pool->queue_front + 1) % pool->queue_max_size;  //环型结构
        pool->queue_size--;
        
        //通知可以添加新任务
        pthread_cond_broadcast(&(pool->queue_not_full));
        
        //释放线程锁
        pthread_mutex_unlock(&(pool->lock));
        
        //执行刚才取出的任务
        //printf("thread 0x%x start working \n", (unsigned int)pthread_self());
        pthread_mutex_lock(&(pool->thread_counter));            //锁住忙线程变量
        pool->busy_thr_num++;
        pthread_mutex_unlock(&(pool->thread_counter));
        
        (*(task.task_func))(task.arg);                           //执行任务
        
        //任务结束处理
        //printf("thread 0x%x end working \n", (unsigned int)pthread_self());
        pthread_mutex_lock(&(pool->thread_counter));
        pool->busy_thr_num--;
        pthread_mutex_unlock(&(pool->thread_counter));
    }
    
    pthread_exit(NULL);
}

/*向线程池的任务队列中添加一个任务*/
int
libthpool_task_put(threadpool_t *pool, task_func function, void *arg)
{
    pthread_mutex_lock(&(pool->lock));
    
    /*如果队列满了,调用wait阻塞*/
    while ((pool->queue_size == pool->queue_max_size) && (!pool->shutdown)){
        //printf("queue is full... waitting...\n");
        pthread_cond_wait(&(pool->queue_not_full), &(pool->lock));
    }
    
    /*如果线程池处于关闭状态*/
    if (pool->shutdown)
    {
        //printf("libtreadpool will be shutdown!\n");
        pthread_mutex_unlock(&(pool->lock));
        return -1;
    }
    
    /*清空工作线程的回调函数的参数arg*/
    if (pool->task_queue[pool->queue_rear].arg != NULL)
    {
        free(pool->task_queue[pool->queue_rear].arg);
        pool->task_queue[pool->queue_rear].arg = NULL;
    }
    
    /*添加任务到任务队列*/
    pool->task_queue[pool->queue_rear].task_func = function;
    pool->task_queue[pool->queue_rear].arg = arg;
    pool->queue_rear = (pool->queue_rear + 1) % pool->queue_max_size;  /* 逻辑环  */
    pool->queue_size++;
    
    /*添加完任务后,队列就不为空了,唤醒线程池中的一个线程*/
    pthread_cond_signal(&(pool->queue_not_empty));
    pthread_mutex_unlock(&(pool->lock));
    
    return 0;
}


/*创建线程池*/
threadpool_t * libthpool_init(int min_thr_num, int max_thr_num, int queue_max_size)
{
    int i;
    threadpool_t *pool = NULL;
    do
    {
        /* 线程池空间开辟 */
        if ((pool=(threadpool_t *)malloc(sizeof(threadpool_t))) == NULL)
        {
            fprintf(stderr,"malloc threadpool false; \n");
            exit(-1);
           
        }
        memset(pool , 0,sizeof(threadpool_t));
        /*信息初始化*/
        pool->min_thr_num = min_thr_num;
        pool->max_thr_num = max_thr_num;
        pool->busy_thr_num = 0;
        pool->live_thr_num = min_thr_num;
        pool->wait_exit_thr_num = 0;
        pool->queue_front = 0;
        pool->queue_rear = 0;
        pool->queue_size = 0;
        pool->queue_max_size = queue_max_size;
        pool->shutdown = 0;
        
        /* 根据最大线程数，给工作线程数组开空间，清0 */
        pool->threads = (pthread_t *)malloc(sizeof(pthread_t)*max_thr_num);
        if (pool->threads == NULL)
        {
            fprintf(stderr,"malloc threads false;\n");
            exit(-1);
        }
        memset(pool->threads, 0, sizeof(pthread_t)*max_thr_num);
        
        /* 队列开空间 */
        pool->task_queue =
        (libthpool_taskdata_t *)malloc(sizeof(libthpool_taskdata_t)*queue_max_size);
        if (pool->task_queue == NULL)
        {
            fprintf(stderr,"malloc task queue false;\n");
            exit(-1);
        }
        memset(pool->task_queue,0,sizeof(libthpool_taskdata_t)*queue_max_size);
        /* 初始化互斥锁和条件变量 */
        if (pthread_mutex_init(&(pool->lock), NULL) != 0           ||
            pthread_mutex_init(&(pool->thread_counter), NULL) !=0  ||
            pthread_cond_init(&(pool->queue_not_empty), NULL) !=0  ||
            pthread_cond_init(&(pool->queue_not_full), NULL) !=0)
        {
            //printf("init lock or cond false;\n");
            break;
        }
        
        /* 启动min_thr_num个工作线程 */
        for (i=0; i<min_thr_num; i++)
        {
            /* pool指向当前线程池  threadpool_thread函数在后面讲解 */
            pthread_create(&(pool->threads[i]), NULL, work_thread, (void *)pool);
            //printf("start thread num %d...0x%x \n",i, (unsigned int)pool->threads[i]);
        }
        /* 管理者线程 admin_thread函数在后面讲解 */
        pthread_create(&(pool->admin_tid), NULL, admin_thread, (void *)pool);
        
        return pool;
    } while(0);
    
    /* 释放pool的空间 */
    threadpool_free(pool);
    return NULL;
}

/*销毁线程池*/
int libthpool_destroy(threadpool_t *pool)
{
    int i;
    if (pool == NULL)
    {
        return OPCODE_ERR_PARA;
    }
    pool->shutdown = 1;
    
    /*销毁管理者线程*/
    pthread_join(pool->admin_tid, NULL);
    
    //通知所有线程去自杀(在自己领任务的过程中)
    for (i=0; i<pool->live_thr_num; i++)
    {
        pthread_cond_broadcast(&(pool->queue_not_empty));
    }
    
    /*等待线程结束 先是pthread_exit 然后等待其结束*/
    for (i=0; i<pool->live_thr_num; i++)
    {
        pthread_join(pool->threads[i], NULL);
    }
    
    threadpool_free(pool);
    return OPCODE_SUCCESS;
}

void handle_taskA_func(void *arg)
{
    TaskMatrixInfoA* info =(TaskMatrixInfoA*) arg;
    {
        for (int it = 0; it < info->rowArraySize; ++it)
        {
            const int node = info->rowArray[it];
            for (int j = info->rowOffset[node]; j < info->rowOffset[node + 1]; ++j)
            {
                info->Id[node] += info->valueNormalMatrix[j] * info->S[info->columnIndice[j]];
            }
        }
    }
}

void handle_taskB_func(void *arg)
{
    TaskMatrixInfoB* info =(TaskMatrixInfoB*) arg;
    {
        for (int it = 0; it < info->rowArraySize; ++it)
        {
            int row = info->rowArray[it];
            for (int p = info->rowOffset[row]; p < info->rowOffset[row + 1]; ++p)
            {
                int col = info->columnIndice[p];
                const int k = p * 2;
                double cond = info->valueSpiceMatrix[k];
                double cap = info->valueSpiceMatrix[k + 1];
                info->IG[row] += cond * info->S[col];
                info->IC[row] += cap * info->S[col];
            }
        }
        for (int it = 0; it < info->rowArraySize; ++it)
        {
            int row = info->rowArray[it];
            const int kl = row * 2;
            double current = info->D[kl];
            double charge = info->D[kl + 1];
            for (int p = info->rowOffset[row]; p < info->rowOffset[row + 1]; ++p)
            {
                int col = info->columnIndice[p];
                const int k = p * 2;
                current -= info->valueSpiceMatrix[k] * info->S[col];
                charge -= info->valueSpiceMatrix[k + 1] * info->S[col];
            }
            info->R[row] = current;
            info->H[row] = charge;
        }
    }
}

void matrix_calc_taskA(TaskMatrixInfoA** listDataList, int N)
{
    threadpool_t *ths = NULL;
    ths = libthpool_init(MIN_THREAD_NUM, MAX_THREAD_NUM, MAX_QUEUE_SIZE);
    for (int i = 0; i < N; ++i)
    {
        libthpool_task_put(ths, handle_taskA_func, listDataList[i]);
    }
    while(1){
        if(libthpool_state(ths)){
            break;
        }
        sleep(0);
    }
    libthpool_destroy(ths);
}

void matrix_calc_taskB(TaskMatrixInfoB** listDataList, int N)
{
    threadpool_t *ths = NULL;
    ths = libthpool_init(10, 20, 30);
    for (int i = 0; i < N; ++i)
    {
        libthpool_task_put(ths, handle_taskB_func, listDataList[i]);
    }
    while(1){
        if(libthpool_state(ths)){
            break;
        }
        sleep(0);
    }
    libthpool_destroy(ths);
}


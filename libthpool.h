//
//  libthpool.h
//  libthpool
//
//  Created by 陈健康 on 2019/11/12.
//  Copyright © 2019 陈健康. All rights reserved.
//

#ifndef libthpool_h
#define libthpool_h

#define OPCODE_SUCCESS 0
#define OPCODE_FAILURE -1
#define OPCODE_ERR_PARA -2
#define OPCODE_ERR_BIND_DEV -3
#define OPCODE_ERR_BIND_PORT -4
#define OPCODE_ERR_ILLEGAL_FD -5
#define OPCODE_ERR_BUSY -6

typedef struct threadpool_t threadpool_t;
typedef void(*task_func)(void *arg);

typedef struct task_data{
    //this arg must set like malloc,libthpool will call free(arg)
    void *arg;
    task_func task_func;
}libthpool_taskdata_t;

threadpool_t * libthpool_init(int min_thr_num, int max_thr_num, int queue_max_size);
int libthpool_destroy(threadpool_t *pool);
int libthpool_task_put(threadpool_t *pool, task_func function, void *arg);
int libthpool_state(int *task_queue,int *live_thr_num,int *busy_thr_num);
#endif /* libthpool_h */

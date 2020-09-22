//
//  libthpool.h
//  libthpool
//
//  Created by BXYMartin on 2020/11/12.
//  Copyright © 2020 BXYMartin. All rights reserved.
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
int libthpool_state(threadpool_t *pool);

// Exposed Functions
typedef struct {
    int*            rowArray;
    const int*      rowOffset;
    int             rowArraySize;
    const int*      columnIndice;
    const double*   S;
    const double*   valueNormalMatrix;
    double*         Id;
} TaskMatrixInfoA;

void matrix_calc_taskA(TaskMatrixInfoA** listDataList, int N);

typedef struct {
    const double*       valueSpiceMatrix;
    const int*          rowOffset;
    const int*          columnIndice;
    double*             A;
    double*             S;
    double*             R;
    double*             H;
    double*             D;
    double*             IC;
    double*             IG;
    double*             alpha;
    int*                rowArray;
    int                 rowArraySize;
    void*               hdl;
} TaskMatrixInfoB;

void matrix_calc_taskB(TaskMatrixInfoB** listDataList, int N);

#endif /* libthpool_h */

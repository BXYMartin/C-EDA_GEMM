//
//  test.c
//  libthpool
//
//  Created by 陈健康 on 2019/11/12.
//  Copyright © 2019 陈健康. All rights reserved.
//

#include "libthpool.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

void handle_task_func(void *arg)
{
    int s =(*(int*)arg) % 6;
    if(s==0)
        s=1;
    printf("func handle start...arg=%d..sleep=%d..\n",*(int*)arg,s);
    sleep(s);
    printf("func handle end.......\n");
    return ;
}

int main()
{
    threadpool_t *ths = NULL;
    int data=0,*data_ptr=NULL;
    int queue_size,live_thr,busy_thr;
    ths = libthpool_init(10, 20, 30);
    while(data<100){
        data++;
        data_ptr = (int*)malloc(sizeof(int));
        memset(data_ptr,0,sizeof(int));
        *data_ptr = data;
        libthpool_task_put(ths, handle_task_func, data_ptr);
        //sleep(1);
    }
    while(1){
        libthpool_state(&queue_size, &live_thr, &busy_thr);
        if(queue_size == 0 && busy_thr == 0 && live_thr == 10){
            break;
        }
        sleep(1);
    }
    libthpool_destroy(ths);
    return 0;
}




#include "list.h"

#ifndef _KERNEL_SEMAPHORE_
#define _KERNEL_SEMAPHORE_

struct semaphore
{
    struct spinlock lk;
    int count;
    struct list_head wait_queue;

    // debug (for sleeping lock)
    int is_sleeplock;
    char *name;
    int locked;
    int pid;
    int seq;
};

#endif
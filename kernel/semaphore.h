#include "list.h"

#ifndef _KERNEL_SEMAPHORE_
#define _KERNEL_SEMAPHORE_

struct semaphore
{
    struct spinlock lk;
    int count;
    struct list_head wait_queue;

    // debug (for sleeping lock)
    char *name;
    int locked;
    int pid;
};

#endif
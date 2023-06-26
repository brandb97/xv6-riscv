#ifndef _KERNEL_SEMAPHORE_
#define _KERNEL_SEMAPHORE_

struct semaphore
{
    struct spinlock lk;
    int count;
    struct list_head wait_queue;
};

#endif
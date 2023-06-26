// wait queue and semaphore all implement here
// because now only semaphore use wait queue

#include "list.h"
#include "proc.h"
#include "spinlock.h"
#include "semaphore.h"

static void add_to_wait_queue(struct list_head *);
static void remove_from_wait_queue(struct list_head *);
static void wake_up_queue(struct list_head *);

// not going to use linux 2.6.11 implementation
// which may be add later
void
initsemaphore(struct semaphore *semp, int count)
{
    semp->count = count;
    // may be buggy here, check this later
    initlock(&semp->lk, "semphore_lock");
    // may be buggy here, check this later
    INIT_LIST_HEAD(&semp->wait_queue);
}

void
down(struct semaphore *semp)
{
    acquire(&semp->lk);
    semp->count--;
    if (semp->count < 0) {
        add_to_wait_queue(&semp->wait_queue);
        release(&semp->lk);
        schedule();
        acquire(&semp->lk);
        remove_from_wait_queue(&semp->wait_queue);
    }
    release(&semp->lk);
}

void
up(struct semaphore *semp)
{
    acquire(&semp->lk);
    semp->count++;
    if (semp->count <= 0) {
        wake_up_queue(&semp->wait_queue);
    }
    release(&semp->lk);
}

static void
add_to_wait_queue(struct list_head *wq)
{
    // get current proc
    struct proc *p = myproc();
    // disable intr? I don't know, fix it later
    // add it
    list_add(&p->next, wq);
}

static void
remove_from_wait_queue(struct list_head *wq)
{
    // get current proc
    struct proc *p = myproc();
    // remove it
    list_del(&p->next);
}

static void
wake_up_queue(struct list_head *wq)
{
    // scan queue and `wake up` a blocked one
    // wake up means change its state from 
    // block to running

    struct proc *pos;
    list_for_each_entry(pos, wq, next) {
        if (pos->state == SLEEPING) {
            pos->state = RUNNABLE;
            break;
        }
    }
}
// wait queue and semaphore all implement here
// because now only semaphore use wait queue

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "semaphore.h"

static void add_to_wait_queue(struct list_head *);
static void remove_from_wait_queue(struct list_head *);
static void wake_up_queue(struct list_head *);
static int streql(char *, char *);

// not going to use linux 2.6.11 implementation
// which may be add later
void
initsemaphore(struct semaphore *semp, int count, char *name)
{
    semp->count = count;
    semp->locked = 0;
    semp->pid = -1;
    semp->name = name? name: "semaphore lock";
    // May be buggy here, check this later
    // I do think here is just debug use
    // But let's use macro to fix it
    initlock(&semp->lk, "semaphore lock");
    // may be buggy here, check this later
    INIT_LIST_HEAD(&semp->wait_queue);
}

// Only consider wait queue..., so it's buggy!
// change the state first
void
down(struct semaphore *semp)
{
    struct proc *p = myproc();

    acquire(&semp->lk);
    semp->count--;
    if (semp->count < 0) {
        acquire(&p->lock);
        p->state = SLEEPING;
        release(&p->lock);

        add_to_wait_queue(&semp->wait_queue);
        acquire(&p->lock);
        release(&semp->lk);
        sched();
        acquire(&semp->lk);
        release(&p->lock);
        remove_from_wait_queue(&semp->wait_queue);
    }
    // semp->locked & semp->pid is only used
    // for sleep lock, "sleep lock" should be
    // set as macro
    if (streql(semp->name, "sleep lock")) {
        semp->locked = 1;
        semp->pid = p->pid;
    }
    release(&semp->lk);
}

// Only consider wait queue..., so it's buggy!
void
up(struct semaphore *semp)
{
    acquire(&semp->lk);
    semp->count++;
    // semp->locked & semp->pid is only used
    // for sleep lock
    if (streql(semp->name, "sleep lock")) {
        semp->locked = 0;
        semp->pid = -1;
    }
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
    printf("pid: %d go to sleep\n", p->pid);
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
        acquire(&pos->lock);
        if (pos->state == SLEEPING) {
            pos->state = RUNNABLE;
            printf("wake up pid: %d\n", pos->pid);
            release(&pos->lock);
            break;
        }
        release(&pos->lock);
    }
}

static int
streql(char *s1, char *s2)
{
    while (*s1 && *s2)
        if (*s1++ != *s2++)
            return 0;
    return !(*s1 || *s2);
}
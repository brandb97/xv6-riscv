// wait queue and semaphore all implement here
// because now only semaphore use wait queue

#include "types.h"
#include "atomic.h"
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

static int semp_seq = 0;

// not going to use linux 2.6.11 implementation
// which may be add later
void
initsemaphore(struct semaphore *semp, int count, char *name)
{
    semp->count = count;
    semp->locked = 0;
    semp->pid = -1;
    // SHOULD use macro not "semaphore lock"    
    semp->name = name? name: "semaphore lock";
    semp->is_sleeplock = streql(semp->name, "sleep lock");
    initlock(&semp->lk, name);
    acquire(&semp->lk);
    semp->seq = semp_seq++;
    release(&semp->lk);
    // may be buggy here, check this later
    INIT_LIST_HEAD(&semp->wait_queue);
}

void
down(struct semaphore *semp)
{
    struct proc *p = myproc();

    acquire(&semp->lk);
    semp->count--;
    if (semp->count < 0) {
        // printf("%d wait on %d -- %d wait\n", p->pid, semp->seq, -semp->count);
        add_to_wait_queue(&semp->wait_queue);
        acquire(&p->lock);
        p->state = UNINTERRUPT;
        release(&semp->lk);
        sched();
        acquire(&semp->lk);
        if (p->chan) {
            panic("wakeup by the wrong guy");
        }
        release(&p->lock);
        remove_from_wait_queue(&semp->wait_queue);
    }

    // printf("%d acquired %d semp -- %d wait\n", p->pid, semp->seq, -semp->count);
    if (semp->is_sleeplock) {
        if (semp->locked) {
            printf("%d want to lock %d semp which is already locked by %d\n", 
                    p->pid, semp->seq, semp->pid);
            // procdump();
            panic("sleeplocklock");
        }
        semp->locked = 1;
        semp->pid = p->pid;
    }
    release(&semp->lk);
}

void
up(struct semaphore *semp)
{
    acquire(&semp->lk);
    semp->count++;
    if (semp->is_sleeplock) {
        semp->locked = 0;
        semp->pid = -1;
    }
    if (semp->count <= 0) {
        wake_up_queue(&semp->wait_queue);
    }
    // printf("%d released %d semp\n", myproc()->pid, semp->seq);
    release(&semp->lk);
}

static void
add_to_wait_queue(struct list_head *wq)
{
    struct proc *p = myproc();
    list_add(&p->next, wq);
}

static void
remove_from_wait_queue(struct list_head *wq)
{
    struct proc *p = myproc();
    struct list_head *pos;
    int find = 0;
    list_for_each(pos, wq) {
       if (pos == &p->next) {
           find = 1;
           break;
       }
    }
    if (!find) {
       panic("remove somthing not from this queue");
    }
    list_del(&p->next);
}


// scan queue and `wake up` a blocked one
// wake up means change its state from block to runnable
static void
wake_up_queue(struct list_head *wq)
{
    struct proc *pos;
    // just for debug
    int nr_runble = 0;
    int has_wake_up = 0;
    list_for_each_entry(pos, wq, next) {
        acquire(&pos->lock);
        if (pos->state == UNINTERRUPT && !has_wake_up) {
            pos->state = RUNNABLE;
            has_wake_up = 1;
            // release(&pos->lock);
            // break; remove comment after debug
        }
        if (pos->state == RUNNABLE) {
            nr_runble++;
        }
        release(&pos->lock);
    }

    // the debug info here is only for sleeplock
    // but since there aren't any other use for
    // semaphore, I'll check them all
    if (nr_runble > 1) {
        panic("nr_runble > 1 in sleeplock wq");
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
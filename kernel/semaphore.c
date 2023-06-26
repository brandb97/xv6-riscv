// wait queue and semaphore all implement here
// because now only semaphore use wait queue

#include "list.h"
#include "proc.h"
#include "spinlock.h"
#include "semaphore.h"

// not going to use linux 2.6.11 implementation
// which may be add later
void
down(struct semaphore *semp)
{
    acquire(&semp->lk);
    semp->count--;
    if (semp->count < 0) {
        add_to_wait_queue();
        release(&semp->lk);
        schedule();
        acquire(&semp->lk);
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
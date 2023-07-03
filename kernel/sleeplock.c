// Sleeping locks
// To test the semaphore, I'd like to embed
// semaphore in sleeplock

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"

void
initsleeplock(struct sleeplock *lk, char *name)
{
  initsemaphore(&lk->semp, 1, "sleep lock");
}

void
acquiresleep(struct sleeplock *lk)
{
  down(&lk->semp);
  // panic(holdingsleep(lk));
  // printf("%d acquire sleeplock\n", myproc()->pid);
}

void
releasesleep(struct sleeplock *lk)
{
  up(&lk->semp);
  // panic(!holdingsleep);
  // printf("%d release sleeplock\n", myproc()->pid);
}

int
holdingsleep(struct sleeplock *lk)
{
  int r;
  
  acquire(&lk->semp.lk);
  r = lk->semp.locked && (lk->semp.pid == myproc()->pid);
  release(&lk->semp.lk);
  return r;
}
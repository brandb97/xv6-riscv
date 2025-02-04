#include "types.h"
#include "preempt.h"
#include "atomic.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


struct smptest 
{
  int tests[NR_CPUS];
  int samples[NR_CPUS];
};

static void do_smptest(void *info)
{
  // intr is off
  struct smptest *st = info;
  st->tests[cpuid()] +=  st->samples[cpuid()];
}

uint64
sys_smptest(void)
{
  // if (check_intr_on) {
  //   panic();
  // }
  struct smptest st;
  int i;

  for (i = 0; i < NR_CPUS; i++) {
    st.tests[i] = 0;
    st.samples[i] = i;
  }

	preempt_disable();

	intr_off();
	do_smptest(&st);
	intr_on();

	if (smp_call_function(do_smptest, &st, 1))
		panic("smptest falied");

	preempt_enable();

  for (i = 0; i < NR_CPUS; i++) {
    if (st.tests[i] != st.samples[i]) {
      return -1;
    }
  }
  printf("smptest pass.");
  return 0;
}
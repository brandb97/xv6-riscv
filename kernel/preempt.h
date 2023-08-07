#ifndef __KERNEL_PREEMPT_H
#define __KERNEL_PREEMPT_H

#define barrier() __asm__ __volatile__("": : :"memory")

# define add_preempt_count(val)	do { preempt_count() += (val); } while (0)
# define sub_preempt_count(val)	do { preempt_count() -= (val); } while (0)

#define inc_preempt_count() add_preempt_count(1)
#define dec_preempt_count() sub_preempt_count(1)

#define preempt_count()	(myproc()->preempt_count)

#define preempt_disable() \
do { \
	inc_preempt_count(); \
	barrier(); \
} while (0)

#define preempt_enable() \
do { \
	barrier(); \
	dec_preempt_count(); \
} while (0)

#endif
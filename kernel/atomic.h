#ifndef __KERNEL_ATMOC_H
#define __KERNEL_ATMOC_H

typedef struct { volatile int counter; } atomic_t;
typedef struct { volatile long counter; } atomic64_t;

// TODO: replace 'inline' to '__always_inline'
#define ATOMIC_OP(op, asm_op, I, asm_type, c_type, prefix)		\
inline void atomic##prefix##_##op(c_type i, atomic##prefix##_t *v)	__attribute__((always_inline)); \
inline						\
void atomic##prefix##_##op(c_type i, atomic##prefix##_t *v)	\
{									\
	__asm__ __volatile__ (						\
		"	amo" #asm_op "." #asm_type " zero, %1, %0"	\
		: "+A" (v->counter)					\
		: "r" (I)						\
		: "memory");						\
}

#define ATOMIC_OPS(op, asm_op, I)					\
        ATOMIC_OP (op, asm_op, I, w, int,   )				\
        ATOMIC_OP (op, asm_op, I, d, long, 64)


ATOMIC_OPS(add, add,  i)
ATOMIC_OPS(sub, add, -i)
ATOMIC_OPS(and, and,  i)
ATOMIC_OPS( or,  or,  i)
ATOMIC_OPS(xor, xor,  i)

inline int atomic_read(atomic_t *v) __attribute__((always_inline));
inline int atomic_read(atomic_t *v)
{
  return v->counter;
}

inline void atomic_set(atomic_t *v, int i) __attribute__((always_inline));
inline void atomic_set(atomic_t *v, int i)
{
  v->counter = i;
}

#define cpu_relax()

#endif
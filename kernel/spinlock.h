// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
};

#define SPIN_LOCK_UNLOCKED(n) { .locked = 0, .name = n, .cpu = 0 }
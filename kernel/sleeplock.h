#include "semaphore.h"

// Long-term locks for processes
struct sleeplock {
  struct semaphore semp;
};


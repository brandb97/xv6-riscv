#include "types.h"
#include "spinlock.h"
#include "slab.h"
#include "defs.h"


void * kmem_cache_alloc (kmem_cache_t *cachep, int flags)
{
	return __cache_alloc(cachep, flags);
}

void kmem_cache_free (kmem_cache_t *cachep, void *objp)
{
	unsigned long flags;

	local_irq_save(flags);
	__cache_free(cachep, objp);
	local_irq_restore(flags);
}

void kmem_cache_init(void)
{
}

kmem_cache_t *
kmem_cache_create (const char *name, size_t size, size_t align,
	unsigned long flags, void (*ctor)(void*, kmem_cache_t *, unsigned long),
	void (*dtor)(void*, kmem_cache_t *, unsigned long))
{
}

int kmem_cache_destroy (kmem_cache_t * cachep)
{
}
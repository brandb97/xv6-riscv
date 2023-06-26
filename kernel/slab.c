#include "riscv.h"
#include "types.h"
#include "spinlock.h"
#include "slab.h"
#include "defs.h"

// general cache IS NOT implemented
// general cache interface: kmalloc, kmfree(should be `kfree`)
void * kmalloc ()
{
	panic("kmalloc not implemented");
}

void kmfree (void *pa)
{
	panic("kmfree not implemented");
}


// alloc a slab
void * kmem_cache_alloc (kmem_cache_t *cachep, int flags)
{
	return __cache_alloc(cachep, flags);
}

// free a slab
void kmem_cache_free (kmem_cache_t *cachep, void *objp)
{
	unsigned long flags;

	intr_save(flags);
	__cache_free(cachep, objp);
	intr_restore(flags);
}

void kmem_cache_init(void)
{
	// How to initialize it need a page frame to
	// build our first cache
	// lets check the kernel
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
#include "types.h"
#include "param.h"
#include "riscv.h"
#include "atomic.h"
#include "memlayout.h"
#include "spinlock.h"
#include "list.h"
#include "defs.h"
#include "slab.h"
#include "page.h"
#include "semaphore.h"
#include "preempt.h"
#include "proc.h"

// all 'linux kernel' in comment means 'linux kernel 2.6.11'

// NOTE THAT kmem_cache_create / destory can't be used in int
// as linux kernel has explained, kmem_cache_destory's should prevent
// concurrent alloc

// flags is reserved for further use
#define BOOT_CPUCACHE_ENTRIES	1
struct arraycache_init {
	struct array_cache cache;
	void * entries[BOOT_CPUCACHE_ENTRIES];
};

#define LIST3_INIT(parent) \
	{ \
		.slabs_full	= LIST_HEAD_INIT(parent.slabs_full), \
		.slabs_partial	= LIST_HEAD_INIT(parent.slabs_partial), \
		.slabs_free	= LIST_HEAD_INIT(parent.slabs_free) \
	}
#define list3_data(cachep) \
	(&(cachep)->lists)


/* internal cache of cache description objs */
static kmem_cache_t cache_cache = {
	.lists		= LIST3_INIT(cache_cache.lists),
	.batchcount	= 1,
	.limit		= BOOT_CPUCACHE_ENTRIES,
	.objsize	= sizeof(kmem_cache_t),
	.flags		= 0,
	.spinlock	= SPIN_LOCK_UNLOCKED("cache_cache"),
	.name		= "kmem_cache",
};

static struct arraycache_init initarray_cache =
	{ { 0, BOOT_CPUCACHE_ENTRIES, 1, 0} };
static struct arraycache_init initarray_generic =
	{ { 0, BOOT_CPUCACHE_ENTRIES, 1, 0} };


struct cache_sizes malloc_sizes[] = {
#define CACHE(x) { .cs_size = (x) },
	CACHE(32)
	CACHE(64)
	CACHE(96)
	CACHE(128)
	CACHE(192)
	CACHE(256)
	CACHE(512)
	CACHE(1024)
	CACHE(2048)
	CACHE(4096)
	{ 0, }
#undef CACHE
};

/* Must match cache_sizes above. Out of line to keep cache footprint low. */
struct cache_names {
	char *name;
};

static struct cache_names cache_names[] = {
#define CACHE(x) { .name = "size-" #x },
	CACHE(32)
	CACHE(64)
	CACHE(96)
	CACHE(128)
	CACHE(192)
	CACHE(256)
	CACHE(512)
	CACHE(1024)
	CACHE(2048)
	CACHE(4096)
	{ NULL, }
#undef CACHE
};

static enum {
	NONE,
	PARTIAL,
	FULL
} g_cpucache_up;

struct ccupdate_struct {
	struct kmem_cache_s *cachep;
	struct array_cache *new[NR_CPUS];
};


/* Guard access to the cache-chain. */
static struct semaphore	cache_chain_sem;
static struct list_head cache_chain;
static int slab_break_gfp_order = 1;
static int offslab_limit = 0;

static int cache_grow (kmem_cache_t * cachep, int flags);
static struct slab* alloc_slabmgmt (kmem_cache_t *cachep,
			void *objp, int colour_off, unsigned long flags);
static int do_tune_cpucache (kmem_cache_t* cachep, int limit, int batchcount, int shared);
static int do_tune_cpucache_one (kmem_cache_t* cachep, int limit, int batchcount, int shared);
static void enable_cpucache (kmem_cache_t *cachep);
static void enable_cpucache_one (kmem_cache_t *cachep);
static void *kmem_getpages(kmem_cache_t *cachep);
static void kmem_freepages(kmem_cache_t *cachep, void *addr);
static kmem_cache_t * kmem_find_general_cachep (size_t size);
static void free_block(kmem_cache_t *cachep, void **objpp, int nr_objects);
static void cache_estimate (unsigned long gfporder, size_t size, size_t align,
		 int flags, size_t *left_over, unsigned int *num);
static void* cache_alloc_refill(kmem_cache_t* cachep, int flags);
static void set_slab_attr(kmem_cache_t *cachep, struct slab *slabp, void *objp);
static void cache_init_objs (kmem_cache_t * cachep,
			struct slab * slabp, unsigned long ctor_flags);
static struct array_cache *alloc_arraycache(int entries, int batchcount);
static void cache_flusharray (kmem_cache_t* cachep, struct array_cache *ac);
static void free_block(kmem_cache_t* cachep, void** objpp, int len);
static void slab_destroy (kmem_cache_t *cachep, struct slab *slabp);
static void kmem_freepages(kmem_cache_t *cachep, void *addr);
static inline void * __cache_alloc (kmem_cache_t *cachep, int flags);
static inline void __cache_free (kmem_cache_t *cachep, void* objp);
static int __cache_shrink(kmem_cache_t *cachep);

static inline void ** ac_entry(struct array_cache *ac)
{
	return (void**)(ac+1);
}

static inline struct array_cache *ac_data(kmem_cache_t *cachep)
{
	return cachep->array[cpuid()];
}

static inline kmem_bufctl_t *slab_bufctl(struct slab *slabp)
{
	return (kmem_bufctl_t *)(slabp+1);
}

#define	SET_PAGE_CACHE(p, c) do { p->lru.next = (struct list_head *)c; } while(0)
#define	SET_PAGE_SLAB(p, s) do { p->lru.prev = (struct list_head *)s; } while(0)
#define GET_PAGE_CACHE(p) (kmem_cache_t *)((p)->lru.next)
#define GET_PAGE_SLAB(p) (struct slab *)((p)->lru.prev)


// general cache IS NOT implemented
// general cache interface: kmalloc, kmfree(should be `kfree`)
void* kmalloc (int size, int flags)
{
	struct cache_sizes *csizep = malloc_sizes;

	for (; csizep->cs_size; csizep++) {
		if (size > csizep->cs_size)
			continue;
		return __cache_alloc(csizep->cs_cachep, flags);
	}
	return NULL;
}

void kmfree (void *objp)
{
	// panic("kmfree not implemented");
	// printf("kmfree BUG I don't do anything\n");
	kmem_cache_t *c;
	unsigned long flags;

	if (!objp)
		return;
	intr_save(flags);
	// kfree_debugcheck(objp);
	c = GET_PAGE_CACHE(pa_to_page(objp));
	__cache_free(c, (void*)objp);
	intr_restore(flags);
}


// alloc a object
void* kmem_cache_alloc (kmem_cache_t *cachep, int flags)
{
	// The reason why we don't disable intr here is one line code is simpler
	return __cache_alloc(cachep, flags);
}

// free a object
void kmem_cache_free (kmem_cache_t *cachep, void *objp)
{
	unsigned long saved_flag;

	intr_save(saved_flag);
	__cache_free(cachep, objp);
	intr_restore(saved_flag);
}

int kmem_cache_shrink(kmem_cache_t *cachep)
{
	return __cache_shrink(cachep);
}


void kmem_cache_init(void)
{
	size_t left_over;
	struct cache_sizes *sizes;
	struct cache_names *names;

	slab_break_gfp_order = 1;

	// linux kernel first build a cache_cache for cache descriptor
	initsemaphore(&cache_chain_sem, 1, "cache_chain_sem");
	INIT_LIST_HEAD(&cache_chain);
	list_add(&cache_cache.next, &cache_chain);
	cache_cache.colour_off = cache_line_size();
	cache_cache.array[cpuid()] = &initarray_cache.cache;

	cache_cache.objsize = ALIGN(cache_cache.objsize, cache_line_size());

	cache_estimate(0, cache_cache.objsize, cache_line_size(), 0,
				&left_over, &cache_cache.num);

	if (!cache_cache.num)
		panic("kmem_init cache_cache init failed."); // BUG();

	cache_cache.colour = left_over/cache_cache.colour_off;
	cache_cache.colour_next = 0;
	cache_cache.slab_size = ALIGN(cache_cache.num*sizeof(kmem_bufctl_t) +
				sizeof(struct slab), cache_line_size());

	// then it build general caches (which could be used to store slab cache ...)
	sizes = malloc_sizes;
	names = cache_names;

	while (sizes->cs_size) {
		/* For performance, all the general caches are L1 aligned.
		 * This should be particularly beneficial on SMP boxes, as it
		 * eliminates "false sharing".
		 * Note for systems short on memory removing the alignment will
		 * allow tighter packing of the smaller caches. */
		sizes->cs_cachep = kmem_cache_create(names->name,
			sizes->cs_size, ARCH_KMALLOC_MINALIGN,
			(ARCH_KMALLOC_FLAGS | SLAB_PANIC), NULL, NULL);

		/* Inc off-slab bufctl limit until the ceiling is hit. */
		if (!(OFF_SLAB(sizes->cs_cachep))) {
			offslab_limit = sizes->cs_size-sizeof(struct slab);
			offslab_limit /= sizeof(kmem_bufctl_t);
		}

		sizes++;
		names++;
	}

	// then it reinitialize head array
	{
		void * ptr;
		
		ptr = kmalloc(sizeof(struct arraycache_init), GFP_KERNEL);
		// BUG_ON(ac_data(&cache_cache) != &initarray_cache.cache);
		memmove(ptr, ac_data(&cache_cache), sizeof(struct arraycache_init));
		cache_cache.array[cpuid()] = ptr;
	
		ptr = kmalloc(sizeof(struct arraycache_init), GFP_KERNEL);
		// BUG_ON(ac_data(malloc_sizes[0].cs_cachep) != &initarray_generic.cache);
		memmove(ptr, ac_data(malloc_sizes[0].cs_cachep),
				sizeof(struct arraycache_init));
		malloc_sizes[0].cs_cachep->array[cpuid()] = ptr;
	}

	/* 5) resize the head arrays to their final sizes */
	{
		kmem_cache_t *cachep;
		down(&cache_chain_sem);
		// printf("kmem_init waiting for enable_cpucache\n");
		list_for_each_entry(cachep, &cache_chain, next)
			enable_cpucache_one(cachep);
		up(&cache_chain_sem);
	}

	/* Done! */
	g_cpucache_up = FULL;
}

void
kmem_cache_init_late()
{
	{
		kmem_cache_t *cachep;
		down(&cache_chain_sem);
		// printf("kmem_init waiting for enable_cpucache\n");
		list_for_each_entry(cachep, &cache_chain, next)
			enable_cpucache_one(cachep);
		up(&cache_chain_sem);
	}
}

kmem_cache_t *
kmem_cache_create (char *name, size_t size, size_t align,
	unsigned long flags, void (*ctor)(void*, kmem_cache_t *, unsigned long),
	void (*dtor)(void*, kmem_cache_t *, unsigned long))
{
	size_t left_over, slab_size, ralign;
	kmem_cache_t *cachep = NULL;

	// part of flags checking may delay to cache grow
	if ((!name) ||
		(size < BYTES_PER_WORD) ||
		(size > (1<<MAX_OBJ_ORDER)*PGSIZE)) { // (dtor && !ctor))
			panic("kmem_cache_create");
		}

	if (flags & SLAB_HWCACHE_ALIGN) {
	/* 1) arch recommendation */
		ralign = cache_line_size();
		while (size <= ralign/2)
			ralign /= 2;
	} else {
		ralign = BYTES_PER_WORD;
	}
	/* 2) arch mandated alignment */
	if (ralign < ARCH_SLAB_MINALIGN) {
		ralign = ARCH_SLAB_MINALIGN;
	}
	/* 3) caller mandated alignment */
	if (ralign < align) {
		ralign = align;
	}
	/* 4) Store it. */
	align = ralign;

	cachep = (kmem_cache_t *) kmem_cache_alloc(&cache_cache, SLAB_KERNEL);
	if (!cachep)
		goto opps;
	memset(cachep, 0, sizeof(kmem_cache_t));

	/* Determine if the slab management is 'on' or 'off' slab. */
	if (size >= (PGSIZE>>3))
		/*
		 * Size is large, assume best to place the slab management obj
		 * off-slab (should allow better packing of objs).
		 */
		flags |= CFLGS_OFF_SLAB;

	size = ALIGN(size, align);

	do {
		unsigned int break_flag = 0;
cal_wastage:
		cache_estimate(cachep->gfporder, size, align, flags,
					&left_over, &cachep->num);
		if (break_flag)
			break;
		if (cachep->gfporder >= MAX_GFP_ORDER)
			break;
		if (!cachep->num)
			goto next;
		if (flags & CFLGS_OFF_SLAB &&
				cachep->num > offslab_limit) {
			/* This num of objs will cause problems. */
			cachep->gfporder--;
			break_flag++;
			goto cal_wastage;
		}

		/*
		 * Large num of objs is good, but v. large slabs are
		 * currently bad for the gfp()s.
		 */
		if (cachep->gfporder >= slab_break_gfp_order)
			break;

		if ((left_over*8) <= (PGSIZE<<cachep->gfporder))
			break;	/* Acceptable internal fragmentation. */
next:
		cachep->gfporder++;
	} while (1);

	if (!cachep->num) {
		kmem_cache_free(&cache_cache, cachep);
		cachep = NULL;
		goto opps;
	}
	slab_size = ALIGN(cachep->num*sizeof(kmem_bufctl_t)
				+ sizeof(struct slab), align);

	/*
	 * If the slab has been placed off-slab, and we have enough space then
	 * move it on-slab. This is at the expense of any extra colouring.
	 */
	if (flags & CFLGS_OFF_SLAB && left_over >= slab_size) {
		flags &= ~CFLGS_OFF_SLAB;
		left_over -= slab_size;
	}

	if (flags & CFLGS_OFF_SLAB) {
		/* really off slab. No need for manual alignment */
		slab_size = cachep->num*sizeof(kmem_bufctl_t)+sizeof(struct slab);
	}

	cachep->colour_off = cache_line_size();
	/* Offset must be a multiple of the alignment. */
	if (cachep->colour_off < align)
		cachep->colour_off = align;
	cachep->colour = left_over/cachep->colour_off;
	cachep->slab_size = slab_size;
	cachep->flags = flags;
	cachep->gfpflags = 0;
	initlock(&cachep->spinlock, name);
	cachep->objsize = size;
	/* NUMA */
	INIT_LIST_HEAD(&cachep->lists.slabs_full);
	INIT_LIST_HEAD(&cachep->lists.slabs_partial);
	INIT_LIST_HEAD(&cachep->lists.slabs_free);

	if (flags & CFLGS_OFF_SLAB)
		cachep->slabp_cache = kmem_find_general_cachep(slab_size);
	cachep->ctor = ctor;
	cachep->dtor = dtor;
	cachep->name = name;


	if (g_cpucache_up == FULL) {
		enable_cpucache(cachep);
	} else {
		if (g_cpucache_up == NONE) {
			/* Note: the first kmem_cache_create must create
			 * the cache that's used by kmalloc(24), otherwise
			 * the creation of further caches will BUG().
			 */
			cachep->array[cpuid()] = &initarray_generic.cache;
			g_cpucache_up = PARTIAL;
		} else {
			cachep->array[cpuid()] = kmalloc(sizeof(struct arraycache_init),GFP_KERNEL);
		}
		// BUG_ON(!ac_data(cachep));
		ac_data(cachep)->avail = 0;
		ac_data(cachep)->limit = BOOT_CPUCACHE_ENTRIES;
		ac_data(cachep)->batchcount = 1;
		ac_data(cachep)->touched = 0;
		cachep->batchcount = 1;
		cachep->limit = BOOT_CPUCACHE_ENTRIES;
		cachep->free_limit = (1+NR_CPUS)*cachep->batchcount
					+ cachep->num;
	}

	down(&cache_chain_sem);
	/* cache setup completed, link it into the list */
	list_add(&cachep->next, &cache_chain);
	up(&cache_chain_sem);

opps:
	if (!cachep && (flags & SLAB_PANIC))
		panic("kmem_cache_create(): failed to create slab");
	return cachep;
}

int kmem_cache_destroy (kmem_cache_t * cachep)
{
	panic("kmem_cache_destory not implemented");
	return 0;
}

static void smp_call_function_all_cpus(void (*func) (void *arg), void *arg)
{
	check_intr_on();
	preempt_disable();

	intr_off();
	func(arg);
	intr_on();

	if (smp_call_function(func, arg, 1))
		panic("smp_call_function_all_cpus call smp_call_function");

	preempt_enable();
}

static void do_ccupdate_local(void *info)
{
	struct ccupdate_struct *new = (struct ccupdate_struct *)info;
	struct array_cache *old;

	check_intr_off();
	old = ac_data(new->cachep);
	
	new->cachep->array[cpuid()] = new->new[cpuid()];
	new->new[cpuid()] = old;
}

// get some pages from buddy system, and change page desp state to slab
static void *kmem_getpages(kmem_cache_t *cachep)
{
	return alloc_pages(cachep->gfporder);
}

static void kmem_freepages(kmem_cache_t *cachep, void *addr)
{
	// cachep unused
	free_pages(addr);
}

static inline void * __cache_alloc (kmem_cache_t *cachep, int flags)
{
	unsigned long save_flags;
	void* objp;
	struct array_cache *ac;

	intr_save(save_flags);
	ac = ac_data(cachep);
	if (ac->avail) {
		ac->touched = 1;
		objp = ac_entry(ac)[--ac->avail];
	} else {
		objp = cache_alloc_refill(cachep, flags);
	}
	intr_off(save_flags);

	return objp;
}

static inline void __cache_free (kmem_cache_t *cachep, void* objp)
{
	struct array_cache *ac = ac_data(cachep);

	check_intr_off();

	if (ac->avail < ac->limit) {
		ac_entry(ac)[ac->avail++] = objp;
		return;
	} else {
		cache_flusharray(cachep, ac);
		ac_entry(ac)[ac->avail++] = objp;
	}
	return;
}

static int __cache_shrink(kmem_cache_t *cachep)
{
	return -1;
}

static void cache_estimate (unsigned long gfporder, size_t size, size_t align,
		 int flags, size_t *left_over, unsigned int *num)
{
	int i;
	size_t wastage = PGSIZE<<gfporder;
	size_t extra = 0;
	size_t base = 0;

	if (!(flags & CFLGS_OFF_SLAB)) {
		base = sizeof(struct slab);
		extra = sizeof(kmem_bufctl_t);
	}
	i = 0;
	while (i*size + ALIGN(base+i*extra, align) <= wastage)
		i++;
	if (i > 0)
		i--;

	if (i > SLAB_LIMIT)
		i = SLAB_LIMIT;

	*num = i;
	wastage -= i*size;
	wastage -= ALIGN(base+i*extra, align);
	*left_over = wastage;
}

static void* cache_alloc_refill(kmem_cache_t* cachep, int flags)
{
	int batchcount;
	struct kmem_list3 *l3;
	struct array_cache *ac;

	ac = ac_data(cachep);
retry:
	batchcount = ac->batchcount;
	if (!ac->touched && batchcount > BATCHREFILL_LIMIT) {
		/* if there was little recent activity on this
		 * cache, then perform only a partial refill.
		 * Otherwise we could generate refill bouncing.
		 */
		batchcount = BATCHREFILL_LIMIT;
	}
	l3 = list3_data(cachep);

	acquire(&cachep->spinlock);
	if (l3->shared) {
		struct array_cache *shared_array = l3->shared;
		if (shared_array->avail) {
			if (batchcount > shared_array->avail)
				batchcount = shared_array->avail;
			shared_array->avail -= batchcount;
			ac->avail = batchcount;
			memmove(ac_entry(ac), &ac_entry(shared_array)[shared_array->avail],
					sizeof(void*)*batchcount);
			shared_array->touched = 1;
			goto alloc_done;
		}
	}
	while (batchcount > 0) {
		struct list_head *entry;
		struct slab *slabp;
		/* Get slab alloc is to come from. */
		entry = l3->slabs_partial.next;
		/* list_empty() */
		if (entry == &l3->slabs_partial) {
			l3->free_touched = 1;
			entry = l3->slabs_free.next;
			if (entry == &l3->slabs_free)
				goto must_grow;
		}

		slabp = list_entry(entry, struct slab, list);
		// check_slabp(cachep, slabp);
		// check_spinlock_acquired(cachep);
		while (slabp->inuse < cachep->num && batchcount--) {
			kmem_bufctl_t next;

			/* get obj pointer */
			ac_entry(ac)[ac->avail++] = slabp->s_mem + slabp->free*cachep->objsize;

			slabp->inuse++;
			next = slab_bufctl(slabp)[slabp->free];
			/* BUFCTL_FREE here actually means it was in use */
			slab_bufctl(slabp)[slabp->free] = BUFCTL_FREE;
		    slabp->free = next;
		}
		// check_slabp(cachep, slabp);

		/* move slabp to correct slabp list: */
		list_del(&slabp->list);
		if (slabp->free == BUFCTL_END)
			list_add(&slabp->list, &l3->slabs_full);
		else
			list_add(&slabp->list, &l3->slabs_partial);
	}

must_grow:
	l3->free_objects -= ac->avail;
alloc_done:
	release(&cachep->spinlock);

	if (!ac->avail) {
		int x;
		x = cache_grow(cachep, flags);
		
		// cache_grow can reenable interrupts, then ac could change.
		ac = ac_data(cachep);
		if (!x && ac->avail == 0)	// no objects in sight? abort
			return NULL;

		if (!ac->avail)		// objects refilled by interrupt?
			goto retry;
	}
	ac->touched = 1;
	return ac_entry(ac)[--ac->avail];
}

static void cache_flusharray (kmem_cache_t* cachep, struct array_cache *ac)
{
	int batchcount;

	batchcount = ac->batchcount;
	check_intr_off();
	acquire_irq_on(&cachep->spinlock);
	if (cachep->lists.shared) {
		struct array_cache *shared_array = cachep->lists.shared;
		int max = shared_array->limit-shared_array->avail;
		if (max) {
			if (batchcount > max)
				batchcount = max;
			memmove(&ac_entry(shared_array)[shared_array->avail],
					&ac_entry(ac)[0],
					sizeof(void*)*batchcount);
			shared_array->avail += batchcount;
			goto free_done;
		}
	}

	free_block(cachep, &ac_entry(ac)[0], batchcount);
free_done:
	release_irq_on(&cachep->spinlock);
	ac->avail -= batchcount;
	memmove(&ac_entry(ac)[0], &ac_entry(ac)[batchcount],
			sizeof(void*)*ac->avail);
}

static int cache_grow (kmem_cache_t * cachep, int flags)
{
	struct slab	*slabp;
	void		*objp;
	size_t		 offset;
	int		 local_flags;
	unsigned long	 ctor_flags;

	/* Be lazy and only check for valid flags here,
 	 * keeping it out of the critical path in kmem_cache_alloc().
	 */
	if (flags & ~(SLAB_LEVEL_MASK | SLAB_NO_GROW))
		panic("cache_grow flag check falied\n");
	if (flags & SLAB_NO_GROW)
		return 0;


	ctor_flags = SLAB_CTOR_CONSTRUCTOR;
	local_flags = flags | SLAB_LEVEL_MASK;
	/* About to mess with non-constant members - lock. */
	acquire_irq_on(&cachep->spinlock);

	/* Get colour for the slab, and cal the next value. */
	offset = cachep->colour_next;
	cachep->colour_next++;
	if (cachep->colour_next >= cachep->colour)
		cachep->colour_next = 0;
	offset *= cachep->colour_off;

	release_irq_on(&cachep->spinlock);

	/* Get mem for the objs. */
	if (!(objp = kmem_getpages(cachep)))
		goto failed;

	/* Get slab management. */
	if (!(slabp = alloc_slabmgmt(cachep, objp, offset, local_flags)))
		goto opps1;

	set_slab_attr(cachep, slabp, objp);

	cache_init_objs(cachep, slabp, ctor_flags);

	acquire_irq_on(&cachep->spinlock);

	/* Make slab active. */
	list_add_tail(&slabp->list, &(list3_data(cachep)->slabs_free));
	list3_data(cachep)->free_objects += cachep->num;
	release_irq_on(&cachep->spinlock);
	return 1;
opps1:
	kmem_freepages(cachep, objp);
failed:
	return 0;
}

static struct slab* alloc_slabmgmt (kmem_cache_t *cachep,
			void *objp, int colour_off, unsigned long flags)
{
	struct slab *slabp;
	
	if (OFF_SLAB(cachep)) {
		/* Slab management obj is off-slab. */
		slabp = kmem_cache_alloc(cachep->slabp_cache, 0);
		if (!slabp)
			return NULL;
	} else {
		slabp = objp+colour_off;
		colour_off += cachep->slab_size;
	}
	slabp->inuse = 0;
	slabp->colouroff = colour_off;
	slabp->s_mem = objp+colour_off;

	return slabp;
}

static void cache_init_objs (kmem_cache_t * cachep,
			struct slab * slabp, unsigned long ctor_flags)
{
	int i;

	for (i = 0; i < cachep->num; i++) {
		void* objp = slabp->s_mem+cachep->objsize*i;
		slab_bufctl(slabp)[i] = i+1;
		if (cachep->ctor)
			cachep->ctor(objp, cachep, ctor_flags);
	}

	slab_bufctl(slabp)[i-1] = BUFCTL_END;
	slabp->free = 0;
}

static void set_slab_attr(kmem_cache_t *cachep, struct slab *slabp, void *objp)
{
	int i;
	struct page *page;

	/* Nasty!!!!!! I hope this is OK. */
	/* The comment above is a real comment in linux kernel. */
	i = 1 << cachep->gfporder;
	/* page = virt_to_page(objp);
	 * In kernel virt = pa
	 */
	page = pa_to_page(objp);
	do {
		SET_PAGE_CACHE(page, cachep);
		SET_PAGE_SLAB(page, slabp);
		page++;
	} while (--i);
}

static void enable_cpucache (kmem_cache_t *cachep)
{
	int err;
	int limit, shared;

	if (cachep->objsize > 131072)
		limit = 1;
	else if (cachep->objsize > PGSIZE)
		limit = 8;
	else if (cachep->objsize > 1024)
		limit = 24;
	else if (cachep->objsize > 256)
		limit = 54;
	else
		limit = 120;

	shared = 0;

	if (cachep->objsize <= PGSIZE)
		shared = 8;

	err = do_tune_cpucache(cachep, limit, (limit+1)/2, shared);
	if (err)
		printf("enable_cpucache failed for %s, error %d.\n",
					cachep->name, -err);
}

static void enable_cpucache_one (kmem_cache_t *cachep)
{
	int err;
	int limit, shared;

	if (cachep->objsize > 131072)
		limit = 1;
	else if (cachep->objsize > PGSIZE)
		limit = 8;
	else if (cachep->objsize > 1024)
		limit = 24;
	else if (cachep->objsize > 256)
		limit = 54;
	else
		limit = 120;

	shared = 0;

	if (cachep->objsize <= PGSIZE)
		shared = 8;

	err = do_tune_cpucache_one(cachep, limit, (limit+1)/2, shared);
	if (err)
		printf("enable_cpucache failed for %s, error %d.\n",
					cachep->name, -err);
}

static int do_tune_cpucache (kmem_cache_t* cachep, int limit, int batchcount, int shared)
{
	struct ccupdate_struct new;
	struct array_cache *new_shared;
	int i;

	memset(&new.new,0,sizeof(new.new));
	for (i = 0; i < NR_CPUS; i++) {
		new.new[i] = alloc_arraycache(limit, batchcount);
		if (!new.new[i]) {
			for (i--; i >= 0; i--) kmfree(new.new[i]);
			return -1;
		}
	}
	new.cachep = cachep;

	smp_call_function_all_cpus(do_ccupdate_local, (void *)&new);
	
	check_intr_on();
	acquire(&cachep->spinlock);
	cachep->batchcount = batchcount;
	cachep->limit = limit;
	cachep->free_limit = (1+NR_CPUS)*cachep->batchcount + cachep->num;
	release(&cachep->spinlock);

	for (i = 0; i < NR_CPUS; i++) {
		struct array_cache *ccold = new.new[i];
		if (!ccold)
			continue;
		acquire(&cachep->spinlock);
		free_block(cachep, ac_entry(ccold), ccold->avail);
		release(&cachep->spinlock);
		kmfree(ccold);
	}
	new_shared = alloc_arraycache(batchcount*shared, 0xbaadf00d);
	if (new_shared) {
		struct array_cache *old;

		acquire(&cachep->spinlock);
		old = cachep->lists.shared;
		cachep->lists.shared = new_shared;
		if (old)
			free_block(cachep, ac_entry(old), old->avail);
		release(&cachep->spinlock);
		kmfree(old);
	}

	return 0;
}

static int do_tune_cpucache_one (kmem_cache_t* cachep, int limit, int batchcount, int shared)
{
	struct array_cache *new;
	struct array_cache *new_shared;
	struct array_cache *ccold;

	new = alloc_arraycache(limit, batchcount);
	if (!new) {
		kmfree(new);
		return -1;
	}

	acquire(&cachep->spinlock);
	cachep->batchcount = batchcount;
	cachep->limit = limit;
	cachep->free_limit = (1+(++cachep->online_cpus))*cachep->batchcount + cachep->num;
	ccold = cachep->array[cpuid()];
	cachep->array[cpuid()] = new;
	release(&cachep->spinlock);


	if (ccold) {
		acquire(&cachep->spinlock);
		free_block(cachep, ac_entry(ccold), ccold->avail);
		release(&cachep->spinlock);
		kmfree(ccold);
	}

	if (cachep->lists.shared != NULL) {
		return 1;
	}
	new_shared = alloc_arraycache(batchcount*shared, 0xbaadf00d);
	if (new_shared) {
		struct array_cache *old;

		acquire(&cachep->spinlock);
		old = cachep->lists.shared;
		cachep->lists.shared = new_shared;
		if (old)
			free_block(cachep, ac_entry(old), old->avail);
		release(&cachep->spinlock);
		kmfree(old);
	}

	return 0;
}

static kmem_cache_t * kmem_find_general_cachep (size_t size)
{
	struct cache_sizes *csizep = malloc_sizes;

	/* This function could be moved to the header file, and
	 * made inline so consumers can quickly determine what
	 * cache pointer they require.
	 */
	for ( ; csizep->cs_size; csizep++) {
		if (size > csizep->cs_size)
			continue;
		break;
	}
	return csizep->cs_cachep;
}

static struct array_cache *alloc_arraycache(int entries, int batchcount)
{
	int memsize = sizeof(void*)*entries+sizeof(struct array_cache);
	struct array_cache *nc = NULL;

	nc = kmalloc(memsize, GFP_KERNEL);
	if (nc) {
		nc->avail = 0;
		nc->limit = entries;
		nc->batchcount = batchcount;
		nc->touched = 0;
	}
	return nc;
}

static void free_block(kmem_cache_t *cachep, void **objpp, int nr_objects)
{
	int i;

	/* NUMA: move add into loop */
	cachep->lists.free_objects += nr_objects;

	for (i = 0; i < nr_objects; i++) {
		void *objp = objpp[i];
		struct slab *slabp;
		unsigned int objnr;

		slabp = GET_PAGE_SLAB(pa_to_page(objp));
		list_del(&slabp->list);
		objnr = (objp - slabp->s_mem) / cachep->objsize;
		// check_slabp(cachep, slabp);
		slab_bufctl(slabp)[objnr] = slabp->free;
		slabp->free = objnr;
		slabp->inuse--;

		/* fixup slab chains */
		if (slabp->inuse == 0) {
			if (cachep->lists.free_objects > cachep->free_limit) {
				cachep->lists.free_objects -= cachep->num;
				slab_destroy(cachep, slabp);
			} else {
				list_add(&slabp->list,
				&cachep->lists.slabs_free);
			}
		} else {
			/* Unconditionally move a slab to the end of the
			 * partial list on free - maximum time for the
			 * other objects to be freed, too.
			 */
			list_add_tail(&slabp->list,
				&cachep->lists.slabs_partial);
		}
	}	
}

static void slab_destroy (kmem_cache_t *cachep, struct slab *slabp)
{
	void *addr = slabp->s_mem - slabp->colouroff;

	kmem_freepages(cachep, addr);
	if (OFF_SLAB(cachep))
		kmem_cache_free(cachep->slabp_cache, slabp);
}

// static void cpuup_callback()
// {
// 	kmem_cache_t* cachep;
// 	check_intr_off();

// 	int cpu = cpuid();
// 	down(&cache_chain_sem);
// 	list_for_each_entry(cachep, &cache_chain, next) {
// 		struct array_cache *nc;

// 		nc = alloc_arraycache(cachep->limit, cachep->batchcount);
// 		if (!nc)
// 			goto bad;

// 		acquire(&cachep->spinlock);
// 		cachep->array[cpu] = nc;
// 		cachep->free_limit = (1+NR_CPUS)*cachep->batchcount
// 					+ cachep->num;
// 		release(&cachep->spinlock);

// 	}
// 	up(&cache_chain_sem);
// 	return;

// bad:
// 	up(&cache_chain_sem);
// 	panic("cpuup_callback failed");
// }
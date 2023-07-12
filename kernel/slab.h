#ifndef _KERNEL_SLAB_
#define _KERNEL_SLAB_

typedef struct kmem_cache_s kmem_cache_t;

struct kmem_cache_s {
/* 1) per-cpu data, touched during every alloc/free */
	struct array_cache	*array[NR_CPUS];
	unsigned int		batchcount;
	unsigned int		limit;
/* 2) touched by every alloc & free from the backend */
	struct kmem_list3	lists;
	/* NUMA: kmem_3list_t	*nodelists[MAX_NUMNODES] */
	unsigned int		objsize;
	unsigned int	 	flags;	/* constant flags */
	unsigned int		num;	/* # of objs per slab */
	unsigned int		free_limit; /* upper limit of objects in the lists */
	spinlock_t		    spinlock;

/* 3) cache_grow/shrink */
	/* order of pgs per slab (2^n) */
	unsigned int		gfporder;

	/* force GFP flags, e.g. GFP_DMA */
	unsigned int		gfpflags;

	size_t			colour;		/* cache colouring range */
	unsigned int		colour_off;	/* colour offset */
	unsigned int		colour_next;	/* cache colouring */
	kmem_cache_t		*slabp_cache;
	unsigned int		slab_size;
	unsigned int		dflags;		/* dynamic flags */

	/* constructor func */
	void (*ctor)(void *, kmem_cache_t *, unsigned long);

	/* de-constructor func */
	void (*dtor)(void *, kmem_cache_t *, unsigned long);

/* 4) cache creation/removal */
	const char		*name;
	struct list_head	next;

/* 5) statistics */
#if STATS
	unsigned long		num_active;
	unsigned long		num_allocations;
	unsigned long		high_mark;
	unsigned long		grown;
	unsigned long		reaped;
	unsigned long 		errors;
	unsigned long		max_freeable;
	unsigned long		node_allocs;
	atomic_t		allochit;
	atomic_t		allocmiss;
	atomic_t		freehit;
	atomic_t		freemiss;
#endif
#if DEBUG
	int			dbghead;
	int			reallen;
#endif
};

struct kmem_list3 {
	struct list_head	slabs_partial;	/* partial list first, better asm code */
	struct list_head	slabs_full;
	struct list_head	slabs_free;
	unsigned long	free_objects;
	int		free_touched;
	unsigned long	next_reap;
	struct array_cache	*shared;
};

struct slab {
	struct list_head	list;
	unsigned long		colouroff;
	void			*s_mem;		/* including colour offset */
	unsigned int		inuse;		/* num of objs active in slab */
	kmem_bufctl_t		free;
};

struct array_cache {
	unsigned int avail;
	unsigned int limit;
	unsigned int batchcount;
	unsigned int touched;
};

#define CFLGS_OFF_SLAB		(0x80000000UL)

#define BUFCTL_END	(((kmem_bufctl_t)(~0U))-0)
#define BUFCTL_FREE	(((kmem_bufctl_t)(~0U))-1)
#define	SLAB_LIMIT	(((kmem_bufctl_t)(~0U))-2)

struct cache_sizes {
	size_t		 cs_size;
	kmem_cache_t	*cs_cachep;
};

extern struct cache_sizes malloc_sizes[];

#define BYTES_PER_WORD (4)
#define MIN_OBJ_SIZE BYTES_PER_WORD
#define MAX_OBJ_ORDER (8) // 1MB


/* kmem_cache_create align */
#define ARCH_KMALLOC_MINALIGN 0
#define ARCH_SLAB_MINALIGN 0

/* kmem_cache_create flag 
 * TODO: remove SLAB_RECLAIM_ACCOUNT
 */
#define ARCH_KMALLOC_FLAGS SLAB_HWCACHE_ALIGN
#define SLAB_PANIC 0x1
#define SLAB_HWCACHE_ALIGN 0x2
#define CFLGS_OFF_SLAB 0x4
#define SLAB_RECLAIM_ACCOUNT 0x8

/* limit */
#define MAX_GFP_ORDER 8
#define BATCHREFILL_LIMIT 16

/* ctor flag */
#define SLAB_CTOR_CONSTRUCTOR 0x1  // in cache_grow init ctor_flags UNKOWNN

/* gfp_flags most of won't be use
 * TODO: remove GFP_KERNEL */
#define SLAB_LEVEL_MASK SLAB_NO_GROW /* in cache_grow flag check */
#define GFP_KERNEL 0                 /* kmem_cache_create - flag */
#define	SLAB_NO_GROW (1)	/* don't grow a cache, actually it won't be passed to alloc_pages */

#endif
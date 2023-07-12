#ifndef __KERNEL_PAGE_
#define __KERNEL_PAGE_

#ifdef BUDDY_SYSTEM

#define MAX_ORDER (11)
#define INVALID_ORDER MAX_ORDER
#define NR_PGDESP ((PHYSTOP - KERNBASE) >> PGSHIFT)

#define pa_to_page(x) (struct page *)(page_desp + (((uint64)x - KERNBASE) >> PGSHIFT))
#define page_to_pa(x) (void *)(KERNBASE + ((x - page_desp) << PGSHIFT))


// Page descriptor for all pages in mem
extern struct page {
  struct list_head lru;
  uint32 order;
} page_desp[NR_PGDESP];

struct free_area {
  struct list_head free_list;
  uint64 nr_free;
};

struct {
  struct spinlock lock;
  struct free_area free_area[MAX_ORDER];
  uint64 free_pages;
} kmem;

static inline uint8 page_is_buddy(struct page *buddy, uint32 order)
{
  return buddy->order == order;
}

static inline void rmv_page_order(struct page *page)
{
  page->order = INVALID_ORDER;
}

static inline void set_page_order(struct page *page, uint32 order)
{
  page->order = order;
}

#else

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

#endif


/* a copy from linux "linux/gfp.h" 
 * in xv6, alloc_pages doesn't use any of these
 */
/*
 * GFP bitmasks..
 */
/* Zone modifiers in GFP_ZONEMASK (see linux/mmzone.h - low two bits) */
#define __GFP_DMA	0x01
#define __GFP_HIGHMEM	0x02

/*
 * Action modifiers - doesn't change the zoning
 *
 * __GFP_REPEAT: Try hard to allocate the memory, but the allocation attempt
 * _might_ fail.  This depends upon the particular VM implementation.
 *
 * __GFP_NOFAIL: The VM implementation _must_ retry infinitely: the caller
 * cannot handle allocation failures.
 *
 * __GFP_NORETRY: The VM implementation must not retry indefinitely.
 */
#define __GFP_WAIT	0x10	/* Can wait and reschedule? */
#define __GFP_HIGH	0x20	/* Should access emergency pools? */
#define __GFP_IO	0x40	/* Can start physical IO? */
#define __GFP_FS	0x80	/* Can call down to low-level FS? */
#define __GFP_COLD	0x100	/* Cache-cold page required */
#define __GFP_NOWARN	0x200	/* Suppress page allocation failure warning */
#define __GFP_REPEAT	0x400	/* Retry the allocation.  Might fail */
#define __GFP_NOFAIL	0x800	/* Retry for ever.  Cannot fail */
#define __GFP_NORETRY	0x1000	/* Do not retry.  Might fail */
#define __GFP_NO_GROW	0x2000	/* Slab internal usage */
#define __GFP_COMP	0x4000	/* Add compound page metadata */
#define __GFP_ZERO	0x8000	/* Return zeroed page on success */

#define __GFP_BITS_SHIFT 16	/* Room for 16 __GFP_FOO bits */
#define __GFP_BITS_MASK ((1 << __GFP_BITS_SHIFT) - 1)

/* if you forget to add the bitmask here kernel will crash, period */
#define GFP_LEVEL_MASK (__GFP_WAIT|__GFP_HIGH|__GFP_IO|__GFP_FS| \
			__GFP_COLD|__GFP_NOWARN|__GFP_REPEAT| \
			__GFP_NOFAIL|__GFP_NORETRY|__GFP_NO_GROW|__GFP_COMP)

#define GFP_ATOMIC	(__GFP_HIGH)
#define GFP_NOIO	(__GFP_WAIT)
#define GFP_NOFS	(__GFP_WAIT | __GFP_IO)
#define GFP_KERNEL	(__GFP_WAIT | __GFP_IO | __GFP_FS)
#define GFP_USER	(__GFP_WAIT | __GFP_IO | __GFP_FS)
#define GFP_HIGHUSER	(__GFP_WAIT | __GFP_IO | __GFP_FS | __GFP_HIGHMEM)

/* Flag - indicates that the buffer will be suitable for DMA.  Ignored on some
   platforms, used as appropriate on others */

#define GFP_DMA		__GFP_DMA


#endif
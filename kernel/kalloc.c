// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "list.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

// TODO: Comment this out
#define BUDDY_SYSTEM

#ifdef BUDDY_SYSTEM

#define MAX_ORDER (11)
#define INVALID_ORDER MAX_ORDER
#define NR_PGDESP ((PHYSTOP - KERNBASE) >> PGSHIFT)

#define pa_to_page(x) (struct page *)(page_desp + ((x - KERNBASE) >> PGSHIFT))
#define page_to_pa(x) (void *)(KERNBASE + (x - page_desp) << PGSHIFT)

// Page descriptor for all pages in mem
struct page {
  struct list_head lru;
  uint32 order;
} page_desp[NR_PGDESP];

struct free_area {
  struct list *free_list;
  uint64 nr_free;
};

struct {
  struct spinlock lock;
  struct free_area free_area[MAX_ORDER];
  uint64 free_pages;
} kmem;

#else

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

#endif



void
kinit()
{
  initlock(&kmem.lock, "kmem");
  #ifdef BUDDY_SYSTEM
  // TODO: initialize page_desp
  uint64 i = NR_PGDESP;
  while (i-- > 0) {
    page_desp[i].order = INVALID_ORDER;
  }

  kmem.free_pages = 0;
  uint32 order = 0;
  while (order < MAX_ORDER) {
    kmem.free_area[order].nr_free = 0;
    INIT_LIST_HEAD(kmem.free_area[order]);
  }
  #endif
  freerange(end, (void*)PHYSTOP);
}

#ifndef BUDDY_SYSTEM

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

#else

// only used in initialization
void
freerange(void *pa_start, void *pa_end)
{
  // use __free_page_bulk
  char *p;
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    __free_pages_bulk(pa_to_page(p), 0);
  }
}

// may should replace kalloc
void *
allocrange(uint32 order)
{
  acquire(&kmem.lock);
  /* copy linux code here */
  release(&kmem.lock);
}

void *
kalloc()
{
  return allocrange(0);
}


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

/* add freed page to free_area */
static inline void __free_pages_bulk (struct page *page, unsigned int order)
{
	unsigned long page_idx;
	struct page *coalesced;
	int order_size = 1 << order;

	page_idx = page - page_desp;

  // I comment this out just because I have not implemented BUG_ON
	// BUG_ON(page_idx & (order_size - 1));
	// BUG_ON(bad_range(zone, page));

	kmem.free_pages += order_size;
	while (order < MAX_ORDER-1) {
		struct free_area *area;
		struct page *buddy;
		int buddy_idx;

		buddy_idx = (page_idx ^ (1 << order));
		buddy = (struct page *)(page_desp + buddy_idx);
		if (!page_is_buddy(buddy, order))
			break;
		/* Move the buddy up one level. */
		list_del(&buddy->lru);
		area = kmem.free_area + order;
		area->nr_free--;
		rmv_page_order(buddy);
		page_idx &= buddy_idx;
		order++;
	}
	coalesced = page_desp + page_idx;
	set_page_order(coalesced, order);
	list_add(&coalesced->lru, &kmem.free_area[order].free_list);
	kmem.free_area[order].nr_free++;
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct page *pgdsp;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  acquire(&kmem.lock);
  pgdsp = pa_to_page(pa);
  __free_pages_bulk(pgdsp, pgdsp->order);
  release(&kmem.lock);
}

static inline struct page *expand(struct page *page, uint32 low,
  uint32 high, struct free_area *area) 
{
	unsigned long size = 1 << high;

	while (high > low) {
		area--;
		high--;
		size >>= 1;
		list_add(&page[size].lru, &area->free_list);
		area->nr_free++;
		set_page_order(&page[size], high);
	}
	return page;
}

static struct page *__rmqueue(uint32 order)
{
	struct free_area * area;
	unsigned int current_order;
	struct page *page;

	for (current_order = order; current_order < MAX_ORDER; ++current_order) {
		area = kmem.free_area + current_order;
		if (list_empty(&area->free_list))
			continue;

		page = list_entry(area->free_list.next, struct page, lru);
		list_del(&page->lru);
		rmv_page_order(page);
		area->nr_free--;
		kmem.free_pages -= 1UL << order;
		return expand(page, order, current_order, area);
	}

	return NULL;
}

#endif
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
  // uint64 flags;
  struct list_head lru;
  uint32 order;
} page_desp[NR_PGDESP];

struct free_area {
  struct list_head free_list;
  uint64 nr_free;
};


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

#endif



#endif
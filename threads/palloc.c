#include "threads/palloc.h"
#include <bitmap.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/loader.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#define MIN(a, b) ((a)<(b) ? (a):(b))

/* Page allocator.  Hands out memory in page-size (or
   page-multiple) chunks.  See malloc.h for an allocator that
   hands out smaller chunks.

   System memory is divided into two "pools" called the kernel
   and user pools.  The user pool is for user (virtual) memory
   pages, the kernel pool for everything else.  The idea here is
   that the kernel needs to have memory for its own operations
   even if user processes are swapping like mad.

   By default, half of system RAM is given to the kernel pool and
   half to the user pool.  That should be huge overkill for the
   kernel pool, but that's just fine for demonstration purposes. */

/* A memory pool. */
struct pool {
    struct lock lock;        /* Mutual exclusion. */
    struct bitmap *used_map; /* Bitmap of free pages. */
    uint8_t *base;           /* Base of pool. */

   //[추가]다음 Fit 검색을 시작할 인덱스 (Next-Fit 구현에 필수)
    size_t next_idx;            /* Index to start the next scan for next-fit. */ 
    
    //[추가]pool에 포함된 총 페이지 수 (bitmap_size()와 동일할 수 있으나, 일반적으로 명시)
    size_t pages;               /* Total number of pages in the pool. */
};

/* Two pools: one for kernel data, one for user pages. */
static struct pool kernel_pool, user_pool;

/* Buddy System Globals */
#define BUDDY_SYSTEM_MAX_ORDER 10 // 최대 차수(k)의 상한선 (일반적으로 10~15)

/* 각 차수(2^k 페이지)별로 빈 블록의 시작 인덱스를 관리하는 리스트 배열. 
   buddy_free_list[k]는 2^k 크기의 빈 블록 리스트입니다. */
static struct list buddy_free_list[BUDDY_SYSTEM_MAX_ORDER + 1]; 

/* 빈 블록을 리스트에 연결하기 위한 구조체 */
struct buddy_elem {
    struct list_elem elem;
    size_t page_idx; // 블록의 시작 페이지 인덱스
};

/* 전체 가용 커널 페이지 수(N) 및 Buddy System의 최대 차수(K) */
static size_t kernel_pool_max_pages; 
static size_t buddy_system_max_k;
static size_t get_buddy_order(size_t page_cnt);
static size_t buddy_alloc(size_t page_cnt);
static void buddy_free(size_t page_idx, size_t page_cnt);


static void init_pool(struct pool *, void *base, size_t page_cnt,
                      const char *name);
static bool page_from_pool(const struct pool *, void *page);

/* Current allocation mode. */
static enum palloc_mode palloc_mode = PAL_FIRST_FIT;

static size_t palloc_next_fit_scan(struct pool *pool, size_t page_cnt);
static size_t palloc_best_fit_scan(struct pool *pool, size_t page_cnt);

/* Sets the allocation mode. */
void
palloc_set_mode (enum palloc_mode mode)
{
  palloc_mode = mode;
}

/* Initializes the page allocator.  At most USER_PAGE_LIMIT
   pages are put into the user pool. */
void palloc_init(size_t user_page_limit)
{
    /* Free memory starts at 1 MB and runs to the end of RAM. */
    uint8_t *free_start = ptov(1024 * 1024);
    uint8_t *free_end = ptov(init_ram_pages * PGSIZE);
    size_t free_pages = (free_end - free_start) / PGSIZE;
    size_t user_pages = free_pages / 2;
    size_t kernel_pages;
    if (user_pages > user_page_limit)
        user_pages = user_page_limit;
    kernel_pages = free_pages - user_pages;

    /* Give half of memory to kernel, half to user. */
    init_pool(&kernel_pool, free_start, kernel_pages, "kernel pool");
    init_pool(&user_pool, free_start + kernel_pages * PGSIZE,
              user_pages, "user pool");

   /* --- [Buddy System 초기화 시작] --- */
    kernel_pool_max_pages = bitmap_size(kernel_pool.used_map);
    
    // K 값 계산: 2^K >= N 인 최소 K 찾기
    buddy_system_max_k = 0;
    size_t temp_size = 1;
    while (temp_size < kernel_pool_max_pages) {
        temp_size *= 2;
        buddy_system_max_k++;
    }

    if (buddy_system_max_k > BUDDY_SYSTEM_MAX_ORDER) {
        PANIC("Kernel memory is too large for Buddy System implementation.");
    }
    
    // 빈 리스트 초기화
    for (size_t k = 0; k <= buddy_system_max_k; k++) {
        list_init(&buddy_free_list[k]);
    }

    // 최대 크기 블록(2^K)을 빈 리스트에 추가
    struct buddy_elem *initial_elem = 
        (struct buddy_elem *)palloc_get_multiple(PAL_ASSERT, 
                                                 DIV_ROUND_UP(sizeof(struct buddy_elem), PGSIZE));
    
    // Buddy System은 메모리 인덱스 0부터 시작한다고 가정
    initial_elem->page_idx = 0; 
    list_push_back(&buddy_free_list[buddy_system_max_k], &initial_elem->elem);

    printf("Buddy System initialized: Max pages=%zu, Max order K=%zu\n", 
           kernel_pool_max_pages, buddy_system_max_k);
    /* --- [Buddy System 초기화 종료] --- */
}

/* Obtains and returns a group of PAGE_CNT contiguous free pages.
   If PAL_USER is set, the pages are obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the pages are filled with zeros.  If too few pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
void *
palloc_get_multiple(enum palloc_flags flags, size_t page_cnt)
{
    struct pool *pool = flags & PAL_USER ? &user_pool : &kernel_pool;
    void *pages;
    size_t page_idx;
   enum palloc_mode mode = palloc_mode; //모드 선언

    if (page_cnt == 0)
        return NULL;

    lock_acquire(&pool->lock);
   //[추가]할당 모드에 따른 multiple-partition allocation 처리
   switch (mode) {
        case PAL_FIRST_FIT:
            /* First Fit (기존 구현과 동일): 처음(0)부터 검색 */
            page_idx = bitmap_scan_and_flip(pool->used_map, 0, page_cnt, false);
            break;

        case PAL_NEXT_FIT:
            /* Next Fit: 전용 스캔 함수 사용 */
            page_idx = palloc_next_fit_scan(pool, page_cnt);
            break;

        case PAL_BEST_FIT:
            /* Best Fit: 전용 스캔 함수 사용 */
            page_idx = palloc_best_fit_scan(pool, page_cnt);
            break;

        case PAL_BUDDY:
           /*Buddy System*/
            lock_release(&pool->lock); // 락 해제 (Buddy System은 자체적으로 동기화 필요)
            page_idx = buddy_alloc(page_cnt);
            lock_acquire(&pool->lock); // 락 재획득 (반환 직전)
            break;

        default:
            NOT_REACHED();
    }
   
    lock_release(&pool->lock);

    if (page_idx != BITMAP_ERROR)
        pages = pool->base + PGSIZE * page_idx;
    else
        pages = NULL;

    if (pages != NULL) {
        if (flags & PAL_ZERO)
            memset(pages, 0, PGSIZE * page_cnt);
    } else {
        if (flags & PAL_ASSERT)
            PANIC("palloc_get: out of pages");
    }

    return pages;
}

/* Obtains a single free page and returns its kernel virtual
   address.
   If PAL_USER is set, the page is obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the page is filled with zeros.  If no pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
void *
palloc_get_page(enum palloc_flags flags)
{
    return palloc_get_multiple(flags, 1);
}

/* Frees the PAGE_CNT pages starting at PAGES. */
void palloc_free_multiple(void *pages, size_t page_cnt)
{
    struct pool *pool;
    size_t page_idx;

    ASSERT(pg_ofs(pages) == 0);
    if (pages == NULL || page_cnt == 0)
        return;

    if (page_from_pool(&kernel_pool, pages))
        pool = &kernel_pool;
    else if (page_from_pool(&user_pool, pages))
        pool = &user_pool;
    else
        NOT_REACHED();

    page_idx = pg_no(pages) - pg_no(pool->base);

   // [추가] 락 획득 (동기화)
    lock_acquire(&pool->lock);
    
    // [추가] Buddy System 예외 처리
    if (palloc_mode == PAL_BUDDY) {
        lock_release(&pool->lock); // 락 해제 (Buddy System은 자체적으로 동기화 필요)
        buddy_free(page_idx, page_cnt);
        lock_acquire(&pool->lock); // 락 재획득 (반환 직전) 
}else{

#ifndef NDEBUG
    memset(pages, 0xcc, PGSIZE * page_cnt);
#endif

    ASSERT(bitmap_all(pool->used_map, page_idx, page_cnt));
    bitmap_set_multiple(pool->used_map, page_idx, page_cnt, false);

   // [추가] Next Fit 최적화 로직
    // Next Fit 모드이고, 해제된 블록이 다음 검색 시작 위치(next_idx)보다 
    // 앞에 있다면, next_idx를 이 블록의 시작 인덱스로 당겨서 검색 성능을 높입니다.
    if (palloc_mode == PAL_NEXT_FIT) {
        if (page_idx < pool->next_idx) {
            pool->next_idx = page_idx;
        }
    }
 }

    // [추가] 락 해제
    lock_release(&pool->lock);
}

/* Frees the page at PAGE. */
void palloc_free_page(void *page)
{
    palloc_free_multiple(page, 1);
}

/* Returns the index of the page in the pool's bitmap. */
size_t
palloc_get_page_index (void *page)
{
  struct pool *pool;

  if (page_from_pool (&kernel_pool, page))
    pool = &kernel_pool;
  else if (page_from_pool (&user_pool, page))
    pool = &user_pool;
  else
    return BITMAP_ERROR;

  return pg_no (page) - pg_no (pool->base);
}

/* Initializes pool P as starting at START and ending at END,
   naming it NAME for debugging purposes. */
static void
init_pool(struct pool *p, void *base, size_t page_cnt, const char *name)
{
    /* We'll put the pool's used_map at its base.
     Calculate the space needed for the bitmap
     and subtract it from the pool's size. */
    size_t bm_pages = DIV_ROUND_UP(bitmap_buf_size(page_cnt), PGSIZE);
    if (bm_pages > page_cnt)
        PANIC("Not enough memory in %s for bitmap.", name);
    page_cnt -= bm_pages;

    printf("%zu pages available in %s.\n", page_cnt, name);

    /* Initialize the pool. */
    lock_init(&p->lock);
    p->used_map = bitmap_create_in_buf(page_cnt, base, bm_pages * PGSIZE);
    p->base = base + bm_pages * PGSIZE;
}

/* Returns true if PAGE was allocated from POOL,
   false otherwise. */
static bool
page_from_pool(const struct pool *pool, void *page)
{
    size_t page_no = pg_no(page);
    size_t start_page = pg_no(pool->base);
    size_t end_page = start_page + bitmap_size(pool->used_map);

    return page_no >= start_page && page_no < end_page;
}
//----------------------------------------------------------------------------------------------
//next-fit방법 : 모든 hole 탐새 후 알맞는 빈 공간에 할당
static size_t palloc_next_fit_scan(struct pool *pool, size_t page_cnt)
{
    size_t page_idx = BITMAP_ERROR;
    size_t size = bitmap_size(pool->used_map);

    /* 1. next_idx 부터 끝까지 검색 */
    page_idx = bitmap_scan(pool->used_map, pool->next_idx, 
                           size - pool->next_idx, false);

    /* 2. 찾지 못했다면, 처음(0)부터 next_idx까지 검색 (순환) */
    if (page_idx == BITMAP_ERROR) {
        page_idx = bitmap_scan(pool->used_map, 0, 
                               pool->next_idx, false);
    }
    
    if (page_idx != BITMAP_ERROR) {
        /* 찾은 경우 할당 비트 플립 및 next_idx 업데이트 */
        bitmap_set_multiple(pool->used_map, page_idx, page_cnt, true);
        /* 다음 검색 시작 위치를 현재 할당 블록의 끝으로 설정 */
        pool->next_idx = (page_idx + page_cnt) % size;
    }
    
    return page_idx;
}

//best-fit방법 : 모든 hole 탐색 후 빈공간 많이 남는 곳에 할당
static size_t palloc_best_fit_scan(struct pool *pool, size_t page_cnt)
{
    size_t size = bitmap_size(pool->used_map);
    size_t best_idx = BITMAP_ERROR;
    size_t best_size = size + 1; /* 최대 크기보다 큰 값으로 초기화 */
    size_t current_idx = 0;
    
    /* Best Fit은 직접 순회하며 최적의 위치를 찾습니다. */
    while (current_idx < size) {
        /* 현재 위치에서 빈 페이지 블록의 크기를 찾습니다. */
        size_t free_run_len = bitmap_scan(pool->used_map, current_idx, 
                                           size - current_idx, false);

        /* 빈 블록을 찾지 못했다면 검색 종료 */
        if (free_run_len == BITMAP_ERROR) {
            break;
        }

        /* 빈 블록의 끝 인덱스를 찾습니다. */
        size_t run_end_idx = free_run_len + bitmap_scan(pool->used_map, 
                                                        free_run_len, 
                                                        size - free_run_len 
                                                        , true);
        
        /* 현재 빈 블록의 실제 크기 */
        size_t current_run_size = run_end_idx - free_run_len;

        /* 요청 크기를 만족하고, 현재까지 찾은 최적 크기보다 작은 경우 업데이트 */
        if (current_run_size >= page_cnt && current_run_size < best_size) {
            best_idx = free_run_len;
            best_size = current_run_size;
        }

        /* 다음 검색은 현재 빈 블록이 끝나는 지점에서 시작 */
        current_idx = run_end_idx;
    }

    /* 최적의 위치를 찾았다면 할당 */
    if (best_idx != BITMAP_ERROR) {
        bitmap_set_multiple(pool->used_map, best_idx, page_cnt, true);
        return best_idx;
    }

    return BITMAP_ERROR;
}

static size_t
get_buddy_order(size_t page_cnt)
{
    size_t k = 0;
    size_t size = 1;
    
    // page_cnt를 포함하는 최소의 2의 거듭제곱(2^k)을 찾습니다.
    while (size < page_cnt) {
        size *= 2;
        k++;
    }
    return k;
}
//----------------------------------------------------------------------------
/* Buddy System 할당 로직 */
static size_t
buddy_alloc(size_t page_cnt)
{
    size_t alloc_k = get_buddy_order(page_cnt);
    size_t k = alloc_k;
    size_t page_idx = BITMAP_ERROR;

    /* 1. k 차수 이상의 빈 블록 검색 */
    while (k <= buddy_system_max_k) {
        if (!list_empty(&buddy_free_list[k])) {
            // 빈 블록 발견: 리스트에서 꺼냄
            struct list_elem *e = list_pop_front(&buddy_free_list[k]);
            struct buddy_elem *elem = list_entry(e, struct buddy_elem, elem);
            page_idx = elem->page_idx;
            
            break; 
        }
        k++;
    }

    if (page_idx == BITMAP_ERROR) {
        return BITMAP_ERROR; // 메모리 부족
    }

    /* 2. 분할 (Split) */
    while (k > alloc_k) {
        k--; // 차수 감소
        size_t block_size = 1 << k; // 2^k
        
        // 버디 블록 인덱스: 현재 인덱스 + 블록 크기
        size_t buddy_idx = page_idx + block_size; 
        
        // 버디 블록을 다음 작은 차수(k)의 빈 리스트에 추가 (Split)
        struct buddy_elem *buddy_elem = 
            (struct buddy_elem *)palloc_get_multiple(PAL_ASSERT, 
                                                     DIV_ROUND_UP(sizeof(struct buddy_elem), PGSIZE));
        buddy_elem->page_idx = buddy_idx;
        list_push_back(&buddy_free_list[k], &buddy_elem->elem);
    }
    
    return page_idx;
}

/* Buddy System 해제 및 병합 로직 */
static void
buddy_free(size_t page_idx, size_t page_cnt)
{
    size_t k = get_buddy_order(page_cnt);
    size_t block_size = 1 << k;

    if (block_size != page_cnt) {
        // 요청된 해제 크기가 2의 거듭제곱이 아닌 경우 Buddy System 규칙 위반
        PANIC("Buddy System free size is not power of 2: %zu", page_cnt);
    }

    /* 병합 (Merge) */
    while (k < buddy_system_max_k) {
        size_t buddy_idx = page_idx ^ block_size; // 버디 인덱스 계산 (XOR 연산)
        struct list *free_list = &buddy_free_list[k];
        struct list_elem *e;
        
        bool merged = false;
        
        // 해당 차수 k의 빈 리스트에서 버디 블록 검색
        for (e = list_begin(free_list); e != list_end(free_list); e = list_next(e)) {
            struct buddy_elem *buddy_elem = list_entry(e, struct buddy_elem, elem);
            
            if (buddy_elem->page_idx == buddy_idx) {
                // 버디 블록 발견 (병합)
                list_remove(e);
               
                // 새로운 부모 블록의 시작 인덱스 결정
                page_idx = MIN(page_idx, buddy_idx);
                k++; // 차수 증가 (블록 크기 두 배)
                block_size *= 2;
                merged = true;
                break;
            }
        }
        
        if (!merged) {
            break; // 버디 블록이 비어있지 않으므로 병합 종료
        }
    }
    
    // 최종 블록을 해당 차수의 빈 리스트에 추가
    struct buddy_elem *elem = 
        (struct buddy_elem *)palloc_get_multiple(PAL_ASSERT, 
                                                 DIV_ROUND_UP(sizeof(struct buddy_elem), PGSIZE));
    elem->page_idx = page_idx;
    list_push_back(&buddy_free_list[k], &elem->elem);
}



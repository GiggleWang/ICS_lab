/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * 使用分离存储的方式，维护多个空闲链表，
 * 在本实验中是 size < 1024 , 1024 <= size < 4096 , 4096 <= size < 16384 , 16384 <= size
 * 四个空闲链表,每个链表中的块有大致相等的大小。
 * 
 * the structure of my free and allocated blocks：
 * 已分配的块结构：开头是header，用于存储存储块的大小和已分配状态；接下来是存储的负载
 * 未分配的块结构：开头是header，用于存储存储块的大小和未分配状态；接下来是空闲块；最后是footer，存相同的信息
 * 
 * the organization of the free list：
 * 使用四个独立的空闲链表，每个链表负责管理一个特定大小范围的空闲块。每个链表内的块通过内部指针相连，包括前驱指针和后继指针
 * 
 * how my allocator manipulates the free list:
 * insert：如果空闲列表中已有其他块，新块将被插入作为新的首块，更新前后块的前驱和后继指针。如果空闲列表为空，新块将初始化其前驱和后继指针为0，并设置为列表的首块。
 * remove: 修改对应的前后指针的内部指针
 * 
 */

/* 
 如果想要进行check测试，那么请取消该注释
 */
// #define CHECK


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"




/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

/* Basic constants and macros */
#define WSIZE 4        /* Word and header/footer size (bytes) */
#define DSIZE 8        /* Double word size (bytes) */
#define OVERHEAD 16    /* Min size of free block */
#define CHUNKSIZE (72) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and alloctated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allcated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Transform from address to offset and vice versa */
#define GET_OFFSET(bp) ((char *)bp - (char *)heap_listp)
#define GET_ADDR(offset) (heap_listp + offset)

/* Given block ptr bp, compute address of its header, footer, predecessor and successor */
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define PRED(bp) (bp)
#define SUCC(bp) ((bp) + WSIZE)

/* Given block ptr bp, compute address of next and previous blocks, next and previous free blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

#define NEXT_FREEP(bp) ((GET(SUCC(bp))) ? (GET_ADDR(GET(SUCC(bp)))) : NULL)
#define PREV_FREEP(bp) ((GET(PRED(bp))) ? (GET_ADDR(GET(PRED(bp)))) : NULL)

/* -------------------global variable------------------ */

/* pointer to the header of heap */
static void *heap_listp;

/* pointer to the header of free block lists */
static void *free_listp;      /* size < 1024 */
static void *free_listp1024;  /* 1024 <= size < 4096 */
static void *free_listp4096;  /* 4096 <= size < 16384 */
static void *free_listp16384; /* 16384 <= size */

/* pointer to the free block list latestly realloced which is at the end of heap */
static void *latest_freeblkp;
/* -------------------function declare------------------ */

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *place(void *bp, size_t asize);

static void *insert_freeblk(void *bp);
static void *remove_freeblk(void *bp);

static void **get_freelist_ptr(size_t size);
static void **get_larger_freelist_ptr(void **pp);

void *handleExpand(void *ptr, size_t size, size_t newSize, size_t oldSize, size_t nextFreeSize);
size_t getNextFreeSize(void *ptr);
void *handleShrink(void *ptr, size_t newSize, size_t oldSize);
size_t nextPowerOfTwo(size_t request_size);
int checkForEveryPtr(void *bp);

int mm_check(void);

/**
 * 获取下一个空闲块的大小。
 *
 * 此函数根据给定的指针，检查其后续内存块的分配状态，
 * 若已分配，则返回0；若未分配，则返回该块的大小。
 *
 * @param ptr 当前块的指针。
 * @return 下一个内存块的大小，如果已分配则为0。
 */
size_t getNextFreeSize(void *ptr)
{
    return GET_ALLOC(HDRP(NEXT_BLKP(ptr))) ? 0 : GET_SIZE(HDRP(NEXT_BLKP(ptr)));
}

/**
 * 处理内存缩小的情况。
 *
 * 该函数将一个已分配的内存块大小减小到新的尺寸，并将剩余的部分转换为一个新的空闲块。
 * 更新当前块和新空闲块的头部和尾部，并尝试与相邻的空闲块合并。
 *
 * @param ptr 当前块的指针。
 * @param newSize 缩小后的新大小。
 * @param oldSize 缩小前的原大小。
 * @return 指向缩小后的内存块的指针。
 */
void *handleShrink(void *ptr, size_t newSize, size_t oldSize)
{
    PUT(HDRP(ptr), PACK(newSize, 1));
    PUT(FTRP(ptr), PACK(newSize, 1));
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(oldSize - newSize, 0));
    PUT(FTRP(NEXT_BLKP(ptr)), PACK(oldSize - newSize, 0));
    insert_freeblk(NEXT_BLKP(ptr));
    coalesce(NEXT_BLKP(ptr));
    return ptr;
}

/**
 * 处理内存扩展的情况。
 *
 * 当需要扩大一个已分配的内存块时，此函数尝试利用相邻的空闲块来满足请求的内存大小。
 * 如果相邻的空闲块足够大或者可以进行扩展，则合并当前块与相邻的空闲块。
 * 如果空间不足以扩展，则会分配一个新的内存块，并将旧数据复制过去，释放原内存块。
 *
 * @param ptr 当前块的指针。
 * @param size 请求的内存大小。
 * @param newSize 扩展后的新大小。
 * @param oldSize 当前块的原始大小。
 * @param nextFreeSize 下一个空闲块的大小。
 * @return 指向扩展后或新分配内存块的指针，如果无法扩展或分配新内存则返回 NULL。
 */
void *handleExpand(void *ptr, size_t size, size_t newSize, size_t oldSize, size_t nextFreeSize)
{
    int enough = nextFreeSize >= newSize - oldSize + OVERHEAD;
    int canExpand = (nextFreeSize && !GET_SIZE(HDRP(NEXT_BLKP(NEXT_BLKP(ptr))))) || !GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    if (!enough && canExpand)
    {
        extend_heap(MAX(newSize - oldSize, CHUNKSIZE) / WSIZE);
        nextFreeSize = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
    }
    if (enough || canExpand)
    {
        remove_freeblk(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(newSize, 1));
        PUT(FTRP(ptr), PACK(newSize, 1));
        if (nextFreeSize != newSize - oldSize)
        {
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(oldSize + nextFreeSize - newSize, 0));
            PUT(FTRP(NEXT_BLKP(ptr)), PACK(oldSize + nextFreeSize - newSize, 0));
            insert_freeblk(NEXT_BLKP(ptr));
            latest_freeblkp = NEXT_BLKP(ptr);
        }
        return ptr;
    }
    else
    {
        void *newptr = mm_malloc(size);
        if (newptr == NULL)
            return NULL;
        memcpy(newptr, ptr, oldSize - DSIZE);
        mm_free(ptr);
        return newptr;
    }
}

/**
 * 扩展堆以包含更大的大小。
 *
 * 此函数通过调用mem_sbrk来增加堆的大小，以适应更多的分配请求。
 * 新增加的空间会被初始化为一个空闲块，并尝试与其前一个空闲块进行合并。
 * 函数确保扩展的大小按字（WSIZE）对齐，如果请求的字数为奇数，则增加一个字以保持对齐。
 *
 * @param words 需要扩展的字数。
 * @return 指向新空闲块的指针，如果内存扩展失败则返回 NULL。
 */
static void *extend_heap(size_t words)
{
    char *p;
    size_t size;

    /* 分配偶数个字以维持对齐 */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(p = mem_sbrk(size)) == -1)
        return NULL; // 如果内存扩展失败，返回NULL。

    /* 初始化空闲块的头部/脚部以及新的结尾块头部 */
    PUT(HDRP(p), PACK(size, 0));         /* 空闲块头部 */
    PUT(FTRP(p), PACK(size, 0));         /* 空闲块脚部 */
    insert_freeblk(p);                   /* 插入空闲块到空闲列表 */
    PUT(HDRP(NEXT_BLKP(p)), PACK(0, 1)); /* 新的结尾块头部 */

    /* 如果前一个块是空闲的，则进行合并 */
    return coalesce(p);
}

/**
 * 获取适合给定块大小的空闲列表指针。
 *
 * @param size 块的大小。
 * @return 对应大小的空闲列表的头指针的地址。
 */
static void **get_freelist_ptr(size_t size) // FREELISTPP
{
    if (size >= 16384)
    {
        return &free_listp16384;
    }
    else if (size >= 4096)
    {
        return &free_listp4096;
    }
    else if (size >= 1024)
    {
        return &free_listp1024;
    }
    else
    {
        return &free_listp;
    }
}

/**
 * 获取给定空闲列表指针的下一个更大的空闲列表指针。
 *
 * @param pp 当前空闲列表指针的地址。
 * @return 下一个更大的空闲列表的头指针的地址，如果已是最大，则返回 NULL。
 */
static void **get_larger_freelist_ptr(void **pp) // LARGER_FREELISTPP
{
    if (pp == &free_listp)
    {
        return &free_listp1024;
    }
    else if (pp == &free_listp1024)
    {
        return &free_listp4096;
    }
    else if (pp == &free_listp4096)
    {
        return &free_listp16384;
    }
    else
    {
        return NULL; // 如果已是最大或不匹配，则返回 NULL。
    }
}

/**
 * 尝试合并给定块与其相邻的空闲块。
 *
 * 此函数检查给定块的前一个块和后一个块的分配状态。
 * 如果相邻块是空闲的，则将它们合并成一个更大的块。
 * 合并操作包括更新块的头部和脚部信息，并从空闲链表中添加或移除相应的块。
 * 函数会根据不同情况（前一块空闲、后一块空闲、前后块都空闲）进行合并。
 *
 * @param current_ptr 当前块的指针。
 * @return 指向合并后块的指针，可能是原始指针或者前一块的指针，取决于合并情况。
 */
static void *coalesce(void *current_ptr)
{
    void *prev_ptr = PREV_BLKP(current_ptr);           // 获取前一块的指针
    void *next_ptr = NEXT_BLKP(current_ptr);           // 获取后一块的指针
    size_t prev_free = GET_ALLOC(FTRP(prev_ptr));      // 检查前一块是否已分配
    size_t next_free = GET_ALLOC(HDRP(next_ptr));      // 检查后一块是否已分配
    size_t current_size = GET_SIZE(HDRP(current_ptr)); // 当前块的大小

    /* 检查相邻块是否都已分配 */
    if (prev_free && next_free)
    {
        return current_ptr;
    }
    /* 后一块空闲，合并当前块与后一块 */
    if (prev_free && !next_free)
    {
        size_t combined_size = current_size + GET_SIZE(HDRP(next_ptr)); // 计算合并后的新大小
        remove_freeblk(next_ptr);                                       // 从空闲链表中移除后一块
        remove_freeblk(current_ptr);                                    // 从空闲链表中移除当前块
        PUT(HDRP(current_ptr), PACK(combined_size, 0));                 // 设置新头部
        PUT(FTRP(current_ptr), PACK(combined_size, 0));                 // 设置新脚部
        insert_freeblk(current_ptr);                                    // 将新块重新插入空闲链表
    }
    /* 前一块空闲，合并前一块与当前块 */
    if (!prev_free && next_free)
    {
        size_t combined_size = current_size + GET_SIZE(HDRP(prev_ptr)); // 计算合并后的新大小
        remove_freeblk(current_ptr);                                    // 从空闲链表中移除当前块
        remove_freeblk(prev_ptr);                                       // 从空闲链表中移除前一块
        PUT(FTRP(current_ptr), PACK(combined_size, 0));                 // 设置新脚部
        PUT(HDRP(prev_ptr), PACK(combined_size, 0));                    // 设置新头部
        insert_freeblk(prev_ptr);                                       // 将新块重新插入空闲链表
        current_ptr = prev_ptr;                                         // 更新当前指针为合并后的块
    }
    /* 前后块都空闲，合并前一块、当前块及后一块 */
    if (!prev_free && !next_free)
    {
        size_t combined_size = current_size + GET_SIZE(HDRP(next_ptr)) + GET_SIZE(HDRP(prev_ptr)); // 计算合并后的新大小
        remove_freeblk(next_ptr);                                                                  // 从空闲链表中移除后一块
        remove_freeblk(current_ptr);                                                               // 从空闲链表中移除当前块
        remove_freeblk(prev_ptr);                                                                  // 从空闲链表中移除前一块
        PUT(HDRP(prev_ptr), PACK(combined_size, 0));                                               // 设置新头部
        PUT(FTRP(next_ptr), PACK(combined_size, 0));                                               // 设置新脚部
        insert_freeblk(prev_ptr);                                                                  // 将新块重新插入空闲链表
        current_ptr = prev_ptr;                                                                    // 更新当前指针为合并后的块
    }

    return current_ptr; // 返回可能已合并的块的指针
}

/**
 * 将指定的块插入适当的空闲块列表中。
 *
 * 此函数根据内存块的大小，选择适合的空闲列表，并将该块插入列表的头部。
 * 如果空闲列表中已有其他块，新块将被插入作为新的首块，更新前后块的前驱和后继指针。
 * 如果空闲列表为空，新块将初始化其前驱和后继指针为0，并设置为列表的首块。
 *
 * @param block_ptr 指向需要插入的空闲块的指针。
 * @return 返回指向已插入的空闲块的指针。
 */
static void *insert_freeblk(void *block_ptr)
{
    // 根据块的大小获取对应的空闲列表的头指针
    void **freelist_head = get_freelist_ptr(GET_SIZE(HDRP(block_ptr)));

    // 如果对应的空闲列表非空，则插入块到列表头部
    if (*freelist_head)
    {
        // 设置新插入块的前驱和后继
        PUT(PRED(*freelist_head), GET_OFFSET(block_ptr)); // 将列表现有首块的前驱设置为新块
        PUT(SUCC(block_ptr), GET_OFFSET(*freelist_head)); // 将新块的后继设置为列表现有首块
        PUT(PRED(block_ptr), 0);                          // 新块没有前驱
    }
    else
    {
        // 初始化新块的前驱和后继为0，表示它是列表中的唯一块
        PUT(PRED(block_ptr), 0);
        PUT(SUCC(block_ptr), 0);
    }
    // 更新空闲列表的头指针为新插入的块
    *freelist_head = block_ptr;

    return block_ptr;
}

/**
 * 从相应的空闲列表中移除指定的块。
 *
 * 此函数负责将一个块从其所在的空闲列表中移除。处理涉及更新相关的前驱和后继指针，
 * 确保列表的完整性。如果块是列表的头部或尾部，还需要更新空闲列表的头指针。
 * 移除操作根据块的位置（列表头、中间、尾或独立块）执行不同的更新。
 *
 * @param block_ptr 指向需要移除的空闲块的指针。
 * @return 返回被移除的块的指针。
 */
static void *remove_freeblk(void *block_ptr)
{
    // 获取块所在的空闲列表头指针
    void **freelist_head = get_freelist_ptr(GET_SIZE(HDRP(block_ptr)));
    void *prev_block = PREV_FREEP(block_ptr);
    void *next_block = NEXT_FREEP(block_ptr);

    // 如果块在列表中间，更新前后块的指针
    if (prev_block && next_block)
    {
        PUT(SUCC(prev_block), GET(SUCC(block_ptr)));
        PUT(PRED(next_block), GET(PRED(block_ptr)));
    }
    // 如果块是列表的尾部，只更新前一块的后继指针
    else if (prev_block && !next_block)
    {
        PUT(SUCC(prev_block), 0);
    }
    // 如果块是列表的头部，只更新后一块的前驱指针，并更新列表头指针
    else if (!prev_block && next_block)
    {
        PUT(PRED(next_block), 0);
        *freelist_head = next_block;
    }
    // 如果块是列表中唯一的元素，将列表头指针置为NULL
    else
    {
        *freelist_head = NULL;
    }

    return block_ptr; // 返回被移除的块的指针
}

/**
 * 寻找适合分配请求大小的空闲块。
 *
 * 此函数遍历不同的空闲列表，寻找能满足请求大小的空闲块。开始时，它从与请求大小最匹配的列表开始搜索，
 * 如果在该列表中找不到合适的块，将继续搜索较大的空闲列表。每个列表中的块会按大小顺序遍历，
 * 一旦找到一个足够大的块，即返回该块的指针。如果所有列表都无合适块，则返回NULL。
 * 特别地，函数会避开最近一次释放的块以提高内存利用效率。
 *
 * @param requested_size 请求分配的内存大小。
 * @return 如果找到合适的空闲块，则返回指向该块的指针；否则返回NULL。
 */
void *find_fit(size_t requested_size)
{
    // 根据请求的大小获取合适的空闲列表
    void **freelist_head = get_freelist_ptr(requested_size);
    size_t current_block_size;

    // 遍历空闲列表链表，查找足够大的空闲块
    while (freelist_head)
    {
        void *current_block = *freelist_head;
        while (current_block)
        {
            current_block_size = GET_SIZE(HDRP(current_block));
            // 如果当前块为空或请求大小非法，则终止查找
            if ((requested_size <= 0) || (current_block == NULL))
            {
                return NULL;
            }
            // 如果当前块大小适合且不是最近一次释放的块，则返回该块
            if (requested_size <= current_block_size && current_block != latest_freeblkp)
            {
                return current_block;
            }
            // 移动到下一个空闲块
            current_block = NEXT_FREEP(current_block);
        }
        // 如果当前大小类中没有合适的块，移动到下一个更大的大小类
        freelist_head = get_larger_freelist_ptr(freelist_head);
    }

    return NULL; // 如果没有找到合适的块，返回 NULL
}

/**
 * 在指定的空闲块中放置所需大小的块。
 *
 * 此函数负责在一个空闲块中分配指定大小的内存区域。如果在分配后剩余的空间足够大以容纳一个空闲块（考虑必需的头部和脚部管理数据），
 * 则将该空闲块拆分，更新其大小，并将其重新插入到空闲列表中。如果剩余空间不足以形成一个新的空闲块，则整个块被标记为已分配。
 * 函数首先从空闲列表中移除被选中的块，然后根据实际剩余的大小进行处理，并返回新分配的块的地址。
 *
 * @param free_block 指向需要分配的空闲块的指针。
 * @param allocation_size 请求分配的内存大小。
 * @return 返回指向新分配的块的指针。
 */
void *place(void *free_block, size_t allocation_size)
{
    size_t block_size = GET_SIZE(HDRP(free_block));
    size_t tmp_size = block_size - allocation_size;
    // 当当前块大小减去所需大小后仍有足够空间放置一个包含管理数据的块时
    if ((tmp_size) >= OVERHEAD)
    {
        remove_freeblk(free_block);               // 从空闲列表中移除当前块
        PUT(HDRP(free_block), PACK(tmp_size, 0)); // 更新当前块的头部
        PUT(FTRP(free_block), PACK(tmp_size, 0)); // 更新当前块的脚部

        void *new_block = NEXT_BLKP(free_block);        // 定位新分配块的起始位置
        PUT(HDRP(new_block), PACK(allocation_size, 1)); // 设置新分配块的头部
        PUT(FTRP(new_block), PACK(allocation_size, 1)); // 设置新分配块的脚部
        insert_freeblk(free_block);                     // 将更新后的原块重新插入空闲列表

        return new_block; // 返回新分配块的地址
    }
    else
    {
        remove_freeblk(free_block);                 // 从空闲列表中移除当前块
        PUT(HDRP(free_block), PACK(block_size, 1)); // 设置整个块为已分配
        PUT(FTRP(free_block), PACK(block_size, 1)); // 更新脚部
        return free_block;                          // 返回当前块地址
    }
}

/**
 * 初始化内存管理器。
 *
 * 此函数设置初始堆的基本结构，包括对齐填充、序言块和结尾块，以确保堆的正确对齐。
 * 它还初始化了几个不同大小的空闲列表头指针。初始堆扩展为包含一个基本大小的空闲块。
 * 如果堆的初始扩展失败，函数返回-1；成功则返回0，表示内存管理器已准备好进行内存分配操作。
 *
 * @return 成功时返回0，失败时返回-1。
 */
int mm_init(void)
{
    // 初始化堆，初始大小为4个字（word）的空间。
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1; // 如果扩展堆失败，则返回-1。

    // 将所有大小类的空闲链表指针初始化为NULL。
    free_listp = NULL;
    free_listp1024 = NULL;
    free_listp4096 = NULL;
    free_listp16384 = NULL;

    // 设置对齐填充。
    PUT(heap_listp, PACK(0, 0));
    // 设置序言块的头部。
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    // 设置序言块的脚部。
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    // 设置结尾块的头部。
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    // 将heap_listp指针向前移动，跳过序言块。
    heap_listp += (2 * WSIZE);

    // 使用CHUNKSIZE字节的空闲块扩展堆。
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1; // 如果扩展堆失败，则返回-1。

    return 0; // 如果初始化成功，返回0。
}

/**
 * 计算大于或等于请求大小的最小二的幂。
 *
 * 此函数用于在内存分配中调整请求大小，确保分配的块大小是二的幂次数，
 * 且至少为64，以满足内存管理的效率和对齐要求。如果请求大小已是二的幂次数且不小于64，
 * 则直接返回该大小；否则，找到并返回最接近的更大的二的幂。
 *
 * @param request_size 原始请求的内存大小。
 * @return 返回大于或等于request_size的最小二的幂。
 */
size_t nextPowerOfTwo(size_t request_size)
{
    if (request_size <= 1)
    {
        return 64; // 直接返回64，因为这是最小的符合条件的2的幂次数
    }

    // 已经是 2 的幂次数，且大于等于 64
    if ((request_size & (request_size - 1)) == 0)
    {
        return request_size >= 64 ? request_size : 64;
    }

    size_t v = request_size;
    v--; // 允许 v 本身就是 2 的幂次数
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v < 64 ? 64 : v; // 确保结果至少为 64
}

int maxKey = (1 << 13);
int minKey = (1 << 4);
/**
 * 分配指定大小的内存块。
 *
 * 此函数首先检查如果定义了CHECK宏则执行堆完整性检查。函数处理内存请求，如果请求的大小在指定的范围内，则将大小调整为最近的二的幂。
 * 针对有效的内存请求，计算满足对齐和最小块要求的实际块大小。如果找不到足够大的空闲块，将扩展堆。
 * 使用手动对齐确保每个分配的块的大小符合对齐要求。通过`find_fit`尝试寻找足够大的空闲块；
 * 如果失败，通过`extend_heap`扩展堆。最后，通过`place`函数配置内存块，以确保有效利用空间。
 *
 * @param request_size 请求分配的内存大小。
 * @return 返回指向分配内存块的指针，如果无法满足分配请求则返回NULL。
 */
void *mm_malloc(size_t request_size)
{
#ifdef CHECK
    mm_check();
#endif
    if (request_size < maxKey && request_size > minKey)
    {
        request_size = nextPowerOfTwo(request_size);
    }

    size_t block_size;       /* 需要分配的内存块的实际大小 */
    size_t heap_extend_size; /* 当找不到合适的空闲块时，需要扩展的堆的大小 */
    char *allocated_block;

    // 忽略无效的分配请求
    if (request_size == 0)
    {
        return NULL;
    }

    // 调整请求大小以满足最小块要求并包括额外的管理开销
    if (request_size <= DSIZE)
    {
        block_size = 2 * DSIZE; // 最小块大小
    }
    else
    {
        // 手动进行内存对齐计算，以确保块大小是8的倍数
        block_size = (request_size + DSIZE + (ALIGNMENT - 1)) & ~0x7;
    }

    // 尝试找到一个足够大的空闲块
    if ((allocated_block = find_fit(block_size)) == NULL)
    {
        heap_extend_size = MAX(block_size, CHUNKSIZE); // 计算需要扩展的堆的大小
        if ((allocated_block = extend_heap(heap_extend_size / WSIZE)) == NULL)
        {
            return NULL; // 扩展堆失败，返回NULL
        }
    }

    // 配置内存块以满足请求
    allocated_block = place(allocated_block, block_size);
    return allocated_block;
}

/**
 * 释放指定的内存块。
 *
 * 此函数负责将一个已分配的内存块标记为未分配，并重新插入到适当的空闲列表中以便再次使用。
 * 释放操作包括设置块的头部和脚部标记为未分配状态，并尝试与其相邻的空闲块进行合并，
 * 这有助于减少内存碎片和提高内存利用率。如果编译时定义了CHECK宏，则会在操作前后进行堆完整性检查。
 *
 * @param block_ptr 指向需要释放的内存块的指针。
 */
void mm_free(void *block_ptr)
{
#ifdef CHECK
    mm_check();
#endif
    // 获取要释放的块的大小
    size_t block_size = GET_SIZE(HDRP(block_ptr));

    // 设置块的头部和脚部为未分配状态
    PUT(HDRP(block_ptr), PACK(block_size, 0));
    PUT(FTRP(block_ptr), PACK(block_size, 0));

    // 将块插入到适当的空闲列表中
    insert_freeblk(block_ptr);

    // 尝试与相邻的空闲块进行合并，以减少内存碎片
    coalesce(block_ptr);
}

/**
 * 重新分配内存块。
 *
 * 此函数用于调整已分配内存块的大小。处理包括三种主要情况：
 * 1. 如果传入的指针为NULL，相当于调用mm_malloc申请新的内存块。
 * 2. 如果请求的大小为零，相当于调用mm_free释放当前内存块，并返回NULL。
 * 3. 如果旧内存块足够大或可以调整来满足新的请求大小，函数将调整块大小并可能进行内存块的合并或拆分。
 * 4. 如果旧块不足以满足新大小，将尝试找到一个足够大的新块，可能涉及内存扩展。
 * 在调整大小过程中，会考虑内存对齐和最小块大小。
 *
 * @param ptr 指向当前已分配内存块的指针。
 * @param size 请求的新内存大小。
 * @return 返回重新分配后的内存块的指针。如果无法重新分配，返回NULL。
 */
void *mm_realloc(void *ptr, size_t size)
{
#ifdef CHECK
    mm_check(); // 检查堆的完整性
#endif
    /* 处理特殊情况：指针为空或请求大小为零 */
    if (ptr == NULL)
        return mm_malloc(size);
    if (size == 0)
    {
        mm_free(ptr);
        return NULL;
    }

    /* 计算新旧大小并获取下一块的空闲大小 */
    size_t newSize = ALIGN(size) + DSIZE;
    size_t oldSize = GET_SIZE(HDRP(ptr));
    size_t nextFreeSize = getNextFreeSize(ptr);

    /* 直接返回原指针，大小无变化 */
    if (newSize == oldSize)
        return ptr;

    /* 处理内存缩小 */
    if (newSize < oldSize)
    {
        return handleShrink(ptr, newSize, oldSize);
    }

    /* 处理内存扩展 */
    return handleExpand(ptr, size, newSize, oldSize, nextFreeSize);
}

/**
 * 检查内存管理器的完整性。
 *
 * 此函数执行多项检查以验证堆和空闲列表的完整性。首先，它检查所有在空闲列表中的块确实处于未分配状态。
 * 然后，验证空闲列表中块的链表指针是否正确，即每个块的后继和前驱指针是否匹配。
 * 最后，比较堆中未分配块的数量与空闲列表中块的数量是否一致，确保所有未分配的块都被正确地记录在空闲列表中。
 * 如果任何一项检查失败，函数将输出错误信息并返回0，否则返回1。
 *
 * @return 如果所有检查都通过，则返回1；如果有错误发生，则返回0。
 */
int mm_check(void)
{
    // printf("1");
    if (checkForEveryPtr(free_listp) == 0)
    {
        return 0;
    }
    if (checkForEveryPtr(free_listp1024) == 0)
    {
        return 0;
    }
    if (checkForEveryPtr(free_listp4096) == 0)
    {
        return 0;
    }
    if (checkForEveryPtr(free_listp16384) == 0)
    {
        return 0;
    }

    int heap_list_counter = 0;
    int free_list_counter = 0;
    /* 开始检查是否所有的free block都在freelist里面 */
    for (void *bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)))
        {
            heap_list_counter++;
        }
    }

    for (void *bp = free_listp; bp; bp = NEXT_FREEP(bp))
    {
        free_list_counter++;
    }
    for (void *bp = free_listp1024; bp; bp = NEXT_FREEP(bp))
    {
        free_list_counter++;
    }
    for (void *bp = free_listp4096; bp; bp = NEXT_FREEP(bp))
    {
        free_list_counter++;
    }
    for (void *bp = free_listp16384; bp; bp = NEXT_FREEP(bp))
    {
        free_list_counter++;
    }
    if (heap_list_counter != free_list_counter)
    {
        fprintf(stderr, "ERROR!!! heap_list_counter is no equal to free_list_counter\n");
        return 0;
    }
    return 1;
}

/**
 * 对一个特定的list检查是否符合要求
 * @param bp1 需要检查的list指针
 * @return 如果所有检查都通过，则返回1；如果有错误发生，则返回0。
 */
int checkForEveryPtr(void *bp1)
{
    for (void *bp = bp1; bp; bp = NEXT_FREEP(bp))
    {
        /* 检查在freelist里面是否有已经使用的块 */
        if (GET_ALLOC(HDRP(bp)))
        {
            fprintf(stderr, "ERROR!!! There is Allocated Blocks in FreeList!\n");
            return 0;
        }
        if (GET(SUCC(bp)) != 0)
        {
            void *next_bp = NEXT_FREEP(bp);           // 获取后继块的指针
            size_t current_offset = GET_OFFSET(bp);   // 获取当前块的偏移量
            size_t next_succ_offset = GET(SUCC(bp));  // 获取当前块记录的后继偏移量
            size_t pred_of_next = GET(PRED(next_bp)); // 获取后继块的前驱偏移量

            /* 检查后继块的前驱是否指向当前块，且当前块记录的后继偏移量是否与实际后继块的偏移量匹配  */
            if (pred_of_next != current_offset || next_succ_offset != GET_OFFSET(next_bp))
            {
                fprintf(stderr, "ERROR!!! Free Blocks Offset Do Not Match!\n");
                return 0;
            }
        }
        return 1;
    }
    return 1;
}
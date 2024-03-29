/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "junyongTeam",
    /* First member's full name */
    "JunyongLee",
    /* First member's email address */
    "ljyong3339@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x, y) (x > y ? x : y)
#define PACK(size, alloc) (size|alloc)
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int*)(p) = val)
#define GET_SIZE(p) (GET(p) & (~0x7))
#define GET_ALLOC(p) (GET(p) & (0x7))
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - WSIZE)
#define NEXT_BLEK(bp) ((char*)(bp) + GET_SIZE((char*)(bp) - WSIZE))
#define PREV_BLEK(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

//아래와 같이 수정 가능한지 추후에 체크
// #define NEXT_BLEK2(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))
// #define PREV_BLEK2(bp) ((char*)(bp) - GET_SIZE(HDRP(bp) - WSIZE))

/* 
 * mm_init - initialize the malloc package.
 */

static void *extend_heap(size_t input_size);
static void *coalesce(void* bp);
static void *find_fit(size_t asize);


static void *extend_heap(size_t input_size){
    char *bp;
    //사이즈 조정 - pedding 해야 하기 때문에 필수임
    size_t revise_size = input_size % 2 ? (input_size + 1)*WSIZE : (input_size)*WSIZE;
    if((long)(bp = mem_sbrk(revise_size)) == -1)return NULL;
    //extend 시 메모리 공간 새로 생기니까 header, footer setting 필수
    PUT(HDRP(bp), PACK(revise_size, 0));
    PUT(FTRP(bp), PACK(revise_size, 0));
    //epilogue block 초기화 필수인지?
    return coalesce(bp);
}

static void *coalesce(void * bp){
    //this method use when erase bp block -> but why use it in extend_heap???
    size_t prev_alloc = GET_ALLOC(PREV_BLEK(HDRP(bp)));
    size_t next_alloc = GET_ALLOC(NEXT_BLEK(HDRP(bp)));
    size_t now_size = GET_SIZE(HDRP(bp));
    
    //case 0 - prev, next exist
    if(prev_alloc && next_alloc){
        return bp;
    }
    //case 1 - just front
        //update now header, next footer
        //size check
    else if(prev_alloc && !next_alloc){
        now_size += GET_SIZE(HDRP(NEXT_BLEK(bp)));
        PUT(HDRP(bp), PACK(now_size, 0));
        PUT(FTRP(bp), PACK(now_size, 0));
    }
    //case 2 - just back
    else if(!prev_alloc && next_alloc){
        now_size += GET_SIZE(HDRP(PREV_BLEK(bp)));
        PUT(HDRP(PREV_BLEK(bp)), PACK(now_size, 0));
        PUT(FTRP(bp), PACK(now_size, 0));
        bp = PREV_BLEK(bp);
    }
    //case 3 - nothing
    else{
        now_size += GET_SIZE(HDRP(PREV_BLEK(bp))) + (GET_SIZE(NEXT_BLEK(bp)));
        PUT(HDRP(PREV_BLEK(bp)), PACK(now_size, 0));
        PUT(FTRP(NEXT_BLEK(bp)), PACK(now_size, 0));
        bp = PREV_BLEK(bp);
    }
    return bp;
}

//implicit first-fit 방식으로 구현
static void *find_fit(size_t asize){
    //first start point address
    //bp size 존재 시 탐색 유지
    // not alloc 상태 check, 
    //적절한 size 없을 경우 할당 불가 전달
    void *bp = mem_heap_lo + 2*WSIZE;
    while(GET_SIZE(HDRP(bp)) > 0){
        if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize){
            return bp;
        }
        bp = NEXT_BLEK(bp);
    }
    return NULL;
}

int mm_init(void)
{

    char *heap_list_ptr;
    //create prologue, epilogue and first block header, footer
    if((heap_list_ptr = mem_sbrk(4 * WSIZE)) == (void*)-1)return -1;
    PUT(heap_list_ptr, 0);
    PUT(heap_list_ptr + 1*WSIZE, PACK(DSIZE, 1));
    PUT(heap_list_ptr + 2*WSIZE, PACK(DSIZE, 1));
    PUT(heap_list_ptr + 3*WSIZE, PACK(0, 1));

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

//size, asize, 
//사이즈 조정
//공간 확장 및 재할당
//find_fit 메소드 필요!!!
//존재하지 않을 경우 힙 확장 로직
void *mm_malloc2(size_t size){
    size_t asize;
    size_t extendsize;
    
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}















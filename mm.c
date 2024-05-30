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
#define GET_ALLOC(p) (GET(p) & (0x1))
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLEK(bp) ((char*)(bp) + GET_SIZE((char*)(bp) - WSIZE))
#define PREV_BLEK(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

#define GET_SUCC(bp) (*((void**)(char*)(bp) + WSIZE))
#define GET_PRED(bp) (*(void**)(bp))

/* 
 * mm_init - initialize the malloc package.
 */

static void *extend_heap(size_t input_size);
static void *coalesce(void* bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void *find_next_fit(size_t asize);
static void *save_next_ptr;

static void* free_list;
static void split_free_block(void *bp);
static void add_free_block(void *bp);   //LIFO way


int mm_init(void)
{
    char *heap_list_ptr;
    //create prologue, epilogue and first blonck header, footer
    if((heap_list_ptr = mem_sbrk(4 * WSIZE)) == (void*)-1)return -1;
    PUT(heap_list_ptr, 0);
    PUT(heap_list_ptr + 1*WSIZE, PACK(DSIZE, 1));
    PUT(heap_list_ptr + 2*WSIZE, PACK(DSIZE, 1));
    PUT(heap_list_ptr + 3*WSIZE, PACK(0, 1));

    save_next_ptr = heap_list_ptr + 2*WSIZE;

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)return -1;
    return 0;
}



// size, asize, 
// 사이즈 조정
// 공간 확장 및 재할당
// find_fit 메소드 필요!!!
// 존재하지 않을 경우 힙 확장 로직
void *mm_malloc(size_t size){
    size_t asize;
    size_t extendsize;
    char* bp;

    if(size == 0){
        return NULL;
    }

    if(size <= DSIZE){
        asize = DSIZE * 2;
    }
    else{
        asize = DSIZE * ((size + DSIZE + DSIZE - 1)/DSIZE);
    }

    if((bp = find_next_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);

    if((bp = extend_heap((extendsize)/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}



//ptr == null -> just mm_malloc to new ptr
//size error mm_free 
// mm_malloc -> 
//copy data -> data size 문제
//현재 size가 copysize 보다 작을 경우 잘린 상태로 담는 방식으로 한다.




void *mm_realloc(void *ptr, size_t size){
    if(ptr == NULL){
        return mm_malloc(size);
    }
    if(size <= 0){
        mm_free(ptr);
        return;
    }
    void* new_ptr = mm_malloc(size);
    if(new_ptr == NULL)
        return NULL;

    size_t copySize = GET_SIZE(HDRP(ptr)) - DSIZE;
    if(copySize > size)
        copySize = size;
    
    memcpy(new_ptr, ptr, copySize);
    mm_free(ptr);

    return new_ptr;

}





static void split_free_block(void *bp){
    
}


static void *extend_heap(size_t input_size){
    char *bp;
    //사이즈 조정 - pedding 해야 하기 때문에 필수임
    size_t revise_size = input_size % 2 ? (input_size + 1)*WSIZE : (input_size)*WSIZE;
    if((long)(bp = mem_sbrk(revise_size)) == -1)return NULL;
    //extend 시 메모리 공간 새로 생기니까 header, footer setting 필수
    PUT(HDRP(bp), PACK(revise_size, 0));
    PUT(FTRP(bp), PACK(revise_size, 0));
    //epilogue block 초기화 필수인지?
    PUT(HDRP(NEXT_BLEK(bp)), PACK(0, 1));
    return coalesce(bp);
}

static void *coalesce(void * bp){
    //this method use when erase bp block -> but why use it in extend_heap???
    // size_t prev_alloc = GET_ALLOC(PREV_BLEK(HDRP(bp)));
    // size_t next_alloc = GET_ALLOC(NEXT_BLEK(HDRP(bp)));
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLEK(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLEK(bp)));
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
    void *bp = mem_heap_lo() + 2*WSIZE;
    while(GET_SIZE(HDRP(bp)) > 0){
        if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize){
            return bp;
        }
        bp = NEXT_BLEK(bp);
    }
    return NULL;
}

static void *find_next_fit(size_t asize){
    void * bp = save_next_ptr;
    //!!주의 사항 
    //save ptr부터 탐색 시작해서 ptr 끝까지
    //null로 구현해도 되겠지만 size?? epilogue 문제 때문에 size를 기준으로 한다.
    for(;(GET_SIZE(HDRP(bp)) > 0); bp = NEXT_BLEK(bp)){
        if((!GET_ALLOC(HDRP(bp)))&&(GET_SIZE(HDRP(bp)) >= asize)){
            save_next_ptr = bp;
            return bp;
        }
    }
    //초기화 후 save_ptr 이전까지
    for(bp = mem_heap_lo() + 2*WSIZE;(bp != save_next_ptr); bp = NEXT_BLEK(bp)){
        if((!GET_ALLOC(HDRP(bp)))&&(GET_SIZE(HDRP(bp)) >= asize)){
            save_next_ptr = bp;
            return bp;
        }
    }
    return NULL;
}



//할당하고자 하는 위치에 setting
//case 2가지 -> 현위치 적합시 바로 할당 
//-> 현위치보다 사이즈 크면 나눠서 블록 구분
//그 외의 조건은 find_fit에서 알아서 찾는다.


static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    if((csize - asize) >= 2*DSIZE){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        PUT(HDRP(NEXT_BLEK(bp)), PACK(csize - asize, 0));
        PUT(FTRP(NEXT_BLEK(bp)), PACK(csize - asize, 0));
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}









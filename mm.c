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

//static void *find_next_fit(size_t asize);
//static void *save_next_ptr;

static void* free_list;
static void split_free_block(void *bp);
static void add_free_block(void *bp);   //LIFO way
//static void add_free_block_address_info(void *bp);//address info first

#define DEBUG

#ifdef DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#else
#define dbg_printf(...)
#endif


/********************************************************************** */
/*초기 구현할 때 메서드 구분, msbrk 함수 가져와서 사용하고
기존 implicit의 경우에는 4B 사용했는데
여기서는 pred, succ 추가해서 8B로 사용한다. 
*/

int mm_init(void){
    /*초기 할당 시 
    prologue(header, footer), 
    header, pred, succ, footer
    epilogue(header, footer)
    이렇게 8block 할당한다.
    */
    dbg_printf("\nstart init %p\n", free_list); //teset
    //if(free_list != NULL)return 0;  //시작이 아닐 경우 추가
    
    if((free_list = mem_sbrk(WSIZE*8)) == (void*)-1)return -1;
    //dbg_printf("mem_start_brk: %p \t mem_brk: %p \tmem_size: %d\n", mem_heap_lo(), mem_heap_hi(), mem_heapsize);

    
    PUT(free_list, 0);  //padding
    
    PUT(free_list+WSIZE, PACK(DSIZE, 1));   //prologue header
    PUT(free_list+2*WSIZE, PACK(DSIZE, 1)); //prologue footer

    PUT(free_list+3*WSIZE, PACK(2*DSIZE, 0));   //header
    PUT(free_list+4*WSIZE, NULL);               //pred
    PUT(free_list+5*WSIZE, NULL);               //succ
    PUT(free_list+6*WSIZE, PACK(2*DSIZE, 0));   //footer

    PUT(free_list+7*WSIZE, PACK(0, 1)); //epilogue

    free_list += 4*WSIZE;

    //extendheap은 왜 사용하는 거지?
    if(extend_heap(CHUNKSIZE/WSIZE) == (void*)-1)return -1;
    //dbg_printf("test first block header and footer address %p \t %p\n", HDRP(free_list), FTRP(free_list));
    //dbg_printf("compare address dist and size %d %d\n", (FTRP(free_list) - HDRP(free_list)), 8*WSIZE);
    //dbg_printf("test epilogue and next block header %p\t%p\n", free_list + 3*WSIZE,  HDRP(NEXT_BLEK(free_list)));
    // dbg_printf("max heap address: %p\n", mem_heap_hi()+1);
    // dbg_printf("epilogue address: %p\n", free_list + 3*WSIZE);
    // dbg_printf("header of next blk address: %p\n", HDRP(NEXT_BLEK(free_list)));
    
    dbg_printf("end init %p\n", free_list); //test

    return 0;
}

void *mm_malloc(size_t size){   
    dbg_printf("\n\nmalloc start! %zu\n", size);                      //test
    /************************testcode************************/
    dbg_printf("find free address!: ");
    void * tmp = free_list;
    while(tmp != NULL){
        dbg_printf("%p, %zu\t", tmp, GET_SIZE(HDRP(tmp)));
        tmp = GET_SUCC(tmp);
    }
    dbg_printf("\n");
    /************************testcode************************/

    void *bp;
    /*입력 받은 size를 변형한다.
    DSIZE 규격으로 입력받지 않았을 수 있기 때문*/
    size_t csize;
    if(size == 0)   return NULL;

    if(size <= 2*DSIZE){
        csize = 2*DSIZE;
    }
    else{
        csize = ((size + DSIZE + DSIZE -1)/DSIZE)*DSIZE;
    }

    if((bp = find_fit(csize))!=NULL){
        place(bp, csize);
        dbg_printf("malloc finish!\n");                     //test
        return bp;
    }

    size_t extend_size = MAX(CHUNKSIZE, csize);
    if((bp = extend_heap(extend_size/WSIZE)) == NULL)return NULL;
    place(bp, csize);
    dbg_printf("malloc finish!\n");                     //test
    return bp;
}



void *mm_realloc(void* ptr, size_t size){
    dbg_printf("\n\nrealloc start! %p\t%zu\n", ptr, size);                      //test
    if(ptr == NULL)return mm_malloc(size);
    if(size <= 0){
        mm_free(ptr);
        return;
    }
    void *new_ptr = mm_malloc(size);
    if(new_ptr == NULL)return NULL;

    size_t copy_size = GET_SIZE(HDRP(ptr)) - DSIZE;
    if(copy_size > size)
        copy_size = size;
    memcpy(new_ptr, ptr, copy_size);
    mm_free(ptr);
    dbg_printf("realloc finish! \n");                      //test
    return new_ptr;
}


void mm_free(void *ptr){
    dbg_printf("\n\nfree start! %p\n", ptr);                      //test
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
    /************************testcode************************/
    dbg_printf("find free address!: ");
    void * tmp = free_list;
    while(tmp != NULL){
        dbg_printf("%p, %zu\t", tmp, GET_SIZE(HDRP(tmp)));
        tmp = GET_SUCC(tmp);
    }
    /************************testcode************************/
    dbg_printf("\nfree finish!\n");                      //test
    return;
}

static void *find_fit(size_t size){
    void* bp = free_list;
    while(bp != NULL){
        //size 이상인 주소를 찾으면 반환하고 종료
        if(GET_SIZE(HDRP(bp)) >= size) return bp;
        bp = GET_SUCC(bp);
    }
    //없으면 null 반환
    return NULL;
}

static void place(void* bp, size_t size){
    /*할당하기 위해서 현재 주소를 freelist에서 가져온다.*/
    split_free_block(bp);
    size_t csize = GET_SIZE(HDRP(bp));
    size_t rsize = csize - size;
    if(rsize >= 2*DSIZE){
        //dbg_printf("too big!\n");       //test
        //dbg_printf("\t %p\t%zu\n", bp, GET_SIZE(HDRP(bp))); //test
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));

        PUT(HDRP(NEXT_BLEK(bp)), PACK(rsize, 0));
        PUT(FTRP(NEXT_BLEK(bp)), PACK(rsize, 0));
        //dbg_printf("too big!%zu\t%p\t%p\n", GET_SIZE(HDRP(bp)), bp, NEXT_BLEK(bp));      //test
        //dbg_printf("after split %p \t %zu", bp, GET_SIZE(HDRP(bp)));
        //dbg_printf("\t%p\t%zu\n", NEXT_BLEK(bp), GET_SIZE(HDRP(NEXT_BLEK(bp))));
        add_free_block(NEXT_BLEK(bp));
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    return;
}

static void *coalesce(void* bp){
    dbg_printf("coalescing!!! %p \t %zu\n", bp, GET_SIZE(HDRP(bp)));//test
    size_t size = GET_SIZE(HDRP(bp));
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLEK(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLEK(bp)));

    if(prev_alloc && next_alloc){
        add_free_block(bp);
        return bp;
    }
    else if(prev_alloc && !next_alloc){
        dbg_printf("\tcase 2\t %p \t size: %zu\n", NEXT_BLEK(bp), GET_SIZE(HDRP(NEXT_BLEK(bp))));
        split_free_block(NEXT_BLEK(bp));
        size += GET_SIZE(HDRP(NEXT_BLEK(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if(!prev_alloc && next_alloc){
        dbg_printf("\tcase 3\t %p \t size: %zu\n", PREV_BLEK(bp), GET_SIZE(HDRP(PREV_BLEK(bp))));
        //test
        split_free_block(PREV_BLEK(bp));
        size += GET_SIZE(HDRP(PREV_BLEK(bp)));
        PUT(HDRP(PREV_BLEK(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLEK(bp);
    }
    else{
        dbg_printf("\tcase 4\t prev: %p \t size: %zu\n", PREV_BLEK(bp), GET_SIZE(HDRP(PREV_BLEK(bp))));
        dbg_printf("\tcase 4\t next: %p \t size: %zu\n", NEXT_BLEK(bp), GET_SIZE(HDRP(NEXT_BLEK(bp))));
        
        split_free_block(NEXT_BLEK(bp));
        split_free_block(PREV_BLEK(bp));
        //next footer?
        size += (GET_SIZE(HDRP(PREV_BLEK(bp))) + GET_SIZE(HDRP(NEXT_BLEK(bp))));
        PUT(HDRP(PREV_BLEK(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLEK(bp)), PACK(size, 0));
        bp = PREV_BLEK(bp);
    }
    dbg_printf("\tafter coalescing %p size: %zu\n", bp, GET_SIZE(HDRP(bp)));
    add_free_block(bp);
    dbg_printf("coalescing complete!!!\n");
    return bp;
}

static void *extend_heap(size_t block_size){
    /*size를 입력 받고 조정하는게 좋지 않나
        굳이 Wsize로 나눠서 받아야 하나?*/
    /*그래도 기왕 코드 그렇게 돼있으니까 
        구현하고 나중에 수정하는 방향으로*/
    
    size_t resize = block_size%2 ? (block_size+1)*WSIZE : block_size*WSIZE;
    void *bp;
    if((bp = mem_sbrk(resize))==(void*)-1)return NULL;
    //dbg_printf("\nthis is start of extend heap ptr %p\n", bp);
    //extend block header, footer 할당
    PUT(HDRP(bp), PACK(resize, 0));
    PUT(FTRP(bp), PACK(resize, 0));
    //epilogue 할당
    PUT(NEXT_BLEK(HDRP(bp)), PACK(0, 1));
    //add_free_block(bp);
    dbg_printf("extend success!\n");      //test
    return coalesce(bp);
}

static void split_free_block(void *bp){
    void *bp_nxt = GET_SUCC(bp);
    void *bp_prv = GET_PRED(bp);
    if(bp_nxt && bp_prv){
        GET_PRED(bp_nxt) = bp_prv;
        GET_SUCC(bp_prv) = bp_nxt;
    }
    else if(!bp_nxt && bp_prv){
        GET_SUCC(bp_prv) = NULL;
    }
    //succ 주소를 free_list로 설정
    else if(bp_nxt && !bp_prv){
        GET_PRED(bp_nxt) = NULL;
        free_list = bp_nxt;
    }
    else{
        free_list = NULL;
    }
    //해당 주소 succ, pred null로 설정
    GET_SUCC(bp) = NULL;
    GET_PRED(bp) = NULL;
    return;
}



static void add_free_block(void *bp){
    //input bp에 null 여부만 체크
    if(bp == NULL)return;
    //기존 first block 과 pred, succ 관계 
    GET_SUCC(bp) = free_list;
    //free list가 null 인 경우 체크
    if(free_list != NULL)
        GET_PRED(free_list) = bp;
    GET_PRED(bp) = NULL;
    free_list = bp;
    //coalesce(bp);
    dbg_printf("addfreelist success %p\n", free_list);  //test
    return;
}



/********************************************************************** */


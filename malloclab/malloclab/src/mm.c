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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define MAX(x,y) ((x)>(y)? (x):(y))
#define PACK(size,alloc) ((size)|(alloc))
#define GET(p) (*(unsigned int *)(p))
#define PUT(p,val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
#define PUT_ADDR(p, val) (*(int *)(p) = (int)(long)(val))
#define NEXT_WORD(p) ((unsigned int*)((char *)(p)+WSIZE))
#define N_SEGLIST 11
#define MIN_BLK_SIZE (2*DSIZE)
#define NEXT_FREE_BLKP(bp) (GET((bp)))
#define PREV_FREE_BLKP(bp) (GET((char*)(bp)+WSIZE))
#define isDebug 0
/* hepler function */
static int get_seg_level(size_t size);
static void* get_freeblk_root(size_t size);
static void insert_free_block(void *bp);
static void delete_free_block(void *bp);
static void* find_fit(size_t size);
static void* coalesce(void *bp);
static void* extend_heap(size_t words);
static void place(void* bp, size_t asize);
/* debugging helper function */
static void printblocks();
static void printseglist();
static int mm_check();

/* global variables */
char* heap_listp;
char* blk_start;

/* 
 * mm_init - initialize the heap. 
 */


int mm_init(void)
{
    if((heap_listp=mem_sbrk(3*DSIZE+N_SEGLIST*DSIZE))==(void*)-1) return -1;
    PUT(heap_listp,0); /* alignment padding */
    for(int i=0;i<N_SEGLIST;i++){
        PUT_ADDR(heap_listp+ (i+1)*DSIZE,NULL);//initialize root of seglist as NULL
    }
    PUT(heap_listp+DSIZE+N_SEGLIST*DSIZE+WSIZE,PACK(DSIZE,1));//set prologue header
    PUT(heap_listp+2*DSIZE+N_SEGLIST*DSIZE,PACK(DSIZE,1));//set prologue header
    PUT(heap_listp+2*DSIZE+N_SEGLIST*DSIZE+WSIZE,PACK(0,1));//set epliogue header
    if(extend_heap(CHUNKSIZE/WSIZE)==NULL) return -1;
    blk_start = heap_listp + (N_SEGLIST+3)*DSIZE;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by finding free blocks in seggregated list. 
 *             If there is no free block that fits, extend heap. 
 */
void *mm_malloc(size_t size){
    size_t asize; /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;
    //if(isDebug && mm_check()<0) printf("error\n");
    //if(isDebug) printblocks();
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);

        //if(isDebug) printf("malloc %p: %d\n",bp,asize);
        return bp;
    }
    extendsize = MAX(asize,CHUNKSIZE);
    //extendsize = asize;
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    //if(isDebug) printf("malloc %p: %d\n",bp,asize);
    return bp;
}


/*
 * mm_free - Free a block and put into seggregated list.
 */
void mm_free(void *ptr){
    //if(isDebug) printblocks();
    //if(isDebug && mm_check()<0) printf("error\n");

    if(!ptr || !GET_ALLOC(HDRP(ptr))){ // If it's illegal or not allocated block, do nothing. 
        if(!ptr){
            printf("illegal free\n");
            return;
        }
        printf("free not allocated block\n");
        return;
    }
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    insert_free_block(ptr);
    coalesce(ptr);
    //if(isDebug) printf("free %p: %d\n",ptr,size);
}

/*
 * mm_realloc - Do realloc. 
                Case 1. Original block size is bigger than requested size,
 *                      free remaining blocks.
 *              Case 2. Original block size is smaller
 *                  Case 2.1. Look for next block.
 *                             If it is free and large enough, use it.
 *                  Case 2.2. Look for previous block.
 *                            If it is free and large enough, use it.
 *                  Case 2.3. Look for both next and previous block.
 *                            If they are free and large enough, use it.
 *                  Case 2.4. If it is none of the above case,
 *                            allocate new block and free original block.                     
 */
void *mm_realloc(void *ptr, size_t size)
{

    void *oldptr = ptr;
    void *newptr;
    size_t copySize,asize;
    //if(isDebug && mm_check()<0) printf("error\n");
    //if(isDebug){
    //    printblocks();
    //    printf("realloc %p: %d->%d\n",ptr,GET_SIZE(HDRP(ptr)),size);
    //}
    if(size==0){//If size is 0, return NULL
        mm_free(oldptr);
        return NULL;
    }
    
    if(oldptr==NULL){//If oldptr is NULL, do malloc
        return mm_malloc(size);
    }
    copySize = GET_SIZE(HDRP(ptr));

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    if (copySize>=asize){// Case 1. If original block is bigger than requested size, we can reuse it
        if(copySize - asize<MIN_BLK_SIZE){
	    //Case 1.1 If remaining block size is not enough to free, do nothing
            return oldptr;
        }
        //Case 1.2 If remaining block is large enough, then free remaining block. 
        PUT(HDRP(oldptr),PACK(asize,1));
        PUT(FTRP(oldptr),PACK(asize,1));
        void* new_freed = NEXT_BLKP(oldptr);
        PUT(HDRP(new_freed),PACK(copySize - asize,0));
        PUT(FTRP(new_freed),PACK(copySize - asize,0));
        insert_free_block(new_freed);
        coalesce(new_freed);
        return oldptr;
    }

    //Case 2. Original block is smaller than requested size.
    void* prevptr = PREV_BLKP(oldptr);
    void* nextptr = NEXT_BLKP(oldptr);

    if(nextptr!=NULL && !GET_ALLOC(HDRP(nextptr))){
        /*Case 2.1. If the next block is free and happen to be large enough,
         *          take some of them and add it to the original block. */
        size_t next_freesize = GET_SIZE(HDRP(nextptr));
        if(copySize+next_freesize-asize>=MIN_BLK_SIZE){
            /*Case 2.1.1. If remaining size of next block is large, free them*/
            delete_free_block(nextptr);
            PUT(HDRP(oldptr),PACK(asize,1));
            PUT(FTRP(oldptr),PACK(asize,1));
            void* new_freed = NEXT_BLKP(oldptr);
            PUT(HDRP(new_freed),PACK(copySize + next_freesize - asize,0));
            PUT(FTRP(new_freed),PACK(copySize + next_freesize - asize,0));
            insert_free_block(new_freed);
            coalesce(new_freed);
            return oldptr;
        }
        else if(copySize+next_freesize-asize>0){
            /*Case 2.1.2. If remaining size of next block is small,
             *            add whole block to the original block.*/
            delete_free_block(nextptr);
            PUT(HDRP(oldptr),PACK(copySize + next_freesize,1));
            PUT(FTRP(oldptr),PACK(copySize + next_freesize,1));
            return oldptr;
            
        }
    }



    if(prevptr!=NULL && !GET_ALLOC(HDRP(prevptr))){
        /*Case 2.2. If the previous block is free and happen to be large enough,
         *          move the original block to the previous block */
        size_t prev_freesize = GET_SIZE(HDRP(prevptr));
        if(copySize+prev_freesize-asize>=MIN_BLK_SIZE){
            /*Case 2.2.1. If remaining size of oribinal block is large, free them*/
            delete_free_block(prevptr);
            memmove(prevptr,oldptr,copySize-DSIZE>size?size:copySize-DSIZE);
            PUT(HDRP(prevptr),PACK(asize,1));
            PUT(FTRP(prevptr),PACK(asize,1));
            void* new_freed = NEXT_BLKP(prevptr);
            PUT(HDRP(new_freed),PACK(copySize + prev_freesize - asize,0));
            PUT(FTRP(new_freed),PACK(copySize + prev_freesize - asize,0));
            insert_free_block(new_freed);
            return prevptr;
        }
        else if(copySize+prev_freesize-asize>0){
            /*Case 2.2.2. If remaining size of next block is small,
             *            add whole block to the original block.*/
            delete_free_block(prevptr);
            memmove(prevptr,oldptr,copySize-DSIZE>size?size:copySize-DSIZE);
            PUT(HDRP(prevptr),PACK(copySize + prev_freesize,1));
            PUT(FTRP(prevptr),PACK(copySize + prev_freesize,1));
            return prevptr;

        }
    }
    if(prevptr!=NULL && !GET_ALLOC(HDRP(prevptr)) && nextptr!=NULL && !GET_ALLOC(HDRP(nextptr))){
        /*Case 2.3. If the previous block+original block+next block is free
         *          and happen to be large enough,
         *          move the original block. */
        size_t prev_freesize = GET_SIZE(HDRP(prevptr));
        size_t next_freesize = GET_SIZE(HDRP(nextptr));
        if(copySize+prev_freesize+next_freesize-asize>=MIN_BLK_SIZE){
            /*Case 2.3.1. If remaining size is large, free them*/
            delete_free_block(prevptr);
            delete_free_block(nextptr);
            memmove(prevptr,oldptr,copySize-DSIZE>size?size:copySize-DSIZE);
            PUT(HDRP(prevptr),PACK(asize,1));
            PUT(FTRP(prevptr),PACK(asize,1));
            void* new_freed = NEXT_BLKP(prevptr);
            PUT(HDRP(new_freed),PACK(copySize + prev_freesize + next_freesize - asize,0));
            PUT(FTRP(new_freed),PACK(copySize + prev_freesize + next_freesize - asize,0));
            insert_free_block(new_freed);
            return prevptr;
        }
        else if(copySize+prev_freesize+next_freesize-asize>0){
            /*Case 2.3.2. If remaining size of next block is small,
             *            add whole block to the original block.*/
            delete_free_block(prevptr);
            delete_free_block(nextptr);
            memmove(prevptr,oldptr,copySize-DSIZE>size?size:copySize-DSIZE);
            PUT(HDRP(prevptr),PACK(copySize + prev_freesize,1));
            PUT(FTRP(prevptr),PACK(copySize + prev_freesize,1));
            return prevptr;

        }
    }

   
    //Case 2.4. Next block is not free or not large enough. Allocate a whole new block. 
    newptr = mm_malloc(size); 
    if (newptr == NULL) return NULL; // return NULL if malloc returns NULL
    memcpy(newptr, oldptr, copySize-DSIZE>size?size:copySize-DSIZE); // do memory copy
    mm_free(oldptr);

    return newptr;
}

/* helper function list*/

/* get_seg_level : For seggregated list, classify each size. 
 *                   If the size is in [2^i + 1,2^(i+1)] -> return i
 */
static int get_seg_level(size_t size){
    int n_block = size/DSIZE;
    if(n_block<=2) return 0;
    if(n_block<=4) return 1;
    if(n_block<=8) return 2;
    if(n_block<=16) return 3;
    if(n_block<=32) return 4;
    if(n_block<=64) return 5;
    if(n_block<=128) return 6;
    if(n_block<=256) return 7;
    if(n_block<=512) return 8;
    if(n_block<=1024) return 9;
//    if(n_block<=2048) return 0;
    return 10;
}

/* get_freeblk_root : Get the root of seggregated list of each size. */
static void* get_freeblk_root(size_t size){
    return heap_listp + DSIZE + get_seg_level(size)*DSIZE;
}

/* insert_free_block : Insert free block into seggregated list in ascending order. */
static void insert_free_block(void *bp){//insert into seglist
    size_t size = GET_SIZE(HDRP(bp));
    void* root = get_freeblk_root(size);
    if(GET(root) == NULL){//no node exists
        PUT_ADDR(root,bp);//root points to bp
        PUT_ADDR(NEXT_WORD(bp),root);//prev blkp of bp is root
        PUT_ADDR(bp,NULL);//next blkp of bp is NULL
        return;
    }
/*
    //in pointer order
    void* p = GET(root);
    void* prevp = root;
    while(p!=NULL && (unsigned int*)p < (unsigned int*)bp){
        prevp = p;
        p = NEXT_FREE_BLKP(p);
    }
*/

  //ascending order    
    void* p = GET(root);  
    void* prevp = root;
    while(p!=NULL && GET_SIZE(HDRP(p))<size){
        prevp = p;
        p = NEXT_FREE_BLKP(p);
    }

        
    // make (prevp -> p) to (prevp -> bp -> p)

    PUT_ADDR(prevp,bp); //next blkp of prevp is bp
    PUT_ADDR(NEXT_WORD(bp),prevp); // prev blkp of bp is prev
    PUT_ADDR(bp,p); // next blkp of bp is p
    if(p!=NULL){
        PUT_ADDR(NEXT_WORD(p),bp); // prev blkp of p is bp 
    }

/*  //FIFO
    PUT_ADDR(NEXT_WORD(p),bp); //prev pointer of p is bp
    PUT_ADDR(bp,p); //next pointer of bp is p
    PUT_ADDR(root,bp); //next pointer of root is bp
    PUT_ADDR(NEXT_WORD(bp),root);//prev blkp of bp is root   
*/
}

/* delete_free_block : Delete a block from seggregated list if the block is allocated. */
static void delete_free_block(void *bp){
    void* next_free_blkp = NEXT_FREE_BLKP(bp);
    void* prev_free_blkp = PREV_FREE_BLKP(bp);
    PUT_ADDR(prev_free_blkp,next_free_blkp);
    if(next_free_blkp!=NULL){
        PUT_ADDR(NEXT_WORD(next_free_blkp),prev_free_blkp);
    }
}

/* find_fit : Find a block from seggregated list that fits the size. */
static void *find_fit(size_t size){
    void* root = get_freeblk_root(size);
    void* p2;
    while(root<=heap_listp+DSIZE*(N_SEGLIST)){
        p2 = GET(root);
        while(p2!=NULL && GET_SIZE(HDRP(p2))<size){
            p2 = NEXT_FREE_BLKP(p2);
        }
        if(p2!=NULL) return p2;
        root = root+DSIZE;
    }
    return NULL;
}

/* coalesce : Coalesce the free block. */
static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    if (prev_alloc && next_alloc) { /* Case 1 */ /*do nothing*/
        return bp;
    }
    if (prev_alloc && !next_alloc) { /* Case 2 */
        delete_free_block(NEXT_BLKP(bp));
        delete_free_block(bp);
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        delete_free_block(PREV_BLKP(bp));
        delete_free_block(bp);
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else if(!prev_alloc && !next_alloc) { /* Case 4 */
        delete_free_block(PREV_BLKP(bp));
        delete_free_block(NEXT_BLKP(bp));
        delete_free_block(bp);
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert_free_block(bp);
    return bp;
}

/*extend_heap : Extend the heap. */
static void* extend_heap(size_t words){
    char *bp;
    size_t size;
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1){
        printf("in extend heap : mem_sbrk failed\n");
        return NULL;
    }
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
    insert_free_block(bp);
    /* Coalesce if the previous block was free */
    return coalesce(bp); // coalesce would fill in the next and prev block 
}

/* place : Split the free block and allocate the first part. */
static void place(void* bp, size_t asize){
    int is_realloc = GET_ALLOC(HDRP(bp));
    size_t start_size = GET_SIZE(HDRP(bp));
    size_t remaining_size = start_size - asize; //remaining size
    if(!is_realloc){
        delete_free_block(bp);
    }
    if(remaining_size>=MIN_BLK_SIZE){//If remaining size is big enough, free remaining blocks
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
        void *remaining_blkp = NEXT_BLKP(bp); //remaining free blkp
        PUT(HDRP(remaining_blkp),PACK(remaining_size,0));
        PUT(FTRP(remaining_blkp),PACK(remaining_size,0));
        insert_free_block(remaining_blkp);
        coalesce(remaining_blkp);
    }
    else{// if remainging size is too small to follow the alignment rule, just allocate all
        PUT(HDRP(bp),PACK(start_size,1));
        PUT(FTRP(bp),PACK(start_size,1));

    }

}


/* debugging helper function list */

/* Print all the blocks and information of the blocks(size, isAllocated) */ 
static void printblocks(){
    void* p = blk_start;
    int isAlloc;
    int count=0;
    while(GET(HDRP(p))!=PACK(0,1)){

        isAlloc = GET_ALLOC(HDRP(p));
        if(isAlloc){
            count++;
            printf("%p",p);
            printf("(allocated,%d) -> ",GET_SIZE(HDRP(p)));
        }
        else{
            printf("%p",p);
            printf("(free,%d) -> ",GET_SIZE(HDRP(p)));
        }
        p = NEXT_BLKP(p);
    }
    printf("NULL\n");
}

/* Print all the seggregated list */
static void printseglist(){
    void *p;

    for(int i=0;i<N_SEGLIST;i++){
        printf("seg list %d :",i);
        p = heap_listp + DSIZE + i*DSIZE; //root
        while(p!=NULL){
            printf("%p -> ",p);
            p = GET(p);
        }
        printf("NULL\n");
    }
}


/* mm_check : check heap consistency */
static int mm_check(){
    void* p = blk_start;
    int isErrorExists = 0;
    //until p is NULL(something went wrong) or meets epilogue header
    while(GET(HDRP(p))!=PACK(0,1)){
        if(!GET_ALLOC(HDRP(p))){
            //1. check if free block is in seglist
            void* q = get_freeblk_root(GET_SIZE(HDRP(p)));
            int isInSeglist = 0;
            //printf("search seglist ");
            while(q!=NULL){
                //printf("%p ",q);
                if(q==p){
                    isInSeglist = 1;
                    break;
                }
                q = NEXT_FREE_BLKP(q);
            }
            //printf("\n");
            if(!isInSeglist){
                printf("There exists a free block that is not in the seggregated list\n");
                printblocks();
                printseglist();
                isErrorExists = 1;
            }
            //2. check if there are two contiguous free blocks
            if(NEXT_BLKP(p)!=NULL && !GET_ALLOC(HDRP(NEXT_BLKP(p)))){
                printf("There exist two contiguous free blocks\n");
                isErrorExists = 1;
            }
        }
        //3. check if there are some bad block locations
        if(p==NULL){
            printf("There exist some overlaps or gaps between blocks\n");
            isErrorExists = 1;
        }
        p = NEXT_BLKP(p);
        //printf("%p",p);
    }
    //4. check if there is an allocated block in seggregated list
    for(int i=0;i<N_SEGLIST;i++){
        p = heap_listp + DSIZE + i*DSIZE; //root
        while(p!=NULL){
            if(GET_ALLOC(HDRP(p))){
                printf("There exists an allocated block in the seggregated list\n");
                isErrorExists = 1;
            }
            p = GET(p);
        }
    }
    if(isErrorExists){
        return -1;
    }
    return 0;

}







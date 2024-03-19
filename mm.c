/*
 * mm.c
 *
 * Name: Brook Azene, Ilesh Shrestha
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * Also, read the project PDF document carefully and in its entirety before beginning.
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16


// global heap pointer pointing to start address (fake/special 8 byte padding initially) 
size_t* heapPoint;
// Pointer to top of doubly  linked list, thrown into temp variables but never changed outside of free()
size_t* listTop;
int freeBlocks;


// static functions in place of macros to define constant terms needed in initialisation/size computation
static size_t headSize() {return (ALIGNMENT / 2);}
static size_t footSize() {return (ALIGNMENT / 2);}
static size_t minBlock() {return (headSize() + footSize() + ALIGNMENT);}


//static uint64_t MAX(uint64_t x, uint64_t y) { if (x>y){ return x;} return y;}

static size_t PACK(size_t size, size_t alloc) { return (size | alloc);}

static size_t GET(size_t* p) { return (*(size_t *) p);}

static size_t PUT(size_t* p, size_t val) {return (*p = val);}

static size_t GET_SIZE(void* p) {return (GET(p) & -2);}

static bool GET_ALLOC(void* p) { return (GET(p) & 0x1);}

static size_t* HDRP(void* p) { return (size_t*)(p - 1);}

static size_t* FTRP(void* p) { return (size_t*)((char*)p + *(HDRP(p)) - 8);}

static void* NEXT_BLKP(void* p) { return ((char*)p + *(HDRP(p)));}

//static void* NEXT_BLKP(void* p) { return ((size_t*)p + FTRP(p) + ALGIN);}

//static void* PREV_BLKP(void* p) { return ((size_t*)p - GET_SIZE((size_t*)p - (size_t) 2));}

//static size_t CHUNKSIZE() {return 1 << 12;}


// Prototype def. for helper functions inside malloc()
static size_t* place(size_t *bp, size_t asize);
static size_t* find_fit(size_t size);
static size_t* extend_heap(size_t size);
static void coalesce (size_t* bp);
void removeNode(size_t* p);
void add(size_t* p);


/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}



/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
    heapPoint = 0;
    // Empty heap initialized with min size 48 bytes = head, prev, next, footer, min 16 free space
    heapPoint = mem_sbrk(minBlock() + ALIGNMENT);
    printf("original allocation: %p\n", heapPoint);
    printf("lololo1(): %p\n", mem_heap_lo());

    // Checks for error message from mem_sbrk()
    if (heapPoint == ((void *)-1)) 
    {
        return false;   
    }

    // Actually create the initial two words = fake footer, epilogue header
    // Created one single min block of 32 bytes and set prev, next to zero.

    *heapPoint = 0 | 1;
    heapPoint = heapPoint + 1;
    *heapPoint = (size_t)32 | 2; // header 
    heapPoint = heapPoint + 1;

    *heapPoint = 0; //prev
    heapPoint = heapPoint + 1;
    *heapPoint = 0; // next
    heapPoint = heapPoint + 1;
    *heapPoint = (size_t)32; // footer
    heapPoint = heapPoint + 1;
    *heapPoint = 0 | 1; // epilogue

    // set head node tracker to header of free block
    listTop = (size_t*) mem_heap_lo() + 1;
    printf("lololo(): %p\n", mem_heap_lo);
    printf("top: %p\n", listTop);

    freeBlocks =1;

    return true;
}

/*
 * malloc
 */
void* malloc(size_t size)
{   
  //  printf("size is %ld\n", size);

    size_t asize;
    size_t* blockp;

    if (size == 0){
        return NULL;
    }
      
    asize = align(size + 8);
    //asize = align(size + 16);
    if (asize < 32)
    {
       asize = 32;
    }
    
    printf("asize is %ld\n", asize);
    // check to see if there is a block that can fit
    blockp = find_fit(asize);
    printf("blockp is %p\n", blockp);
    printf("lo: %p\n", mem_heap_lo());
 
    if((blockp)!= 0)
    { 
   //    printf("blockp is %p\n", blockp);
       blockp = place(blockp, asize);
       
       size_t *temp = (size_t *) (((char *)blockp + asize)); // move to next block
       *temp = (*temp | 2); // set next block 2nd last bit to alloc
   return blockp + 1;
    }

 //   printf("Extending...\n");
    //size_t *store = mem_heap_hi();
    blockp = extend_heap(asize);
 //   printf("blockp is %p\n", blockp);

    if ((blockp) == NULL){
    	return NULL;
    }
    
    blockp = place(blockp, asize);
   // size_t tempSize = (size_t) (*blockpp & ~3); // get size of current block
    size_t *temp = (size_t *) (((char *)blockp + asize)); // move to next block
    *temp = (*temp | 3); // set next block 2nd last bit to alloc
    
    return blockp + 1;

}



//helper functions
// bp is the head of the free block
static size_t* place(size_t *bp, size_t asize){
 //   printf("Starting Place... \n");
   
    size_t csize = ((*bp) & ~3);
    size_t storeBit = *bp & 2;
    size_t ksize = csize - asize;
 //   printf("ksize is %ld\n", ksize);
 //   printf("csize is %ld\n", csize);
     
    // splitting a found block that is too big
    if (csize >= (size_t)32 + asize) {

	// we place the allocated block in the later half of the free block
        // took out the stored bit if else cuz we can just | ksize with stored bit = 0 or 1
        //if (storeBit == 0)
        //{
        
   //     printf("%d\n", 1);
        
        
        
        
        *bp = (size_t) (ksize | storeBit); 

     //   printf("bp is %p\n", bp);
     //   printf("*bp is %lu\n", *bp);
     //   printf("asize is %lu\n", asize);
        
        // bp points to head of free block, add csize - asize - 8 
        //to get to where footer should start 
        bp = (size_t*)((char*)bp + ksize - 8);
        
        *bp = (size_t) (ksize);
     //   printf("PUT %ld in %p \n", ksize, bp);
        // move to allocated block area
        bp = bp + 1; // bp is size_t right now
        size_t* temp = bp;
    	

    	// the second to last bit of allocated block should still be 0 b/c
    	// previous block is free. dont need to do anything other than put size 
    	// into the heaeder
    	
        *bp = (size_t)(asize | 1);
       // bp = bp + asize-8;
        
        //PUT(bp, PACK(asize,1));
        bp = (size_t *) ((char*)bp + asize);
        *bp = (*bp | 2);
    //    printf("PUT %ld in %p \n", asize, bp);


	return (size_t*)(temp);
    	
    }
    else
    {
    	// putting size into header of block
    	// not touching second to last bit b/c it should still be the same
        //PUT(bp, PACK(csize, 1));
     //   printf("Went into ELSE\n");
        *bp = ((asize | storeBit) | 1);
        // since there is no free block at all we need to remove it from list/
//	printf("PUT %ld in %p \n", asize, bp);
        removeNode(bp);

        
        return (size_t*)(bp);
        
    
    }

}

static size_t* find_fit(size_t size){

    //if (listTop == 0){
    
    //return extend_heap(size);
    
    
    //}
    // find_fit for the explicit
  //  printf("Starting Find Fit... \n");
    size_t* temp_next_block = listTop; // first scenario is first block
    size_t* next_pointer = ((size_t*)listTop + 2); // & of next pointer
    size_t temp_size = (size_t)(*listTop & ~3); // size of the first free block
    // notsure whether to put 0 or NULL yet gotta test
    
    while (*next_pointer != 0){
  //      printf("tempblock is %ln\n", temp_next_block);
        printf("here:%p\n", temp_next_block);
        if (size <= temp_size){
    //        printf("block found is %p\n", temp_next_block);
            return temp_next_block; //points to the header of the next free block

        }
        temp_next_block = (size_t*) *next_pointer; // next_pointer -> head of next block
        next_pointer = ((size_t*)temp_next_block + 2); // next of next_pointer
        temp_size = ((*temp_next_block) & ~3); // get size of next block

    }    

    return 0;

}

static size_t* extend_heap(size_t size){

    // temporary pointers to help with adjusting
   // printf("Extending... \n");
    size_t* blockp;
    char* temp; 
    temp = mem_heap_hi()-7; // memheaphi gives start of last bit -7 is start of epi
    //size_t storedBit = (size & 2);
    size_t epi_second = (*temp & 2);
    blockp = mem_sbrk(size); // add size to heap

    // memsbrk fails check
    if (blockp == (void*) -1){
    	return NULL;
    
    }
    
    *((size_t*)temp) = (size | epi_second); // epi becomes head -> free for now
    char* new_end = mem_heap_hi();
    new_end = new_end - 7; // gets to epi 
    
   // printf("new_end = %p\n", new_end); //epi
    
    *((size_t*)new_end) = (size_t)1; // set epi to alloc with size 8
    *((size_t*)new_end -1) = size; // set footer = size

    add((size_t*)temp);
    return blockp-1;

}

// bp is the header off middle block
static void coalesce (size_t* bp){
// bp points to header of free blockd
// bp second bit = prev alloc or not
// bp + size is header of next
    size_t size = ((*bp) & ~3);
 //   printf("size for coalesce = %lu\n", size);
// case 1 before is alloc, after is alloc

//for (int i = 1; i <= freeBlocks; i++){
  //  size_t *temp = listTop; 
  //  printf("head is %p \n", temp);
  //  printf("next is %ld\n", *(temp + 2));
  //  temp = (size_t*)(*(temp + 2));    
    

//}
    if((*bp & 2) && (*((size_t*)bp + size/8) & 0x1))
    {
         add(bp); //add bp -> make bp the new head

   } 

// case 2 before is not alloc, after is alloc
    else if (!(*bp & 2) && (*((size_t*)bp + size/8) & 1))
    {
    	printf("remove start 2\n");
        //removeNode(bp); // remove middle from list
	//printf("remove end 2\n");
	 
        size_t prevSize = (*(bp - 1) & ~3);

        removeNode((size_t *)((size_t *)bp - prevSize/8)); // remove left from list
        size_t second = (*((size_t *)bp - prevSize/8) & 2);

        // store second bit for new head
        size_t total = size + prevSize;
        size_t *newHSIZE = (size_t *) ((char *)bp - prevSize); // new head
        size_t *newFSIZE = (size_t *) ((char *)bp + size - 8);

        *newHSIZE = ((total | second) & ~1);  
        *newFSIZE = (total & ~1);

        add((size_t *)((char *)bp - prevSize));
	printf("remove end 2\n");

    }
// case 3 before is alloc, after is no alloc
    else if ((*bp & 2) && !(*((size_t*)bp + size/8) & 1))
    {

        // remove bp, right
        // compute total
        // add to header in middle, add to footer in right
        // add(middle head)
	printf("remove start 3\n");
        //removeNode(bp); // remove middle from list
	 size_t second = (*bp & 2);
        size_t nextSize = (*(bp + size/8) & ~3);

        removeNode((size_t *)((char *)bp + size)); // remove right from list

        // store second bit for new head
        size_t total = size + nextSize;
        size_t *newHSIZE = (size_t *) ((char *)bp); // new head
        size_t *newFSIZE = (size_t *) ((char *)bp + size + nextSize - 8);

        *newHSIZE = ((total | second) & ~1);  
        *newFSIZE = (total & ~1);
	
        add(bp);
	printf("remove end 3\n");

    }

// case 4 neither before or after is alloc
    else if (!(*bp & 2) && !(*((size_t*)bp + size/8) & 1))
    {
        
	printf("remove start 4\n");
        //removeNode(bp); // remove middle from list
	//printf("remove end 4\n");
        size_t prevSize = (*(bp - 1) & ~3);
        printf("remove left \n");
        size_t second = ((*(size_t *)bp - prevSize/8) & 2);
        removeNode((size_t *)((char *)bp - prevSize)); // remove left from list
	printf("remove right 4\n");
        size_t nextSize = (*(bp + size/8) & ~3);
        removeNode((size_t *)((char *)bp + size)); // remove right from list

        size_t total = size + prevSize + nextSize;
        size_t *newHSIZE = (size_t *) ((char *)bp - prevSize); // new head
        size_t *newFSIZE = (size_t *) ((char *)bp + size + nextSize - 8);

        *newHSIZE = ((total | second )& ~1);  
        *newFSIZE = (total & ~1);

        add((size_t *)((char *)bp - prevSize));
	printf("remove end 4\n");

        // add to header in left, add to footer in right
        // add(left head) 

    }
    printf("Sucessfully coalesced \n");


}

// p is the head of free block
void removeNode(size_t* p){
printf("Starting Remove... \n");
    //printf("num free blocks = %d\n", freeBlocks);
    printf("p is %p\n", p);
    size_t* nextp = p + 2;
    size_t* prevp = p + 1;
    
    if (p == listTop)
    {
        listTop = (size_t *) (*(nextp)); // dereference next pointer to get next head
        

    }
    
   
    printf("nextp is at %p\n", nextp);
    printf("prevp is at %p\n", prevp);
    //printf("nextp val is %ld\n", *nextp);
    //printf("prevp val is %ld\n", *prevp);
    
    // change next if next node exists
    if (*nextp != 0)
    {
        size_t *temp = nextp;
        printf("what is this: %p\n", temp);
        temp = temp + 1; // previous of next block
        *temp = *prevp;  // p next previous is now p previous
        
    }
    // change prev if prev node exists
    if (*prevp != 0)
    {
        size_t *prevBlock = (size_t *) (*prevp);  // p prevvious next is now p next
        prevBlock = prevBlock + 2; // next of prev block
        *prevBlock = *nextp;
        printf("lo: %p\n",mem_heap_lo());

    }
    freeBlocks = freeBlocks - 1;
    
    printf("removed node bp \n");
    printf("num free blocks = %d\n", freeBlocks);

}




// p is the head of block we are adding
void add(size_t* p)

{
printf("Starting Add... \n");
    //new head of list
    // set prev to 0
    printf("p is %p\n", p);
    printf("listop = %p\n", listTop);
    //printf ("listtop val = %ld\n", *listTop);
  //  printf("num free blocks = %d\n", freeBlocks);
    size_t* prev = p + 1;
    *prev = (size_t) 0;
    //set next to old head
    size_t *next = p + 2;
    *next = (size_t) listTop;

    //change old head prev
    if (listTop != 0){
    
    size_t* temp = listTop + 1;
    //temp = (size_t*)temp + 1;
    *temp = (size_t) p;
    //set new head to new block
    }
    
    listTop = p;
    
    size_t size = *p & ~3;
    printf("size = %ld\n", size);
    p = (size_t*)((char*)p + size);
    //printf("pointer = %p\n", p);
    printf("new epilogue = %p\n", mem_heap_hi()-7);
    *p = (*p & ~2);
    freeBlocks = freeBlocks + 1;
    printf("A free block was added to the list\n");
    printf("num free blocks = %d\n", freeBlocks);
    
}
/*
 * free - pointer = start of malloc payload
 */
void free(void* ptr)
{
    printf("Starting Free... \n");
    printf("Freeing address %p \n", ptr);
//    printf("heap low = %p \n", mem_heap_lo());
    //printf("")
    
    size_t headSize = (size_t) ((*((size_t *)ptr - 1)) & ~3);
    size_t bpsize = (size_t) ((*((size_t *)ptr - 1)));
    size_t blk_second = (bpsize & 2);
    printf("size is %ld\n", *((size_t*)ptr -1 ));

    // if ptr address provided is NULL/invalid
    if (ptr == NULL)
    {
        return;
    }
    //temp took out this else if bc we are never going into the else

    // Otherwise, provided block address is freed, size readjusted, payload rest to zero
    else
    {
    	size_t *temp = (size_t *) ((char *)ptr + headSize - 8);// 8 because char casting
    	// check to see if there is a next block
	    
        //if ((*temp & ~3) >= 32)
      //  {
         
            // size_t* nextBlock = (size_t*)((char*)temp + ptrSize - 8);
        *temp = (*temp & ~2); // set the header of next block second last bit to 0
            
	    //}

        
        //size_t savedBit =  (size_t) ((*(size_t *)ptr - 1) & 2);
        // 128 - 64 //
        
	*((size_t*)ptr - 1) = (bpsize | blk_second ) & ~1; // head changed to deallocatd
	*((size_t*)ptr ) = 0; // prev
	*((size_t*)ptr + 1) = 0; // next
	*((size_t*)ptr + headSize/8 - 2) = bpsize; // footer size
	
	freeBlocks = freeBlocks + 1;
//	printf("num free blocks = %d\n", freeBlocks);
	//add(ptr - 8);
        // change size to = header + footer + any optional padding + next pointer       
        // coalesce on every free() call. Need to implement. Just add to end of linked list.
        coalesce((size_t*)ptr - 1);
    }
  //  printf("Ending Free... \n");
    

}



/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{

    size_t sizeOld = *((size_t *)(oldptr - 8));
    size_t* new;
    // Unnecessary to check bit for a function that only accepts allocated blocks ---> maybe for edge case testing
    // size_t checAlloc = *(uint64_t *)(temp) & 0x1;
    
    if (oldptr == NULL)
    {    
        malloc(size); 
    }
    else if (size == 0)
    {    
        free(oldptr);
    }
    else if (sizeOld == size)
    {
        return oldptr;
    }
    else
    {

        /* Must already be from m/c/realloc = check allocator bit
        malloc(size)
        memcpy(new, old, *pointer to size of old)
            or memcpy(new, old, size)
        free(old) and add to free list
        */
       new = malloc(size);
       mem_memcpy(new, oldptr, size);
       free(oldptr);

       return new;

    }
    return NULL;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG

for (next = listTop, (*listTop & 0x0) == )







#endif /* DEBUG */
    return true;
}

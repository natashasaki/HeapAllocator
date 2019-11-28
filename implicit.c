/* This file contains the  implementation of the implicit free
   list allocator. Includes functions that initialize the  heap,
   deal with malloc, realloc, and freeing memory.
 */

/* Features of implicit:
   - Headers that track block information (8byte)
   - Free blocks that are recycled and reused for subsequent malloc requests if possible
   - malloc implementation searches heap for free blocks with implicit list
 */

#include "allocator.h"
#include "debug_break.h"
#include <string.h>

#define HEADER_SIZE 8

#define GET(p) (*(Header *)p).sa_bit //extracts header bits
#define GET_HEADER(blk) (Header *)blk - 1
#define GET_MEMORY(p) p + 1
#define GET_USED(p) (GET(p) & 0x1) //gets least significant bit
#define GET_SIZE(p) (GET(p) & ~0x7) // 3 LSB hold allocated status
#define SET_HEADER(p, val) (GET(p) = val)
#define SET_USED(p) (GET(p) |=  0x1)
#define SET_UNUSED(p) (GET(p) &= ~0x1)
#define GET_NEXT_HEADER(p) (Header*)((char*)p + GET_SIZE(p));

// node for linked list comprises of a ptr to the header and
// pointer to next node
typedef struct header {
    size_t sa_bit; // stores size and allocation status
} Header;

// global variable for  start of linked list
static void *segment_start;
static size_t nused;
static size_t segment_size;
static Header *base; //start node

//helper function header
Header *find_best_header(Header** cur_head, size_t size);

// rounds up sz to closest multiple of mult
size_t roundup(size_t sz, size_t mult) {
    return (sz + mult - 1) & ~(mult - 1);
}

// initialize the heap and return the  status of this
// initialization
bool myinit(void *heap_start, size_t heap_size) {
    /*  This must be called by a client before making any allocation
     * requests.  The function returns true if initialization was 
     * successful, or false otherwise. The myinit function can be 
     * called to reset the heap to an empty state. When running 
     * against a set of of test scripts, our test harness calls 
     * myinit before starting each new script.
     */

    // does heap have to be bigger than header size? can you initialize knowing
    // there is space for 0 blocks?
    if (heap_size < HEADER_SIZE) { 
        return false;
    }
    
    segment_start = heap_start;
    segment_size = heap_size;
    nused = 0;
    base = segment_start;
    (*base).sa_bit = 0; //clear heap by allowing overwrite
    return true;
}

// Malloc function that uses best fit to determine utilization of blocks. Implemented using a linked
// list of structs containing a pointer to the header and apointer to  the next node
void *mymalloc(size_t requested_size) {
    size_t req_size = roundup(requested_size, ALIGNMENT);
    size_t total_size = req_size  + HEADER_SIZE; // header is a multiple of alignment
    
    if (requested_size == 0 || requested_size > MAX_REQUEST_SIZE ||
        (req_size + nused > segment_size)) {
        return NULL;
    }

    Header *next_head_loc;
    void *block;
    Header *cur_head = base;
    Header *best_blk_head = find_best_header(&cur_head, total_size);
    
    if (best_blk_head != NULL) { // usable block found
        size_t best_blk_size = GET_SIZE(best_blk_head);
        SET_USED(best_blk_head);
        block = GET_MEMORY(best_blk_head);  //best_blk_head + 1;
        nused += (best_blk_size - HEADER_SIZE);
        return block;
        
    } else { // new allocation
        size_t header = total_size + 1;
        next_head_loc = GET_NEXT_HEADER(cur_head);
        SET_HEADER(next_head_loc, header);
        nused += total_size;
        block = GET_MEMORY(next_head_loc);
        return block;
    }
    return NULL;
}

// function that searches for a free, usable block of at least
// total_size  using best-fit (block with lowest amt of enough free space)
Header *find_best_header(Header** cur_head_ad, size_t total_size) {
    size_t best_blk_size = segment_size; //set to some max value
    Header *best_blk_head = NULL; 
    Header *cur_head = *cur_head_ad;
    size_t cur_blk_size = GET_SIZE(*cur_head_ad);
    
    while(cur_blk_size != 0) {  
        if(cur_blk_size >= total_size && !GET_USED(cur_head)) {
            if((cur_blk_size < best_blk_size) || (best_blk_head == NULL)) {
                best_blk_head = cur_head;
                best_blk_size = cur_blk_size;
            }
        }
        cur_head = GET_NEXT_HEADER(cur_head);
        cur_blk_size = GET_SIZE(cur_head);
    }
    *cur_head_ad = cur_head; // make sure current node in mymalloc is updated
    return best_blk_head;
}
    
// this function "frees"  memory by clearing the lowest bit in the header (where use of
// block  is stored) for future resuse
void myfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    Header* head = GET_HEADER(ptr);
    SET_UNUSED(head);
    nused -= (GET_SIZE(head) - HEADER_SIZE);   
}

// myrealloc moves memory to new location with the new size and copies
// over data from old pointer and frees the old_ptr
void *myrealloc(void *old_ptr, size_t new_size) {
    
    if (old_ptr == NULL) {
        return mymalloc(new_size); //nothing to copy over
        
    } else if (old_ptr != NULL && new_size == 0) { 
        myfree(old_ptr);
    } else {
        Header *old_head = GET_HEADER(old_ptr);
        size_t  old_size = GET_SIZE(old_head);
        void * new_ptr = mymalloc(new_size); // updating of new header done in malloc
        if (new_ptr == NULL) { //realloc failed
            return NULL;
        }
        if (new_size < old_size) {
            old_size = new_size;
        }
        memcpy(new_ptr, old_ptr, old_size);
        myfree(old_ptr);
        return new_ptr;
    }
    return NULL;
}

bool validate_heap() {
    /* TODO: remove the line below and implement this to 
     * check your internal structures!
     * Return true if all is ok, or false otherwise.
     * This function is called periodically by the test
     * harness to check the state of the heap allocator.
     * You can also use the breakpoint() function to stop
     * in the debugger - e.g. if (something_is_wrong) breakpoint();
     */

    if(!base)  {
        return false;
    }
    return true;
}

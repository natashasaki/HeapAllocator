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

static void *segment_start;
static size_t segment_size;
static size_t nused;

#define HEADER_SIZE 8

#define GET(p) (*(size_t *)p) //extracts header bits
#define GET_HEADER(blk) (size_t*)((char *)blk - HEADER_SIZE)
#define GET_MEMORY(p) ((char *)p  + HEADER_SIZE)

// LSB on: used; off: unused
#define GET_USED(p) (GET(p) & 0x1) //gets least significant bit
#define GET_SIZE(p) (GET(p) & ~0x8) // 3 LSB hold allocated status
#define SET(p, val) (*(size_t *)p = val)
#define SET_USED(p) (GET(p) |=  0x1)
#define SET_UNUSED(p) (GET(p) &=  ~0x1)

//#define NEXT(p) ((char*)p + GET_SIZE(p) - HEADER_SIZE)

// node for linked list comprises of a ptr to the header and
// pointer to next node
typedef struct node {
    size_t *header;
    struct node *next;
} node_t;

// global variable for  start of linked list
static void *segment_start;
static size_t nused;
static size_t segment_size;
static node_t start; //of linked list
static node_t * base;

// start: 0x107000000 
// size: 4294967296  (larger than  32 bytes unsigned max)

//write linked  list  fxn
// write next block extraction function

size_t roundup(size_t sz, size_t mult) {
    return (sz + mult - 1) & ~(mult - 1);
}

// !!expected to wipe the allocator's slate clean and start fresh!!
bool myinit(void *heap_start, size_t heap_size) {
    /*  This must be called by a client before making any allocation
     * requests.  The function returns true if initialization was 
     * successful, or false otherwise. The myinit function can be 
     * called to reset the heap to an empty state. When running 
     * against a set of of test scripts, our test harness calls 
     * myinit before starting each new script.
     */

    // set first  linke list  as NULL  overwrite later
 
    // how to free?? 
    segment_start = heap_start;
    segment_size = heap_size;
    nused = 0;
    start.header = segment_start;
    start.next = NULL;
    base =  &start;
    if (heap_size <= HEADER_SIZE) {
        return false;
    }
    return true;
}

// Malloc function that uses best fit to determine utilization of blocks. Implemented using a linked
// list of structs containing a pointer to the header and apointer to  the next node
void *mymalloc(size_t requested_size) {

    // make sure allocated block aligns with ALIGNMENT!!
    // ALIGMENT for PAYLOAD not internal heap data like HEADER

    // 8 bytes to store size & status in-use vs not (use any of 3 LSB to store)
    size_t total_size = roundup(requested_size + HEADER_SIZE, ALIGNMENT); 
    if (requested_size == 0 || requested_size > MAX_REQUEST_SIZE ||
        total_size + nused > segment_size) {
        return NULL;
    }

    //if base of linked list is NULL create new struct with header ptr and next ptr 
    
    // implements best-fit search
    // search each block for space
    // goes through headers to check size of block and status
    void *best_blk_head = NULL;
    size_t best_blk_size = segment_size; //set to some max value
    node_t *cur_node = base;
    size_t *cur_head = cur_node->header;
    node_t next_node;
    size_t cur_blk_size;
    //void *node;
    
    
    while(cur_node->next) { //go through up to last node
        cur_blk_size = GET_SIZE(cur_head);
        if(cur_blk_size >= total_size && !GET_USED(cur_head)) {
            if((cur_blk_size < best_blk_size) || (best_blk_head == NULL)) {
                best_blk_head = cur_head;
                best_blk_size = cur_blk_size;
            }
        }
        cur_node = cur_node->next; //next node
        cur_head= cur_node->header;
    }
    
    if (best_blk_head != NULL) { // usable block found
        SET_USED(best_blk_head);
        void* block = GET_MEMORY(best_blk_head);
        nused += total_size;
        return block;
        
    } else { // new allocation
        // set header
        size_t header = (total_size |= 0x1); //multiple of 8 ie last 3 bytes 0's)

        next_node.header = (size_t *)((char*)cur_head + GET_SIZE(cur_head));
        next_node.next = NULL;
        *(next_node.header) = header;

        cur_node->next = &next_node;
        nused += total_size;
        return GET_MEMORY(next_node.header);

        
        // node = (node_t*)((char*)cur_node + total_size);
        // ((node_t*)node)->next = NULL;
        // ((node_t*)node)->ptr = (size_t *)((char*)cur_node->ptr + total_size);
        //  next_node = node;
        //  cur_node->next = next_node;  // unitialized... ??
        // next_node->ptr = (size_t *)((char*)cur_node->ptr + total_size); //in general segment start + nused 
        //  next_node->next = NULL;
        
        //    if (cur_node == base) {
        //      base = cur_node;
        //  }
    }
    //nused += GET_SIZE(best_blk);
    return NULL;
}

// this function "frees"  memory by clearing the lowest bit in the header (where use of
// block  is stored) for future resuse
void myfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    size_t *head = GET_HEADER(ptr);
    SET_UNUSED(head);
    nused -= GET_SIZE(head);
}

// myrealloc moves memory to new location with the new size and copies
// over data from old pointer and frees the old_ptr
void *myrealloc(void *old_ptr, size_t new_size) {
    size_t *old_head = GET_HEADER(old_ptr);
    if (old_ptr == NULL) {
        return  mymalloc(new_size); //nothing to copy over
    } else if (old_ptr != NULL && new_size == 0) { 
        myfree(old_ptr);
    } else {
        void* new_ptr = mymalloc(new_size); // updating of new header done in malloc
        if (new_ptr == NULL) { //realloc failed
            return NULL;
        }
       
        memcpy(new_ptr, old_ptr, GET_SIZE(old_head));
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

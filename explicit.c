/* This program contains the implementation of the explicit heap allocator,
   builds largely on the implicit free list allocator (see implicit.c).
   However, this implementation manages to improve utilization and speed.
   Includes unique malloc, free, realloc functions.
*/

/*
   Unique points about explicit implementation: 
   -  list managed as a doubly-linked list
   - Malloc searches explicit list of free blocks
   - freed block coalesced with right neighbor if also free
   - Realloc resizes a block in-place whenever possible
 */

#include "allocator.h"
#include "debug_break.h"
#include <string.h>

#define HEADER_SIZE 8
#define MIN_BLOCK_SIZE HEADER_SIZE + sizeof(ListPointers)

#define GET(p) (*(Header *)p).sa_bit //extracts header bits
#define GET_HEADER(blk) (Header *)blk - 1
#define GET_MEMORY(p) p + 1
#define GET_USED(p) (GET(p) & 0x1) //gets least significant bit
#define GET_SIZE(p) (GET(p) & ~0x7) // 3 LSB hold allocated status
#define SET_HEADER(p, val) (GET(p) = val)
#define SET_USED(p) (GET(p) |=  0x1)
#define SET_UNUSED(p) (GET(p) &= ~0x1)

//possibly change
#define GET_NEXT_HEADER(p) (Header*)((char*)p + GET_SIZE(p))

#define GET_LISTPOINTERS(p) (ListPointers *)((Header*)p + 1)
//#define GET_NEXT_PTR(p) GET_LISTPOINTERS(p)->next
//#define GET_PREV_PTR(p) GET_LISTPOINTERS(p)->prev
#define SET_NEXT_PTR(p1, p2) p1->next = p2->next
//#define SET_PREV_PTR(p1, p2)


// node for linked list comprises of a ptr to the header and
// pointer to next node
typedef struct header {
    size_t sa_bit; // stores size and allocation status
} Header;

typedef struct pointers {
    Header *prev;
    Header *next;
} ListPointers;

// global variable for  start of linked list
static void *segment_start;
static size_t nused;
static size_t segment_size;
static Header *base; //start node
ListPointers *start;

//helper function header
Header *find_best_header(Header ** cur_head, size_t size);
void merge(ListPointers  *lp_ptr, ListPointers *lp_next);

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
    if (heap_size < MIN_BLOCK_SIZE) { 
        return false;
    }
    
    segment_start = heap_start;
    segment_size = heap_size;
    nused = 0;
    base = segment_start;
    (*base).sa_bit = 0; //clear heap by allowing overwrite
    start  = GET_LISTPOINTERS(base);
    start->prev = NULL;
    start->next = NULL;
    
    return true;
}

// Malloc function that uses best fit to determine utilization of blocks. Implemented using a linked
// list of structs containing a pointer to the header and apointer to  the next node
void *mymalloc(size_t requested_size) {
    size_t req_size = roundup(requested_size, ALIGNMENT);
    size_t total_size = req_size  + HEADER_SIZE; // header is a multiple of alignment

    if (total_size < MIN_BLOCK_SIZE) { //all blocks must have enough space to store header & ptrs
        total_size = MIN_BLOCK_SIZE;
    }
    
    if (requested_size == 0 || requested_size > MAX_REQUEST_SIZE ||
        (req_size + nused > segment_size)) {
        return NULL;
    }

    Header *next_head_loc;
    void *block;
    Header *cur_head = base;
    Header *best_blk_head = find_best_header(&cur_head, total_size);
    
    if (best_blk_head != NULL) { // usable block found

        // update pointers
        ListPointers *lp_best_blk = GET_LISTPOINTERS(best_blk_head);
        if (lp_best_blk->next) {
        ListPointers *next_blk = GET_LISTPOINTERS(lp_best_blk->next);
        next_blk->prev = lp_best_blk->prev;
        (GET_LISTPOINTERS(lp_best_blk->prev))->next = lp_best_blk->next;
        }
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
    Header *head = GET_HEADER(ptr);
    
    Header *next_head = (Header *)((char*)head + GET_SIZE(head));
    ListPointers *ptr_lp = GET_LISTPOINTERS(head);
    
    //need  to continuously  coaslesce? will it even enter this case or max
    //one merge?
    if (next_head && !GET_USED(next_head)) { // coalescing
        ListPointers *next_lp = GET_LISTPOINTERS(next_head);
        merge(ptr_lp, next_lp);
        //next_head = (Header *)((char*)head + GET_SIZE(head));
    } else { // if no merging, put  new free  ptr at front of linked list (LIFO)
        
        start->prev = head;
        ptr_lp->next = base;
        ptr_lp->prev = NULL;
        base = head;
        start = GET_LISTPOINTERS(base);
    }

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
        return NULL;
    } else {
        Header *cur_head = GET_HEADER(old_ptr);
        Header *next_head = GET_NEXT_HEADER(cur_head);
        ListPointers *next_lp = GET_LISTPOINTERS(next_head);
        size_t old_size = GET_SIZE(cur_head);
        new_size = roundup(new_size + HEADER_SIZE, ALIGNMENT);
        // def  can in-place reallocation
        if (new_size <= old_size) {
            if ((old_size - new_size) < MIN_BLOCK_SIZE) {
            // smaller free block can't be made
                return old_ptr; //keep same address

            } else { //create smaller free blocks
                SET_HEADER(cur_head, new_size + 1);

                start->prev = next_head;
                next_lp->next = base;
                next_lp->prev = NULL;
                base = next_head;
                start = GET_LISTPOINTERS(base);
                return old_ptr;
            }
        } else { //maybe can in-place realloc if coaslesce but maybe not
            
        
            while (next_head && !GET_USED(next_head)) {
                
                ListPointers *cur = GET_LISTPOINTERS(cur_head);
               
                merge(cur, next_lp);
                cur_head = GET_HEADER(cur);
                new_size = GET_SIZE(cur_head);
                if (new_size <= old_size) { //can be in-place realloced
                    return cur_head;
                }
                //otherwise: update variables and try again
                cur_head = next_head;
                next_head = GET_NEXT_HEADER(next_head);

            }
            //can't be inplace realloced; must malloc
     
           
            void *new_ptr = mymalloc(new_size); // updating of new header done in malloc
            if (new_ptr == NULL) { //realloc failed
                return NULL;
            }
            memcpy(new_ptr, old_ptr, old_size);
            myfree(old_ptr);
            return new_ptr;
        }
  
    }
}

// merges/coalesces a block and the block to the right, which is
// also unused. Takes in pointer to header of these blocks and
// updates pointers, and headers
void merge(ListPointers *lp_ptr, ListPointers *lp_next) {
    //void free(ptr p) { // p points to payload
    // ptr h1 = p – ; // b points to block header
    Header *ptr_h =  GET_HEADER(lp_ptr);
    SET_UNUSED(ptr_h); // clear allocated bit

    // will it know how far forward to move?
    //next_h = h1 + GET_SIZE(h1); // find next block (UNSCALED +)
    // if (!GET_USED(next_h)) { // if next block is not allocated,
    Header *next_h = GET_HEADER(lp_next);
    size_t new_header = GET(ptr_h) + GET(next_h);
    SET_HEADER(ptr_h, new_header); // add its size to this block

        //}
    // update pointers to free block
    // write functions at top for this!
    // first 16 bytes of payload = ptrs  to  prev and next free ptrs
    //  ListPointers *lp_1 = GET_LISTPOINTERS(h1);
    //  ListPointers *lp_next = GET_LISTPOINTERS(next_h);
    
    //SET_NEXT_PTR(h1, next_h);
    lp_ptr->next = lp_next->next;
    lp_ptr->prev = lp_next->prev;
  
    if (lp_next->next) {
        ListPointers *lp_new_next = GET_LISTPOINTERS(lp_next->next);
        lp_new_next->prev = ptr_h;
       
    }
    
    lp_next->next = NULL;
    lp_next->prev = NULL;
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

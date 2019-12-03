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
#include <stdio.h>
#include <stdbool.h>


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
static Header *segment_start;
static size_t nused;
static size_t segment_size;
static Header *base; //start node
static ListPointers *start;
static Header *top;
static Header *end;

//helper function header
Header *find_best_header(Header *cur_head, size_t size);
void merge(ListPointers  *lp_ptr, ListPointers *lp_next);
size_t adjust_block_size(size_t size);
void print_heap();
void print_linked_list();

// rounds up sz to closest multiple of mult
size_t roundup(size_t sz, size_t mult) {
    return (sz + mult - 1) & ~(mult - 1);
}


size_t adjusted_block_size(size_t size) {
    if((size + HEADER_SIZE) < MIN_BLOCK_SIZE) {
        return MIN_BLOCK_SIZE;
    } else {
        return roundup(size + HEADER_SIZE, ALIGNMENT);
    }
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
    top = segment_start;
    //  memset(segment_start,0, segment_size);
           
    //Header* s = segment_start;
    (*top).sa_bit = 0;
    end = top;
    
    base = NULL;
    //(*base).sa_bit = 0; //clear heap by allowing overwrite
    start = NULL;
    //start->prev = NULL;
    // start->next = NULL;
    
    return true;
}


void *mymalloc(size_t requested_size) {
    size_t req_size = roundup(requested_size, ALIGNMENT);
    size_t total_size = adjusted_block_size(requested_size);
    
    if (requested_size == 0 || requested_size > MAX_REQUEST_SIZE ||
        (req_size + nused > segment_size)) {
        return NULL;
    }

    void *block; //block to be returned
    Header *cur_head = base;
    Header *best_blk_head = find_best_header(base, total_size);
    
    if (best_blk_head != NULL) { // usable block found

        // update pointers
        ListPointers *lp_best_blk = GET_LISTPOINTERS(best_blk_head);
        if (lp_best_blk->next) { // true if more than 1 blk if  the  blk is not end
            ListPointers *next_blk = GET_LISTPOINTERS(lp_best_blk->next);
            next_blk->prev = lp_best_blk->prev;

            if (next_blk->prev) { // if not at fro
                (GET_LISTPOINTERS(lp_best_blk->prev))->next = lp_best_blk->next;
            }
        } else {
            if (lp_best_blk->prev) {
                (GET_LISTPOINTERS(lp_best_blk->prev))->next = NULL;
            }
        }
        size_t best_blk_size = GET_SIZE(best_blk_head);
        SET_USED(best_blk_head);

        if (best_blk_head == base) {
            base = (GET_LISTPOINTERS(base))->next;
            start = GET_LISTPOINTERS(base);

        }
        block = GET_MEMORY(best_blk_head);  //best_blk_head + 1
        nused += (best_blk_size - HEADER_SIZE);
        return block;
        
    } else { // new allocation
        if (end == top && !GET_SIZE(top)) {
             cur_head = end;
        } else {
            cur_head = GET_NEXT_HEADER(end);
            end = cur_head;
        }
        
        size_t header = total_size + 1;
        SET_HEADER(cur_head,header);
       // next_head_loc = GET_NEXT_HEADER(cur_head);
      //  SET_HEADER(next_head_loc, header);
        nused += total_size;
        block =  GET_MEMORY(cur_head);
        return block;
    }
    return NULL;
}

// FUNCTION THAT SEARCHES FOR A FREE, USABLE BLOCK OF AT LEAST
// TOTAL_SIZE  USING BEST-FIT (BLOCK WITH LOWEST AMT OF ENOUGH FREE SPACE)
Header *find_best_header(Header* cur_head, size_t total_size) {
    // size_t best_blk_size = segment_size; //SET TO SOME MAX VALUE
    Header *best_blk_head = NULL; 
    // Header *cur_head = *cur_head_ad;
    // breakpoint();
    if (cur_head == NULL) {
        return NULL;
    }
    
    ListPointers *old_lp = GET_LISTPOINTERS(cur_head);
    size_t cur_blk_size = GET_SIZE(cur_head);
    int i = 0;

    // searches linked list
    // while ((i == 0) || cur_head) 
    while((i == 0) || (old_lp->next)) {  
        cur_blk_size = GET_SIZE(cur_head);
        if(cur_blk_size >= total_size && !GET_USED(cur_head)) {
            //  if((cur_blk_size < best_blk_size) || (best_blk_head == NULL)) {
                best_blk_head = cur_head;
                // best_blk_size = cur_blk_size;
                return best_blk_head;
        }
        i++;
        old_lp = GET_LISTPOINTERS(cur_head); 
        cur_head = old_lp->next;
    }
    return best_blk_head;
}
    
// THIS FUNCTION "FREES"  MEMORY BY CLEARING THE LOWEST BIT IN THE HEADER (WHERE USE OF
// BLOCK IS STORED) FOR FUTURE RESUSE
void myfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    
    Header *head = GET_HEADER(ptr);
    nused -= (GET_SIZE(head) - HEADER_SIZE);
    ListPointers *ptr_lp = GET_LISTPOINTERS(head);
    Header *next_head = GET_NEXT_HEADER(head);
    if (base && head != end && !GET_USED(next_head)) { // coalescing
        ListPointers *next_lp = GET_LISTPOINTERS(next_head);
        //  breakpoint();
        merge(ptr_lp, next_lp);
        if (GET_HEADER(next_lp) == end) {
            end = head;
        }
    
    } else { // if no merging, put  new free  ptr at front of linked list (LIFO)
        if (!base) {
            base = head;
            start = GET_LISTPOINTERS(head);
            start->prev = NULL;
            start->next = NULL;
         
        } else { 
            start->prev = head;
            ptr_lp->next = base;
            ptr_lp->prev = NULL;
            base = head;
            start = GET_LISTPOINTERS(base);
        }
    }
    SET_UNUSED(head);
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
        size_t old_size = GET_SIZE(cur_head);
        Header *next_head;
        ListPointers *next_lp;
        nused -= (old_size - HEADER_SIZE);
        size_t adjusted_size = adjusted_block_size(new_size);
        
        char temp_data[old_size];
        char* temp = &temp_data[0];
        
        memcpy(temp, old_ptr, old_size);
        // def  can in-place reallocation
        if (adjusted_size <= old_size) {
            //   if ((old_size - adjusted_size) < MIN_BLOCK_SIZE) {
                nused += (old_size - HEADER_SIZE);
                //  next_head  = NULL;
                // smaller free block can't be made
                //  SET_HEADER(next_head, 0);
                memmove(old_ptr, old_ptr, new_size);
                return old_ptr;
                //   return old_ptr; //keep same address
                //    }
                //     } else { //create smaller free blocks
                //         SET_HEADER(cur_head, adjusted_size + 1);
                //         next_head = GET_NEXT_HEADER(cur_head);
                //         next_lp = GET_LISTPOINTERS(next_head);
                //         start->prev = next_head;
                //        next_lp->next = base;
                //       next_lp->prev = NULL;
                //       base = next_head;
                //       start = GET_LISTPOINTERS(base);
                //      nused += (adjusted_size -  HEADER_SIZE);
                //      return old_ptr;
                //   }
        } else { //maybe can in-place realloc if coaslesce but maybe not
            size_t updated_block_size = old_size;
            bool reached_end = false;
           
            ListPointers *cur = GET_LISTPOINTERS(cur_head);
            int i = 0;
            if (cur_head != end) {
                next_head = GET_NEXT_HEADER(cur_head);
                while ((i == 0 && !GET_USED(next_head)) || (!reached_end && GET_SIZE(next_head) &&!GET_USED(next_head))) {
                    
                    next_lp = GET_LISTPOINTERS(next_head);
                    // breakpoint();
                    merge(cur, next_lp);
                    updated_block_size = GET_SIZE(cur_head);
                    
                    if (next_head == end) {
                        end = cur_head;
                        reached_end = true;
                        // breakpoint();
                    }
                    if ((new_size + HEADER_SIZE) <= updated_block_size) { //can be in-place realloced
                        //  breakpoint();
                        nused += (old_size - HEADER_SIZE);
                        SET_USED(cur_head);

                        // deal with 4 possible cases
                        // 1) cur_head has a valid prev and next (ie cur_head in middle of linkedlist)
                        // 2) cur_head has a prev but no next (ie cur_head is last in linked_list)
                        // 3) cur_head has a next but no prev (ie cur_head is base with other nodes in list)
                        // 4) cur_head has neither next nor prev (ie cur_head is only node in linkedlist)
                        
                        if (cur->prev) {
                            //case 2
                            (GET_LISTPOINTERS(cur->prev))->next = cur->next; 
                            if (cur->next) { //case 1
                                (GET_LISTPOINTERS(cur->next))->prev = cur->prev;
                            }   
                        } else {
                            if (cur->next) { // case 3
                                base = cur->next;
                                start = GET_LISTPOINTERS(base);
                                (GET_LISTPOINTERS(cur->next))->prev = NULL;
                            } else {  // case 4
                                base = NULL;
                                start  = NULL;
                                //start = GET_LISTPOINTERS(base);
                                //  start->prev =  NULL;
                                // start->next = NULL;
                            }
                        }   
                        memcpy(old_ptr, temp, old_size);
                        return old_ptr;
                    }
                    
                    //otherwise: update variables and try again
                    if (!reached_end) {
                        next_head = GET_NEXT_HEADER(next_head);
                    } else  {
                        //breakpoint();
                        break;
                    }
                    i++;
                }
            }
            
            //can't be inplace realloced; must malloc
            //   breakpoint();
            void *new_ptr = mymalloc(new_size); // updating of new header done in malloc
            if (new_ptr == NULL) { //realloc failed
                return NULL;
            }
            //  memcpy(new_ptr, temp, old_size);
            memcpy(new_ptr, temp, old_size);
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

    //update block size  
    size_t new_header = GET(ptr_h) + GET(next_h);
    SET_HEADER(ptr_h, new_header); // add its size to this block
    SET_HEADER(next_h, 0);

        //}
    // update pointers to free block
    // write functions at top for this!
    // first 16 bytes of payload = ptrs  to  prev and next free ptrs
    //  ListPointers *lp_1 = GET_LISTPOINTERS(h1);
    //  ListPointers *lp_next = GET_LISTPOINTERS(next_h);
    
    //SET_NEXT_PTR(h1, next_h);
    if (next_h == base) {
        base = GET_HEADER(lp_ptr);
        start = lp_ptr;
    }
    lp_ptr->next = lp_next->next;
    lp_ptr->prev = lp_next->prev;

    // what if
    if (lp_next->next) {
        ListPointers *lp_new_next = GET_LISTPOINTERS(lp_next->next);
        lp_new_next->prev = ptr_h;
       
    }
    if (lp_ptr->prev) {
        ListPointers *lp_before = GET_LISTPOINTERS(lp_ptr->prev);
        lp_before->next = ptr_h;
    }
    
    lp_next->next = NULL;
    lp_next->prev = NULL;
   
}

// some functions to trace the  heap and check if  output  is right
bool validate_heap() {
    //    print_linked_list();

    // Header *s = segment_start;
    //   if (GET_SIZE(s)) {
    
        //    print_heap();
    //   }

    // printf("\n\n\n");
      return true;
}

void print_linked_list() {
    printf("linked list: \n");
    Header* cur = base;
    if (cur) {
        while ((GET_LISTPOINTERS(cur))->next) {
            printf("Header Address: %p   ; Header: %lu\n", cur, GET(cur));
            cur = (GET_LISTPOINTERS(cur))->next; //header
        }
        
        printf("Header Address: %p   ; Header: %lu", cur, GET(cur));
        printf("\n\n");
    }
}
void print_heap() {
    Header *cur = segment_start;
    printf("Print entire heap: \n");

    if (GET_SIZE(cur) != 0) {
        while(cur && GET_SIZE(cur)) {
            printf("Header Address: %p ; Header: %lu\n", cur, GET(cur));
            cur = GET_NEXT_HEADER(cur);
        }
    }
}

/* This program contains the implementation of the explicit heap allocator,
   builds largely on the implicit free list allocator (see implicit.c).
   However, this implementation manages to improve utilization and speed.
   Merges, in place reallocs
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
#define GET_NEXT_HEADER(p) (Header*)((char*)p + GET_SIZE(p))
#define GET_LISTPOINTERS(p) (ListPointers *)((Header*)p + 1)
#define SET_NEXT_PTR(p1, p2) p1->next = p2->next
                                                                    
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
static Header *base; //start of linkedlist
static ListPointers *start; // base ptrs
static Header *top;
static Header *end;

//helper function header
void merge(ListPointers  *lp_ptr, ListPointers *lp_next);
size_t adjust_block_size(size_t size);
void print_heap();
void print_linked_list();
Header *make_new_allocation(size_t allocate_size);
void allocate_usable_block(Header* block_head);
Header *find_block_header(size_t size);
void middle_merging(ListPointers *cur_lp, ListPointers *next_lp, bool cur_allocated);
void base_merging(ListPointers *cur_lp, ListPointers *next_lp, bool cur_allocated);
void end_merging(ListPointers *cur_lp, ListPointers *next_lp, Header* old_base);
void make_smaller_block(Header *cur_head, size_t adjusted_size, size_t old_size);
bool can_inplace_realloc(Header *cur_head, size_t new_size);
bool check_alignment();
bool check_heap_size();

// rounds up sz to closest multiple of mult
size_t roundup(size_t sz, size_t mult) {
    return (sz + mult - 1) & ~(mult - 1);
}

// rounds up based on minimum size or alignment
size_t adjusted_block_size(size_t size) {
    if((size + HEADER_SIZE) < MIN_BLOCK_SIZE) {
        return MIN_BLOCK_SIZE;
    } else {
        return roundup(size + HEADER_SIZE, ALIGNMENT);
    }
}
// initialize heap and return status of this initialization
bool myinit(void *heap_start, size_t heap_size) {
    
    if (heap_size < MIN_BLOCK_SIZE) { 
        return false;
    }

    // initialize global variables and clear heap
    segment_start = heap_start;
    segment_size = heap_size;
    top = segment_start;
    SET_HEADER(top, segment_size);
    end = top;
    base = top;
    start = GET_LISTPOINTERS(base);
    start->prev = NULL;
    start->next = NULL;
    
    return true;
}

// function that allocates memory onto the heap either
// by finding suitable free block given requested size
// or by making a new allocation
void *mymalloc(size_t requested_size) {
    size_t total_size = adjusted_block_size(requested_size);
    
    if (nused >= segment_size || requested_size == 0 || requested_size > MAX_REQUEST_SIZE) {
        return NULL;
    }
    
    void *block; //block to be returned
    Header *usable_blk_head = find_block_header(total_size);
    
    if (usable_blk_head != end) { // recyclable block found
        allocate_usable_block(usable_blk_head);
        size_t blk_size = GET_SIZE(usable_blk_head);
        nused += (blk_size - HEADER_SIZE);
        return GET_MEMORY(usable_blk_head);   
    } else {
        block = make_new_allocation(total_size);
    }
    return block;
}

// function that allocates a block that is found to be
// usable and returns the block with its updated pointers
void allocate_usable_block(Header* block_head) {
     // deal with 4 possible cases
     // 1) cur_head has a valid prev and next (ie cur_head in middle of linkedlist)
     // 2) cur_head has a prev but no next (ie cur_head is last in linked_list)
     // 3) cur_head has a next but no prev (ie cur_head is base with other nodes in list)
     // 4) cur_head has neither next nor prev (ie cur_head is only node in linkedlist)
             
    ListPointers *lp_blk = GET_LISTPOINTERS(block_head);
    ListPointers *next_blk = GET_LISTPOINTERS(lp_blk->next);
    next_blk->prev = lp_blk->prev;
    
    if (lp_blk->prev) {
        ListPointers *lp_before_cur = GET_LISTPOINTERS(lp_blk->prev);
        if (lp_blk->next) { // case 1
            ListPointers *lp_after_next = GET_LISTPOINTERS(lp_blk->next);
            lp_before_cur->next = lp_blk->next;
            lp_after_next->prev = lp_blk->prev;
        } else { // case 2
            lp_before_cur = NULL;
        }
           
    } else {
        if (lp_blk->next) { // case 3
            base = lp_blk->next;
            start = GET_LISTPOINTERS(base);
            start->prev = NULL;
        } else { // case 4
            base = NULL;
            start = NULL;
        }
    }
    
    SET_USED(block_head);
    lp_blk->next= NULL;
    lp_blk->prev = NULL;
}


// makes new allocation at end of heap where the end  is the free
// remaining block of heap left segment. Returns new block
Header *make_new_allocation(size_t allocate_size) {
    Header *cur_head = end;
    size_t old_blk_size = GET_SIZE(cur_head);
    ListPointers *old_ls = GET_LISTPOINTERS(cur_head);
    size_t header = allocate_size + 1;  // +1 for allocated
  
    SET_HEADER(cur_head, header);
    nused += allocate_size;
    Header *remaining_seg = GET_NEXT_HEADER(cur_head);
    
    if (cur_head == base) { //remaining heap seg is only node, must update base
        base = GET_NEXT_HEADER(cur_head);
        start = GET_LISTPOINTERS(base);
        start->next = NULL;
        start->prev = NULL;
        SET_HEADER(base, old_blk_size - allocate_size);
           
    } else { //allocate from reamining segment of heap
        ListPointers *new_remain = GET_LISTPOINTERS(remaining_seg);
        ListPointers *prev_node = GET_LISTPOINTERS(old_ls->prev);
        prev_node->next = remaining_seg;
        new_remain->prev = old_ls->prev;
        new_remain->next = NULL;
        SET_HEADER(remaining_seg, old_blk_size - allocate_size);
    }
    old_ls->prev = NULL; //allocated block's ptr
    end = remaining_seg;
    return GET_MEMORY(cur_head);
}

// function that searches for a free, usable block of at least
// total_size using first-fit (block with lowest amt of enough free space)
Header *find_block_header(size_t total_size) {
    Header *best_blk_head = end;
    Header *head = base;
    ListPointers *old_lp = GET_LISTPOINTERS(head);
    size_t cur_blk_size = GET_SIZE(head);
    
    while(old_lp->next) {
        cur_blk_size = GET_SIZE(head);
        if(cur_blk_size >= total_size && !GET_USED(head)) {
            best_blk_head = head;
            return best_blk_head;
        }
        old_lp = GET_LISTPOINTERS(head); 
        head = old_lp->next;
    }
    return best_blk_head;
}
    
// this function frees memory and adds the pointer onto the linked
// list and merges with next block on right if possible
// if pointer can't be merged, it goes to front of linkedlist
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
        merge(ptr_lp, next_lp);
        if(GET_HEADER(next_lp) == end) { //update block that's at end of heap
            end = head;
        }
    
    } else { // if no merging, put new free ptr at front of linked list
        start->prev = head;
        ptr_lp->next = base;
        ptr_lp->prev = NULL;
        base = head;
        start = GET_LISTPOINTERS(base);
    }
    SET_UNUSED(head);
}

// myrealloc tries to do in-place reallocation if possible
// either if new_size smaller than old_size or through merging
// creates smaller blocks out of larger coalesced blocks if
// possible. Otherwise, moves memory to new location 
void *myrealloc(void *old_ptr, size_t new_size) {
    if (old_ptr == NULL) {
        return mymalloc(new_size);
        
    } else if (old_ptr != NULL && new_size == 0) { 
        myfree(old_ptr);
        return NULL;
        
    } else {
        Header *cur_head = GET_HEADER(old_ptr);
        size_t old_size = GET_SIZE(cur_head);
        nused -= old_size;
        size_t adjusted_size = adjusted_block_size(new_size);
        char temp_data[old_size]; 
        char *temp = &temp_data[0];
        if (old_size != segment_size) { // store old data to be realloced
            memcpy(temp, old_ptr, old_size);
        }
        
        if (adjusted_size <= old_size) {   // guaranteed can in-place realloc
            memmove(old_ptr, old_ptr, new_size);
            if ((old_size - adjusted_size) < MIN_BLOCK_SIZE) { // too small for  new block
                nused += old_size;
            } else { // big enough to create smaller blocks
                make_smaller_block(cur_head, adjusted_size, old_size);
                nused += adjusted_size;
            }
            return old_ptr;
            
        } else { // possibly can in-place realloc if coaslesce but maybe not
            if (can_inplace_realloc(cur_head, new_size)) {
                allocate_usable_block(cur_head);
                memcpy(old_ptr, temp, old_size);
                return old_ptr;
            } else {  //can't be inplace realloced; must malloc
                void *new_ptr = mymalloc(new_size);
                nused += HEADER_SIZE; 
                if (new_ptr == NULL) { //realloc failed
                    return NULL;
                }
                memcpy(new_ptr, temp, old_size);
                return new_ptr;
            }
        }
    }
}

// function that tries to continuously merge free blocks for realloc
// and returns status on whether in-place realloc is possible
bool can_inplace_realloc(Header *cur_head, size_t new_size) {
    size_t old_size = GET_SIZE(cur_head);
    size_t updated_block_size = old_size;
    ListPointers *lp_cur = GET_LISTPOINTERS(cur_head);
    ListPointers *lp_next;
    if (cur_head == end) {
        return false;
    }
    Header *next_head = GET_NEXT_HEADER(cur_head);
    while (next_head && !GET_USED(next_head)) {
        lp_next = GET_LISTPOINTERS(next_head);
        merge(lp_cur, lp_next);
        updated_block_size = GET_SIZE(cur_head);

        if (next_head == end) {
            end = cur_head;
            return false;
        }
        if ((new_size + HEADER_SIZE) <= updated_block_size) { //can be in-place realloced
            nused += updated_block_size;
            return true;
        }
        next_head = GET_NEXT_HEADER(cur_head); // try again
    }
    return false;  
}

    
// function to make a smaller block if coalesced
// block is big enough to create a smaller block
void make_smaller_block(Header *cur_head, size_t adjusted_size, size_t old_size) {
    SET_HEADER(cur_head, adjusted_size + 1);
    Header *new_head = GET_NEXT_HEADER(cur_head);
    size_t new_blk_size = old_size - adjusted_size;
    SET_HEADER(new_head, new_blk_size + 1); //set as free

    if (cur_head == end) {
        end = new_head;
    }
    nused += old_size;
    if (!base) {
        base = new_head;
        start = GET_LISTPOINTERS(base);
        start->next = NULL;
        start->prev = NULL;
        SET_UNUSED(new_head);
    } else{
        myfree(GET_MEMORY(new_head));
    }
}

// coalesces a block and its right block. Deals with cases of
// merging used block with free block and when both  blocks are free
// (ie repeated merging) 
void merge(ListPointers *lp_cur, ListPointers *lp_next) {
    Header *cur_h = GET_HEADER(lp_cur);
    Header *next_h = GET_HEADER(lp_next);
    Header *old_base = base;
    bool cur_is_allocated = GET_USED(cur_h);
    
    // Cases for merging blocks when both already freed
    // 1) FE: left block is base/front, right block is end
    // 2) EF: left = end; right = front/base
    // 3) MM: both nodes are middle of linked list
    // 4) FM: left = base/first, right = middle
    // 5) EM: left = end, right = middle
    // 6) MF: left = middle, right = front/base
    // cases not mutually exclusive to account for if next_h is base and end
    
    if(next_h == base) { // case 2, 6
        base_merging(lp_cur, lp_next, cur_is_allocated);
    }

    if(next_h == end) { // case 1
        end_merging(lp_cur, lp_next, old_base);
    }

    if(lp_next->prev && lp_next->next) { // next block is middle of linked list
        // case 3, 4, 5
        middle_merging(lp_cur, lp_next, cur_is_allocated);
    }
 
    // update block size headers and pointers
    size_t new_header = GET_SIZE(cur_h) + GET(next_h);
    SET_HEADER(cur_h, new_header);
    SET_HEADER(next_h, 0);
    SET_UNUSED(cur_h); 
    lp_next->next = NULL;
    lp_next->prev = NULL;
}

// function that deals with merging when the next block is in the middle
// of linked list (case 3, 4, 5 mentioned in merge)
void middle_merging(ListPointers *cur_lp, ListPointers *next_lp, bool cur_allocated) {
    Header *cur_head = GET_HEADER(cur_lp);
    ListPointers *lp_before_cur;
    ListPointers *lp_before_next = GET_LISTPOINTERS(next_lp->prev);
    ListPointers *lp_after_next = GET_LISTPOINTERS(next_lp->next);
                                          
    if(!cur_allocated && cur_head != end) { // case 3, 4 
        lp_before_next->next = next_lp->next;
        lp_after_next->prev = next_lp->prev;
    } else {
        if (!cur_allocated) { // case 5
            lp_before_cur = GET_LISTPOINTERS(cur_lp->prev);
            lp_before_cur->next = NULL;
        } 
        cur_lp->prev = next_lp->prev;
        cur_lp->next = next_lp->next;
        if(cur_allocated) { // first merge (in realloc or free)
            lp_before_next->next = cur_head;
            lp_after_next->prev = cur_head;
        }
    }
}

// function that deals with merging when next block is the base of linked list
// house-keeping of pointers depending on case
void base_merging(ListPointers *lp_cur, ListPointers *lp_next, bool cur_allocated) {
    Header *next_head = GET_HEADER(lp_next);
    Header *cur_head = GET_HEADER(lp_cur);
    ListPointers *lp_before_cur = GET_LISTPOINTERS(lp_cur->prev);
    
    if (!cur_allocated) { //bit = 0 ie used; merging 2 freed blocks; case 2, 6
        lp_before_cur->next = lp_cur->next;
        ListPointers *lp_after_cur = GET_LISTPOINTERS(lp_cur->next);
        lp_after_cur->prev = lp_cur->prev;
    }
    if (cur_allocated && next_head != end) {
        ListPointers *lp_after_next = GET_LISTPOINTERS(lp_next->next);
        lp_after_next->prev = cur_head;
    }
    // update pointers and global variables
    lp_cur->next = lp_next->next;
    lp_cur->prev = lp_next->prev; //ie NULL
    base = cur_head;
    start = GET_LISTPOINTERS(base);
}

// function that deals with merging when next block is end of linked list
// (merging with remaining heap seg); house-keeping of pointers depending on case
void end_merging(ListPointers *lp_cur, ListPointers *lp_next, Header *old_base) {
    ListPointers *lp_before_next = GET_LISTPOINTERS(lp_next->prev);
    Header *cur_head = GET_HEADER(lp_cur);
    Header *next_head = GET_HEADER(lp_next);
    
    if(!GET_USED(cur_head)) { // case 1
        lp_before_next->next = NULL;
    } else { // first merge; if right block is only node in list, will enter here too
        lp_cur->next = lp_next->next;
        lp_cur->prev = lp_next->prev;
        if (next_head != old_base) {
            lp_before_next->next = cur_head;
        }
    }
}
// some functions to trace the heap and check if output  is right
bool validate_heap() {
    // print_linked_list();

    // Header *s = segment_start;
    // if (GET_SIZE(s)) {
    //  print_heap();
    // }

    // printf("\n\n\n");
    //breakpoint();
    if (!check_alignment()) {
        return false;
    }
    if (!check_heap_size()) {
        return false;
    }
    
    return true;
}

// checks the alignment  of all of the blocks on the heap
// return true if aligned, false otherwise
bool check_alignment() {
    Header *cur = top;
    if (cur != end) {
        while (cur != GET_NEXT_HEADER(end)) {
            if (((unsigned long)cur & 7) != 0) {
                return false;
            }
            cur = GET_NEXT_HEADER(cur);
        }
    }
    return true;

}

// checks that all the blocks add up to the heap segment that
// was initialized; false if not
bool check_heap_size() {
    Header *cur = top;
    size_t sum_size = 0;
    
    while (cur != GET_NEXT_HEADER(end)) {
        sum_size += GET_SIZE(cur);
        cur = GET_NEXT_HEADER(cur);
    }
    return (sum_size == segment_size);
}

// prints header address and header info (ie total size and
// allocation) of each free block starting at base
void print_linked_list() {
    printf("linked list: \n");
    Header *cur = base;
    if (cur && cur != (Header*)((char*)top + segment_size)) {
        while ((GET_LISTPOINTERS(cur))->next) {
            printf("Header Address: %p   ; Header: %lu\n", cur, GET(cur));
            cur = (GET_LISTPOINTERS(cur))->next; //header
        }
        printf("Header Address: %p   ; Header: %lu", cur, GET(cur));
        printf("\n\n");
    }
}

// prints entire heap from segment_start address
// prints address and header information
void print_heap() {
    Header *cur = segment_start;
    printf("Print entire heap: \n");
    if (cur != end) {
        while(cur != GET_NEXT_HEADER(end)) {
            printf("Header Address: %p ; Header: %lu\n", cur, GET(cur));
            cur = GET_NEXT_HEADER(cur);
        }
    } else {
     printf("Header Address: %p   ; Header: %lu", cur, GET(cur));
        printf("\n\n");
    }
}

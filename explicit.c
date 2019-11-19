#include "allocator.h"
#include "debug_break.h"

bool myinit(void *heap_start, size_t heap_size) {
    /* TODO: remove the line below and implement this!
     * This must be called by a client before making any allocation
     * requests.  The function returns true if initialization was 
     * successful, or false otherwise. The myinit function can be 
     * called to reset the heap to an empty state. When running 
     * against a set of of test scripts, our test harness calls 
     * myinit before starting each new script.
     */
    return false;
}

void *mymalloc(size_t requested_size) {
    // TODO: remove the line below and implement this!
    return NULL;
}

void myfree(void *ptr) {
    // TODO: implement this!
}

void *myrealloc(void *old_ptr, size_t new_size) {
    // TODO: remove the line below and implement this!
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
    return false;
}

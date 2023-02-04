/* File: implicit.c 
 * Name: Matthew Louis Ayoob
 * -------------------------
 *
 * This file represents an implementation of an
 * implicit heap allocator with header and minimum
 * payload sizes of 8. It utilzes a plethora of helper
 * functions to simlify the code alongside my implementations 
 * of myinit,mymalloc, myfree, myrealloc, validate_heap, 
 * and dump_heap.
 */
#include "./allocator.h"
#include "./debug_break.h"
#include <stdio.h>
#include <string.h>

typedef size_t hdr;

// relevant  globals
static hdr *segment_start;
static size_t segment_size;
static hdr *segment_end;
static size_t nused;

// constant representing header size, min. payload size, and alignment
#define HDR_SIZE 8
#define MIN_BLOCK 16
#define MAX_REQUEST_SIZE (1 << 30)

/* Function: roundup (from bump.c)
 * -----------------
 * Parameters:
 *     sz - a size_t value to be rounded to the next multiple of n
 *     n   - the size_t that param 1 will be rounded to
 * 
 * Returns: NA
 * This function rounds up the given number to the given multiple, which
 * must be a power of 2, and returns the result.  (you saw this code in lab1!).
 */
size_t roundup(size_t sz, size_t mult) {
    return (sz + mult - 1) & ~(mult - 1);
}

/* Function: is_avail
 * -------------------------
 * Parameters:
 *     hdr_ptr - a pointer to a hdr type 
 * 
 * Returns: True - if the block is not allocated
 *          False - if the block is allocated
 *
 * This function tells if a parameter header's
 *  payload has already been allocated or not 
 * through a boolean return value.
 */
bool is_avail(hdr *hdr_ptr) {
    return !(*hdr_ptr & 0x1);
}

/* Function: make_taken
 * -------------------------
 * Parameters:
 *     hdr_ptr - a pointer to a hdr type 
 * 
 * Returns: NA
 *
 * This function will make a header's payload
 * "allocated" by turning on the LSB of the size_t
 * hdr type.
 */
void make_taken(hdr *hdr_ptr) {
    *hdr_ptr = *hdr_ptr | 0x1;
}

/* Function: taken_and_new_sz
 * -------------------------
 * Parameters:
 *     hdr_ptr - a pointer to a hdr type 
 *     needed_sz - a size_t representation
 *                 of the payload size
 * 
 * Returns: NA
 *
 * This function will make a header's payload
 * "allocated" by turning on the LSB of the size_t
 * hdr type while setting its payload size to 
 * the new size, or param2.
 */
void taken_and_new_sz(hdr *ptr, size_t needed_sz) {
    *ptr = needed_sz | 0x1;
}


/* Function: grab_pl
 * -------------------------
 * Parameters:
 *     hdr_ptr - a pointer to a hdr type 
 * 
 * Returns: size_t representation of the 
 *          header's payload
 *
 * This function will access a header's 
 * payload and remove the LSB indicator of
 * free of allocatd, returning the size_t
 * magnitude of the payload.
 */
size_t grab_pl(hdr *hdr_ptr) {
    return (*hdr_ptr & ~0x1); // 7
}


/* Function: set_pl_of_new
 * -------------------------
 * Parameters:
 *     old_hdr - a pointer to the old header 
 *     new_hdr - a pointer to the new header
 *     amt_to_remove - a size_t representation
 *                 of the amount just allocated
 * 
 * Returns: NA
 *
 * This function takes in an old and new pointer and
 * sets the remainder of the size left to the new header.
 */
void set_pl_of_new(hdr *old_hdr, hdr *new_hdr, size_t amt_to_remove) {
    if (new_hdr == segment_end) return;
    *new_hdr = *old_hdr - HDR_SIZE - amt_to_remove;
}

/* Function: skip_to_next_header
 * -------------------------
 * Parameters:
 *     hdr_ptr - a pointer to a hdr type 
 * 
 * Returns: void * representing the pointer
 *          to the next header
 *
 * This function will do the necessary pointer arithmitic
 * to skip the current header and its payload. This returns
 * the void * representation of the next header in the heap.
 */
void *skip_to_next_header(hdr *hdr_ptr) {
    return (char *)hdr_ptr + grab_pl(hdr_ptr) + HDR_SIZE;
}

/* Function: address_of_pl
 * -------------------------
 * Parameters:
 *     hdr_ptr - a pointer to a hdr type 
 * 
 * Returns: void * representing the pointer
 *          to a header's respective payload space.
 *
 * This function will do the necessary pointer arithmitic
 * to skip the current header, returning the intuitive address
 * to its payload.
 */
void *address_of_pl(hdr *ptr) {
    return (char *)ptr + HDR_SIZE;
}

/* Function: set_pl
 * -------------------------
 * Parameters:
 *     hdr_ptr - a pointer to a hdr type 
 *     new_sz - a size_t size to set payload to
 * 
 * Returns: NA
 *
 * This function will access a header's 
 * payload and replace it with its new size_t
 * value.
 */
void set_pl(hdr *hdr_ptr, size_t new_sz) {
    *hdr_ptr = new_sz;
}

/* Function: myinit
 * -------------------------
 * Parameters:
 *     heap_start - a pointer to a hdr type 
 *     heap_size - a size_t representation
 *                 of the payload size
 * 
 * Returns: NA
 *
 * This function is called by the test harness calls with every 
 * fresh script. It wipes the llocator's clean and start fresh, 
 * setting relevant globals, error checking, and makes the first header.
 */
bool myinit(void *heap_start, size_t heap_size) {
    /* This must be called by a client before making any allocation
     * requests.  The function returns true if initialization was 
     * successful, or false otherwise. The myinit function can be 
     * called to reset the heap to an empty state. When running 
     * against a set of of test scripts, our test harness calls 
     * myinit before starting each new script.
     */

    if (heap_size <= MIN_BLOCK || !heap_start ) { 
        return false;
    }

    // set globals about heap attributes
    segment_start = (hdr *)heap_start;
    segment_size = heap_size;
    segment_end = (hdr *)((char *)heap_start + segment_size); // grab heap start address and finds end of heap
    nused = 0;

    // validity checks about globals
    if (segment_start == NULL || segment_end == NULL ||
        (char *)segment_end - (char *)segment_start != segment_size) {
    return false;
    }

    // set up initial header and set its payload size
    *segment_start = segment_size - HDR_SIZE;
    return true;      
}


/* Function: mymalloc
 * -------------------------
 * Parameters: 
 *     requested_sizez - a size_t representation
 *               of the payload size to be allocated
 * 
 * Returns: the void * representation of the payload address
 *
 * This function will search the heap, header by header, to find
 * a free block to fit the allocation request. It will split the 
 * remainder of the space into a new header if it can satisfy
 * the minimum payload requirement.
 */
void *mymalloc(size_t requested_size) {
 
    if (requested_size <= 0 || requested_size > segment_size - HDR_SIZE || requested_size > MAX_REQUEST_SIZE) { // invalid request size
        return NULL;
    }

    // check properties of padded size
    size_t needed_sz = roundup(requested_size, ALIGNMENT);
    if (needed_sz <= 0) { 
        return NULL;
    }

    // now finds first fit for allocation if it exists
    hdr *looping_adr = segment_start;
    while ( looping_adr < segment_end) { // traverses implicit listblock-by-block
        
        if (is_avail(looping_adr) && grab_pl(looping_adr) >= needed_sz) {

            size_t rem  = grab_pl(looping_adr) - needed_sz;

            if (rem < MIN_BLOCK) {
                make_taken(looping_adr);
            
            } else {
                 hdr *new_hdr = (hdr *)((char *)looping_adr + HDR_SIZE + needed_sz);
                 set_pl_of_new(looping_adr, new_hdr, needed_sz);
                 taken_and_new_sz(looping_adr, needed_sz);
                 
             }
             return address_of_pl(looping_adr);
        }
        else {
            looping_adr = skip_to_next_header(looping_adr);
        }
    }
    return NULL;
}


/* Function: myfree
 * -------------------------
 * Parameters:
 *     ptr - a void * to the payload
 *           to be freed
 * 
 * Returns: NA
 *
 * This function will make a header's payload
 * "free" by turning off the LSB of the size_t
 * hdr type. It also updates the global representing
 * how much heap has been used. 
 */
void myfree(void *ptr) {
    if ((hdr *)ptr > segment_end || (hdr *)ptr < segment_start || !ptr) {
        return;
    }
    else { // turn off last bit
        hdr *temp_ptr = (hdr *)((char *)ptr - HDR_SIZE); // go back to header
        *temp_ptr &= ~(0x7); // turn off last bit 
    }
}


/* Function: myrealloc
 * -------------------------
 * Parameters:
 *     old_ptr - a pointer to the space to reallocate 
 *     new_size - a size_t representation
 *                 of the payload size
 * 
 * Returns: a void * to the payload of the new memory
 *
 * This function will reallocate a previous request to
 * a new size. In implicit, there is no in_place, so 
 * after some error checking, there is just a new
 * malloc call, and memmove is utilized.
 */
void *myrealloc(void *old_ptr, size_t new_size) {

    if (!old_ptr && new_size == 0) { //edge case from man pages
        myfree(old_ptr);
        return mymalloc(new_size);
        
    } else if  (new_size <= 0 || new_size > segment_size - HDR_SIZE) {
        myfree(old_ptr);
        return mymalloc(new_size); // malformed requests
    }

    void *new_request = mymalloc(new_size);

    if (new_request != 0) { // make sure new request was successful
        if (old_ptr != 0) {
           memmove(new_request, old_ptr, new_size);
           }
        myfree(old_ptr);
    }
    return new_request;
}


/* Function: validate_heap
 * -------------------------
 * Parameters: NA
 * 
 * Returns: boolean representation of 
 *         if heap validation was successful
 *
 * This function does some routine heap checks, such
 * as confirming normal heap initialization, amount of 
 * used space, free blocks, minimum payload, etc.
 */
bool validate_heap() {
    /* check your internal structures!
     * Return true if all is ok, or false otherwise.
     * This function is called periodically by the test
     * harness to check the state of the heap allocator.
     * You can also use the breakpoint() function to stop
     * in the debugger - e.g. if (something_is_wrong) breakpoint();
     */
    hdr *start_of_heap = segment_start;
    size_t total = 0; // will include framentation purely amt that is not usable
   
    if (start_of_heap == NULL) {
        printf("Incorrect Initialization of Heap");
        breakpoint();
        return false;
    }

    // check payload size and free status throughout heap
    while (start_of_heap < segment_end) {
        total += grab_pl(start_of_heap) + HDR_SIZE;
        if ( (*start_of_heap & ~0x1) < HDR_SIZE) {
            breakpoint();
            printf("Incorrect payload size, must be at least 8 in this design");
        }
        start_of_heap = skip_to_next_header(start_of_heap);  
    }

    if (total == segment_size) { 
        return true;
    }
    
    printf("Incorrect Heap Strucrure: Review header alignment, amount used, location, free status, and payload size etc.");
    breakpoint();
    
    return false;
}


/* Function: dump_heap
 * -------------------------
 * Parameters: NA
 * 
 * Returns: NA
 *
 * This function will create a meaningful
 * visualization of the heap used for debugging. 
 * It prints relevant information like free status, 
 * payload size, and the pointer. 
 */
void dump_heap() {
    hdr *heap_start = segment_start;

    while (heap_start < segment_end) {
        printf("|------------------------|---------------------------|---------------------------|\n");
        printf("|Free(1 = yes): %d        | Payload Size: %ld         | Pointer: %p          \n", is_avail(heap_start), grab_pl(heap_start), heap_start);
        printf("|------------------------|---------------------------|---------------------------|\n");
        heap_start = skip_to_next_header(heap_start);
    }

}

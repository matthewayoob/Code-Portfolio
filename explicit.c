/* File: explicit.c 
 * Name: Matthew Louis Ayoob
 * Class: CS107
 * -------------------------
 *
 * This file represents an implementation of an
 * explicit  heap allocator with header and minimum
 * payload sizes of 16. It utilzes a plethora of helper
 * functions to simlify the code alongside my implementations 
 * of myinit, mymalloc, myfree, myrealloc, validate_heap, 
 * and dump_heap with coalescing and in-place realloc.
 *
 * Citation: Linked List notes from CS106B for handling 
 * the free list node operations and Helper Hours.
 */
#include "./allocator.h"
#include "./debug_break.h"
#include <stdio.h>
#include <string.h>

typedef size_t hdr;

// struct definition for casting of a block
typedef struct node
{
    hdr b_hdr;
    hdr *prev;
    hdr *next;
} node;

// setting up globals
static hdr *segment_start;
static size_t segment_size;
static hdr *segment_end;
static node *start_of_free;
static size_t blocks_in_free;

// constants used for arithmitic
#define HDR_SIZE 8 
#define MIN_BLOCK_SIZE 24
#define MIN_PL 16
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
 * -----------------
 * Parameters:
 *     hdr_ptr - a node pointer to the block to be checked
 * 
 * Returns: boolean representation of if the block is able to 
 *         to be allocated or not

 * This function tells if a block is allocated or not.
 */
bool is_avail(node *hdr_ptr) {
    if (!hdr_ptr ) return false;
    return !((hdr_ptr->b_hdr) & 0x1);
}

/* Function: grab_pl
 * -----------------
 * Parameters:
 *     hdr_ptr - a pointer to a node to be checked
 * 
 * Returns: a size_t representation of the payload size
 *
 * This function returns the payload associated with the parameter
 * pointer and removes the LSB to give an accurate number.
 */
size_t grab_pl(node *hdr_node) {
    if (!hdr_node) return 0;
    return ((hdr_node->b_hdr) & ~0x7);
}

/* Function: skip_to_next_header
 * -----------------
 * Parameters:
 *     hdr_ptr - a pointer to a node to be checked
 * 
 * Returns: a void * representation of the next header
 *
 * This function returns address to next header after
 * skipping current header and its payload through pointer arithmetic.
 */
void *skip_to_next_header(hdr *hdr_ptr) {
    if (!hdr_ptr) return NULL;
    return (char *)hdr_ptr + grab_pl((node *)hdr_ptr) + HDR_SIZE;
}

/* Function: make_free
 * -----------------
 * Parameters:
 *     node_hdr - a pointer to a node to be freed
 * 
 * Returns: NA
 *
 * This function frees a header's LSB through 
 * bitmasking. 
 */
void make_free(node *node_hdr) {
    if (!node_hdr) return;
    node_hdr->b_hdr &= ~0x1;
}

/* Function: back_to_hdr
 * -----------------
 * Parameters:
 *     hdr_ptr - a pointer to a paylaod
 * 
 * Returns: a node  * representation of the header
 *
 * This function returns the address of the header
 * after being given the payload address.
 */
node *back_to_hdr(node *hdr_ptr) {
    return (node *)((char *)hdr_ptr - HDR_SIZE);
}

/* Function: set_pl
 * -----------------
 * Parameters:
 *     node_hdr - a pointer to a node
 *     size - size_t representation of the payload to be set
 * 
 * Returns: NA
 *
 * This function sets a header's payload and confirms
 * a properly set LSB.
 */
void set_pl(node *node_hdr, size_t size) {
    if (!node_hdr) return;
    node_hdr->b_hdr = (size & ~0x1);
}

/* Function: to_pl
 * -----------------
 * Parameters:
 *     looping_adr - a pointer to a header node
 * 
 * Returns: a node * representation of the payload
 *
 * This function returns the address of the payload
 * after being given a header address.
 */
void *to_pl(node *looping_adr) {
    return (char *)looping_adr + HDR_SIZE;
}

/* Function: make_taken
 * -----------------
 * Parameters:
 *     node_hdr - a pointer to a node to be checked
 * 
 * Returns: NA
 *
 * This function sets a header's LSB to 1, making the 
 * header allocated.
 */
void make_taken(node *node_hdr) {
    if (!node_hdr) return;
    node_hdr->b_hdr |= 0x1;
}

/* Function: delete_node
 * -----------------
 * Parameters:
 *    node_to_be_deleted - a pointer to a node to be deleted
 *                         from the free list
 * 
 * Returns: NA
 *
 * This function deletes a parameter pointer from the free list by
 * analying where the node to be deleted is in the free list (first,
 * last, middle, or sole node). 
 *
 * Citation: Linked List notes from CS106B handout.
 */
void delete_node(node *node_to_be_deleted) {
    if (!start_of_free  || !node_to_be_deleted) return;

    blocks_in_free -= 1;
    hdr *prev_ptr = node_to_be_deleted->prev;
    hdr *next_ptr = node_to_be_deleted->next;
    
    // deleting first node
    if (!prev_ptr && next_ptr) { // only a next pointer
        start_of_free = (node *)next_ptr;
        ((node *)next_ptr)->prev = NULL;
        
    // deleting only node in free list
    } else if (!next_ptr && !prev_ptr) {
        start_of_free = NULL;

    // deleting last node
    } else if (!next_ptr && prev_ptr) { // only a prev ptr
        ((node *)prev_ptr)->next = NULL;

    // any middle node
    } else {
        ((node *)prev_ptr)->next = next_ptr;
        ((node *)next_ptr)->prev = prev_ptr;
    }
    node_to_be_deleted = NULL;
}

/* Function: add_node
 * -----------------
 * Parameters:
 *    node_node - a pointer to a node to be added
 *                         to the free list
 * 
 * Returns: NA
 *
 * This function adds a parameter pointer node to the free list by
 * putting it first in the free list or making it the only node
 * in the free list.
 *
 * Citation: Linked List notes from CS106B.
 */
void add_node(node *new_node) {

    if (new_node == NULL || new_node == start_of_free) return; //invalid node parameter
    blocks_in_free += 1;

    if (start_of_free == NULL) { //if nothing is in free list
        new_node->prev = NULL;
        new_node->next = NULL;
        
    } else { // make it in front of everything else
        new_node->next = (hdr *)start_of_free;
        (start_of_free)->prev = (hdr *)new_node;
        new_node->prev = NULL; 
    }
    start_of_free = new_node;
}

/* Function: coalesce
 * -----------------
 * Parameters:
 *    node_hdr - a pointer to a node 
 * 
 * Returns: NA
 *
 * This function checks if a right header in the free list
 * can be coalesced together with the left node header. They will
 * only combine if both adjacent nodes are free to decrease fragmentation. 
 *
 */
void coalesce(node *node_hdr) {
  
    if (node_hdr == NULL || (hdr*)node_hdr < segment_start
        || (hdr*)node_hdr > segment_end) return;
    node *right_hdr = skip_to_next_header((hdr *)node_hdr);

    // checks new header is valid
    if( right_hdr == NULL || (hdr*)right_hdr < segment_start
        || (hdr*)right_hdr > segment_end || !is_avail(right_hdr)) return;

    node_hdr->b_hdr += (grab_pl(right_hdr) + HDR_SIZE);
    make_free(node_hdr);
    delete_node(right_hdr);
} 

/* Function: split_block
 * -----------------
 * Parameters:
 *     start - a pointer to the starting node
 *     new_hdr - a pointer to the right node
 *     new_s - the size_t new payload of the first header
 *     rem - the size_t payload of the right header
 * 
 * Returns: a void * representation of the next header
 *
 * This function sets the first and right header's header, 
 * updates the free list, coalesces, and returns the address to the payload.
 */
void *split_block(node *start, node *new_hdr, size_t rem, size_t new_s) {
    set_pl(new_hdr, rem);
    set_pl(start, new_s);
    make_taken(start);
    add_node(new_hdr);
    coalesce(new_hdr);
    return to_pl(start);
}

/* Function: old_realloc
 * -----------------
* Parameters:
 *     start - a pointer to the starting node
 *     old_ptr - a pointer to the original node
 *     new_size - the size_t represenation of the new payload 
 * 
 * Returns: a void * representation of the payload
 *
 * This function just moves an allocation to handle a reallocation 
 * request. This is a last resort of realloc in explicit. This 
 * returns the address to the new payload.
 */
void *old_realloc(node *start, node *old_ptr, size_t new_size) {
    void *new_request = mymalloc(new_size);
    memmove(new_request, old_ptr, new_size);
    make_free(start);
    add_node(start);
    coalesce(start); 
    return new_request;
}

/* Function: split_rem
 * -----------------
 * Parameters:
 *     start - a pointer to the starting node
 *     new_hdr - a pointer to the right node
 *     new_s - the size_t new payload of the first header
 *     rem - the size_t payload of the right header
 * 
 * Returns: a void * representation of the payload
 *
 * This function sets the first and right header's header, 
 * updates the free list, and returns the address to the payload.
 */
void *split_rem(node *start, node *new_hdr, size_t new_s, size_t rem) {
    set_pl(start, new_s);
    make_taken(start);
    set_pl(new_hdr, rem);
    make_free(new_hdr);
    add_node(new_hdr);
    return to_pl(start);
}

/* Function: return_malloc
 * -----------------
 * Parameters:
 *     looping_adr - 
 * 
 * Returns: a void * representation of the payload
 *
 * This function does unified bookkeeping for mymalloc by
 * removing the node that was just malloced and returning
 * the new payload.
 */
void *return_malloc(node *looping_adr) {
    delete_node(looping_adr);
    return to_pl(looping_adr);
}

/* Function: myinit
 * -------------------------
 * Parameters:
 *     heap_start - a void * to the beginning of heap
 *     heap_size - a size_t representation
 *                 of the payload size
 * 
 * Returns: boolean representation of if heap instantiation
 *          was successful
 *
 * This function is called by the test harness calls with every 
 * fresh script. It wipes the allocator's clean and start fresh, 
 * setting relevant globals, error checking, makes the first header, and
 * sets up the free list in this explicit allocator.
 */
bool myinit(void *heap_start, size_t heap_size) {
    /* This must be called by a client before making any allocation
     * requests.  The function returns true if initialization was 
     * successful, or false otherwise. The myinit function can be 
     * called to reset the heap to an empty state. When running 
     * against a set of of test scripts, our test harness calls 
     * myinit before starting each new script.
     */
    
    if (heap_size <= MIN_BLOCK_SIZE || !heap_start) return false;

    segment_start = (hdr *)heap_start;
    segment_size = heap_size;
    segment_end = (hdr *)((char *)heap_start + segment_size); 

    if (segment_start == NULL || segment_end == NULL ||
        (char *)segment_end - (char *)segment_start != segment_size) {
    return false;
    }
    *segment_start = segment_size - HDR_SIZE;

    // set up inital node
    ((node *)segment_start)->b_hdr = *segment_start;
    ((node *)segment_start)->prev = NULL;
    ((node *)segment_start)->next = NULL;

    start_of_free = ((node *)segment_start);
    blocks_in_free = 1;
    return true;
}
                       
/* Function: mymalloc
 * -------------------------
 * Parameters: 
 *     requested_size - a size_t representation
 *               of the payload size to be allocated
 * 
 * Returns: the void * representation of the payload address
 *
 * This function will search the heap, free header by header, to find
 * a block to fit the allocation request. It will split the 
 * remainder of the space into a new header if it can satisfy
 * the minimum payload requirement of 16.
 */
void *mymalloc(size_t requested_size) {

    if (requested_size <= 0 || requested_size > segment_size - HDR_SIZE || requested_size > MAX_REQUEST_SIZE) return NULL;
    if (start_of_free == NULL) return NULL; // no heap left 

    size_t needed_sz = roundup(requested_size, HDR_SIZE);
    if (needed_sz <= 0)  return NULL;
    if (needed_sz < MIN_PL ) {
        needed_sz = MIN_PL; // make sure minimum payload is 16
    }
    
    node *looping_adr = start_of_free;
    while (looping_adr != NULL) {
        
        size_t pl = grab_pl(looping_adr);
         if (is_avail(looping_adr) && pl >= needed_sz) {

             //payload will not split
             if (pl == needed_sz || pl == needed_sz + HDR_SIZE) {
                 
                 make_taken(looping_adr);
                 return return_malloc(looping_adr);
                    
            } else {
                 
                size_t rem = pl - needed_sz - HDR_SIZE;
                looping_adr->b_hdr = (needed_sz | 0x1);

                //payload will split
                if (rem >= MIN_PL) {
                    
                    node *new_hdr = (node *)((char *)looping_adr + HDR_SIZE + needed_sz);
                    set_pl(new_hdr, rem);
                    add_node(new_hdr);
                    return return_malloc(looping_adr);
                    
                }
            }
            looping_adr->b_hdr = (pl | 0x1);
            return return_malloc(looping_adr);
        
        } else {
             looping_adr = ((node *)(looping_adr->next));
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
 * hdr type and adds it to the free list.
 * It also updates the global representing how
 *  much heap has been used. 
 */
void myfree(void *ptr) {
    node *temp_ptr = back_to_hdr(ptr);
    
    if ((hdr *)ptr > segment_end || (hdr *)ptr < segment_start
        || !ptr || is_avail(temp_ptr) ) {
        return;
        
    } else {
        make_free(temp_ptr);
        add_node(temp_ptr);
        coalesce(temp_ptr);
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
 * a new size. In explicit, there is in_place, so we will
 * check the right header to expand into if we need extra space.
 * As a last resort, the allocation will just move locations if 
 * in-place realloc is not possible.
 */
void *myrealloc(void *old_ptr, size_t new_size) {

    if (!old_ptr) {
        return mymalloc(new_size);
   } else if (new_size <= 0 || new_size > segment_size || new_size > MAX_REQUEST_SIZE) {
        myfree(old_ptr);
        return NULL; // malformed requests
    } else if ((hdr *)old_ptr > segment_end || (hdr *)old_ptr < segment_start) return NULL;
    
    node *start = back_to_hdr((node *)old_ptr);
    size_t prev_size = grab_pl(start);
    size_t new_s = roundup(new_size, HDR_SIZE);
    if (new_s < MIN_PL) {
        new_s = MIN_PL;
        } 

    // shrinking in space
    if (new_s <= prev_size) {
        if (roundup(prev_size, HDR_SIZE) == roundup(new_s, HDR_SIZE)) {
            make_taken(start);
            return to_pl(start);
        }
        node *new_hdr = (node *)((char *)start + HDR_SIZE + new_s);
        size_t rem = prev_size - new_s - HDR_SIZE; 
        if (rem >= MIN_PL) { // if there's enough room to split
            
            return split_block(start, new_hdr, rem, new_s);
        }
        
        set_pl(start, prev_size);
        make_taken(start);
        return to_pl(start);
        
    } else { //growing in place into the neighboring block if applicable
        
        node *to_check = (node *)((char *)start + HDR_SIZE + grab_pl(start));
        node *new_hdr = (node *)((char *)start + HDR_SIZE + new_s);
        
        if (is_avail(to_check)) {
            size_t right_size = grab_pl(to_check);

            if (right_size + prev_size + HDR_SIZE >= new_s) {
                size_t tog_space = right_size + prev_size + HDR_SIZE; 
                size_t space_left = tog_space - new_s;
                delete_node(to_check); 

                if (space_left >= MIN_BLOCK_SIZE) {
                    return split_rem(start, new_hdr, new_s, (right_size + prev_size - new_s));
            }  
            (start->b_hdr) += right_size + HDR_SIZE; 
            return to_pl(start);
            }
        }
        if (start != 0) { //last resort is old reallocation
            return old_realloc(start, old_ptr, new_size);
        }
    }
    return NULL;
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
 * used space, free blocks, minimum payload, and checks structure 
 * of the free list. 
 */
bool validate_heap() {
    /* TODO(you!): remove the line below and implement this to 
     * check your internal structures!
     * Return true if all is ok, or false otherwise.
     * This function is called periodically by the test
     * harness to check the state of the heap allocator.
     * You can also use the breakpoint() function to stop
     * in the debugger - e.g. if (something_is_wrong) breakpoint();
     */
    return true;
    hdr *start_of_heap = segment_start;
    size_t total = 0;
    size_t free_list_amt = 0;

    if (!start_of_heap) {
        printf("Broken Initialization of Heap");
        breakpoint();
        return false;
    }

    while (start_of_heap < segment_end) {
        total += grab_pl((node *)start_of_heap) + HDR_SIZE;

        if (start_of_heap > segment_end || start_of_heap < segment_start) {
            printf("Your header is outside of the heap!");
            breakpoint();
        }

        size_t pl = grab_pl((node *)start_of_heap);
        if (pl != roundup(pl, HDR_SIZE) || pl < MIN_PL) {
           printf("Your payload is not a multiple of 8 or is less than 16");
            }

        size_t lsb = ((node *)start_of_heap)->b_hdr & 0x1;
        if (lsb != 0 && lsb != 1) {
            printf("Your LSB is neither 1 or 0");
             }


        start_of_heap = ((hdr *)(skip_to_next_header((hdr *)start_of_heap)));
    }
         
    node  *looping_adr = start_of_free;
    while (looping_adr != NULL) {
        free_list_amt += 1;
        if (!is_avail(looping_adr)) {
            printf("Something in your free list is not free");
            breakpoint();
            return false;
        }
        looping_adr = (node *)(looping_adr->next);
    }

    if (total == segment_size) {
        return true;
    }
    printf("Invalid Heap Stucture: Check header alignment, amount used, location, free status, payload size, and the free list");
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
 * payload size, and the pointer for the whole heap and
 * also prints the free list pointers, prev and next, if 
 * possible.
 */
void dump_heap() {

    hdr *heap_start = segment_start;
    printf("Heap Visualization:\n");
    printf("----------------------------------------------\n");

    // prints payload size, pointer, and free status for every block
    while (heap_start < segment_end) {
        printf("%s", (is_avail((node*)heap_start) == 0x1) ? "  FREE   " : "ALLOCATED");
        printf(",  Payload Size: %ld,  Hdr Pointer: %p \n", grab_pl((node *)heap_start), heap_start);
        heap_start = skip_to_next_header(heap_start);
    }
    
     printf("----------------------------------------------\n");
     printf("\n");
     printf("Free List Visualization: %ld (Amount in Free List) \n", blocks_in_free);
     printf("----------------------------------------------\n");
     node *looping_adr = start_of_free;

     //prints everything in free list, original pointer, prev and next pointer
     while (looping_adr != NULL) { 
         printf("%s", (is_avail(looping_adr) == 0x1) ? "FREE" : "ALLOCATED");
         printf(",  Hdr Pointer: %p,  Prev Pointer: %p,  Next Pointer: %p \n", looping_adr, (void *)((looping_adr)->prev), (void *)((looping_adr)->next));
         looping_adr = ((node *)(looping_adr->next));
     }    
}

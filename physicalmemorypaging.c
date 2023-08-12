#include "physicalmemory.hh"
#include "virtualmemoryregion.hh"
#include <iostream>

using namespace std;


PhysicalMemory::PhysicalMemory(size_t npages) : PhysMem(npages) {

    vec.resize(npages);
    clock_hand = 0;
}

PPage PhysicalMemory::get_new_ppage(VPage mapped_page, 
    VirtualMemoryRegion *owner) {

    while (nfree() == 0) { // if no physical pages are free, throws out a page using the clock
        if (vec[clock_hand].inner_owner->clock_should_remove(vec[clock_hand].page)) {

            vec[clock_hand].inner_owner->clock_remove(vec[clock_hand].page);
            clock_hand = (clock_hand + 1) % vec.size();
            break;
        } else {
            vec[clock_hand].inner_owner->clock_sweep(vec[clock_hand].page);
        }
        clock_hand = (clock_hand + 1)%vec.size();
    }
    
    assert(nfree() > 0);
    PPage pg = page_alloc();
    assert(pg);
    int index = ( (pg - pool_base() ) / get_page_size() );
    vec[index] = {mapped_page, owner};
    return pg;

}

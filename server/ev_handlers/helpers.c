#include "ev_handlers.h"

void find_mapped_machines(pid_t token, unsigned long pfn, struct mapped_page* entry) {
    struct vaddr_arg argument = {
        .pfn=pfn,
        .token=token
    };
    foreach_mapped_page(mapped_page_lookup_callback, (void*) (&entry), (void*) (&argument)); 
}

void mapped_page_lookup_callback(void* current_entry, void* found, void* arg) {
    struct mapped_page* curr = (struct mapped_page*)current_entry;
    struct vaddr_arg* argument = (struct vaddr_arg*)arg;

    if(curr->pfn == argument->pfn && curr->token == argument->token) {
        struct mapped_page** temp = (struct mapped_page**)found;
        *temp = (struct mapped_page*) current_entry;
    }
}

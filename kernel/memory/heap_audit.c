#include "heap_audit.h"
#include "heap.h"
#include "../log/logger.h"
void heap_audit_init(void) {
    klog(LOG_WARNING, "Only allocation smoke tests are available; global leak tracking is not implemented");
}
int heap_audit_verify_allocations(void) {
    void* a = kmalloc(1024);
    void* b = kmalloc(2048);
    if (!a || !b) {
        if (a) kfree(a);
        if (b) kfree(b);
        klog(LOG_ERROR, "Heap allocation smoke test failed");
        return 0;
    }
    kfree(a); kfree(b);
    klog(LOG_INFO, "Heap allocation/free smoke test passed; global leak count was not measured");
    return 1;
}
uint32_t heap_audit_get_used_bytes(void) {
    return 0xFFFFFFFFu; /* Explicitly unavailable; no heap accounting exists. */
}

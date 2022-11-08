#include "GCTest.hpp"
#include <stdio.h>

using namespace RTGC;
RTGC::AnchorListPool RTGC::g_anchorListPool;

ShortOOP _oop(int ptr) {
    return *(ShortOOP*)&ptr;
}

void show_list(ReferrerList* list) {
    int idx = 0;
    NodeIterator<false> iter;
    iter.initVectorIterator(list);
    for (; iter.hasNext(); ) {
        ShortOOP oop = iter.next();
        printf("%d) %d\n", idx++, *(int32_t*)&oop);
    }
    printf("==================");
}

int main(int argc, const char* args[]) {
    precond(sizeof(ShortOOP) == 4);
    precond(sizeof(ReferrerList) == 32);
    g_anchorListPool.initialize();
    ReferrerList* list = (ReferrerList*)g_anchorListPool.allocate();
    list->init();
    for (int i = 1; i < 4; i ++) {
        list->add(_oop(i));
        show_list(list);
    }
}

#ifdef _MSC_VER
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
#endif

#define _USE_JVM 0
#define _USE_MMAP 1
#define _ULIMIT 1
void* RTGC::VirtualMemory::reserve_memory(size_t bytes) {
    precond(bytes % MEM_BUCKET_SIZE == 0);
    void* addr;    
#if _USE_JVM
    size_t total_reserved = bytes;
    size_t page_size = os::vm_page_size();
    size_t alignment = page_size;
    ReservedHeapSpace total_rs(total_reserved, alignment, page_size, NULL);
    addr = total_rs.base();// nullptr;
#elif defined(_MSC_VER)
    addr = VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_READWRITE);
#elif _USE_MMAP
    addr = mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
#else
    addr = malloc(bytes);
#if _ULIMIT    
    addr = realloc(addr, 4096);
#endif
#endif
    postcond(addr != NULL);
    //rtgc_log(false, "reserve_memory %p %dk\n", addr, (int)(bytes/1024));
    return addr;
}

void RTGC::VirtualMemory::commit_memory(void* addr, void* bucket, size_t bytes) {
    precond(bytes % MEM_BUCKET_SIZE == 0);
#if _USE_JVM
    rtgc_log(1, "commit_memory\n");
    return;
#elif defined(_MSC_VER)
    addr = VirtualAlloc(bucket, MEM_BUCKET_SIZE, MEM_COMMIT, PAGE_READWRITE);
    if (addr != 0) return;
#elif _USE_MMAP
    int res = mprotect(bucket, bytes, PROT_READ|PROT_WRITE);
    //rtgc_log(false, "commit_memory mprotect %p:%p %d res=%d\n", addr, bucket, (int)(bytes/1024), res);
    if (res == 0) return;
#elif _ULIMIT    
    void* mem = ::realloc(addr, offset + bytes);
    if (mem == addr) return;
#endif
    //assert(0, "OutOfMemoryError:E009");
}

void RTGC::VirtualMemory::free(void* addr, size_t bytes) {
    precond(bytes % MEM_BUCKET_SIZE == 0);
#if _USE_JVM
    fatal(1, "free_memory\n");
    return;
#elif defined(_MSC_VER)
    addr = VirtualFree(addr, bytes, MEM_RELEASE);
    if (addr != 0) return;
#elif _USE_MMAP
    int res = munmap(addr, bytes);
    if (res == 0) return;
#else    
    ::free(addr);
    return;
#endif
    //assert(0, "Invalid Address:E00A");
}

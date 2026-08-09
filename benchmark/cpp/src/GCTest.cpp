#include "GCTest.hpp"

#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>
#include <unistd.h>

struct RCObj {
    int idx;
    atomic_int refCount;
};

const int CNT_OBJ = 2048 * 10;
const int TEST_THREAD = 20;
const int TEST_LOOP = 1000 * 10;

struct Container {
    RCObj* obj;
};

Container sharedObj;

#define HOOK_ADDR 0

__attribute__((noinline)) RCObj* replace_in_heap(RCObj** addr, RCObj* value) {
    while (true) {
        RCObj* old = *addr;
        int old_rc = old == NULL ? -1 : 
            (HOOK_ADDR ? old->refCount : atomic_load_explicit(&old->refCount, memory_order::memory_order_acquire));

#if HOOK_ADDR            
        if ((RCObj*)atomic_load_explicit((atomic_intptr_t*)addr, memory_order::memory_order_acquire) != old) {
            continue;
        }
        atomic_store_explicit((atomic_intptr_t*)addr, (intptr_t)value, memory_order::memory_order_release);
#else
        if (*addr != old) continue;
        *addr = value;
#endif

        if (old_rc >= 0) {
            if (old_rc == 0) {
                printf("addr: %p old: %p\n", addr, old);
                assert(old_rc > 0);
            }
#if HOOK_ADDR            
            old->refCount = old_rc - 1;
#else 
            atomic_store_explicit(&old->refCount, old_rc - 1, memory_order::memory_order_release);
#endif
        }
        return old;
    }
}

void replace_not_in_heap(RCObj** addr, RCObj* value) {
    RCObj* old = atomic_exchange_explicit((_Atomic(RCObj*)*)addr, value, memory_order::memory_order_seq_cst);
    if (old != value) {
        atomic_fetch_add(&value->refCount, 1);
        if (old != NULL) {
            atomic_fetch_sub(&old->refCount, 1);
        }
    }
}

atomic_int cntTest = 0;
void* testLoop(void* argument) {
    RCObj rcObjs[CNT_OBJ];
    const bool use_usleep = true;

    atomic_fetch_add(&cntTest, 1);
    for (int k = 0; k < TEST_LOOP; k ++) {
        for (int i = 0; i < CNT_OBJ; i++) {
            rcObjs[i].refCount = 1;
            replace_in_heap(&sharedObj.obj, &rcObjs[i]);
        }
        replace_in_heap(&sharedObj.obj, nullptr);
    }
    atomic_fetch_sub(&cntTest, 1);
    while (cntTest > 0) {
        usleep(1000);
    }
}



void doTestOverDecrement() {
    RCObj rcObjs[CNT_OBJ];
    sharedObj.obj = NULL;
    for (int i = 0; i < CNT_OBJ; i ++) { 
        rcObjs[i].idx = i;
        rcObjs[i].refCount = 0;        
    }

    printf("a_int32: %d, int32 %d\n", (int)sizeof(_Atomic(int32_t)), (int)sizeof(int32_t));

    
    pthread_t threads[TEST_THREAD];

    for (int i = 0; i < TEST_THREAD; i ++) {
        pthread_create(&threads[i], NULL, testLoop, rcObjs);
    }
    SimpleTimer timer;
    timer.reset();
    void* thread_result;
    for (int i = 0; i < TEST_THREAD; i ++) {
        pthread_join(threads[i], &thread_result);
    }
    int elapsed = timer.reset();

    for (int i = 0; i < CNT_OBJ; i ++) { 
        if (rcObjs[i].refCount < 0) {
            printf("%d: %d\n", i, rcObjs[i].refCount);        
        }
    }
    printf ("[tp:%d] elapsed: %d collision: %d\n", 0, elapsed, 0);

    // 단일 쓰레드 (no collision) Test 결과.
    // [unsafe]          elapsed:  8352 collision: 0
    // [acquire/release] elapsed: 11362 collision: 0
    // [atomic_exchange] elapsed: 27345 collision: 0    
}

int main(int argc, const char* args[]) {

    doTestOverDecrement();
    return 0;

    StressTestOptions options;
    options.basePayload = atoi(args[1]); 
    options.primitiveBytes = atoi(args[2]); 
    options.circularRefPercent = atoi(args[3]);
    options.replacePercent = atoi(args[4]);

    testNoGCPerformance(&options);
    return 0;
}


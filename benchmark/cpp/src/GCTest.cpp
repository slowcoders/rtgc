#include "GCTest.hpp"

#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>
#include <unistd.h>

struct RCObj {
    int idx;
    int cntIncrement;
    atomic_int refCount;
};

const int CNT_OBJ = 2048;
const int TEST_LOOP = 100*1000;

struct Container {
    RCObj* obj;
};

volatile Container sharedObj;

void* testLoop_noExhg(void* argument) {
    RCObj* rcObjs = (RCObj*)argument;
    for (int i = 0; i < CNT_OBJ; i ++) { 
        rcObjs[i].idx = i;
        rcObjs[i].cntIncrement = 0;
        rcObjs[i].refCount = 0;        
    }

    for (int k = 0; k < TEST_LOOP; k ++) {
        for (int i = 0; i < CNT_OBJ; i ++) {
            while (true) {
                RCObj* old = sharedObj.obj;
                int old_rc = old == NULL ? -1 : atomic_load_explicit(&old->refCount, memory_order::memory_order_acquire);
                if (sharedObj.obj != old) {
                    rcObjs[0].idx ++;
                    continue;
                }

                RCObj* b = rcObjs + i;
                b->refCount = 0;
                sharedObj.obj = b;
                if (old_rc >= 0) {
                    assert(old_rc == 0);
                    atomic_store_explicit(&old->refCount, old_rc + 1, memory_order::memory_order_release);
                }
                break;
            }
        }
        sharedObj.obj = NULL;
        // assign_ref(&dummy);
        for (int i = 0; i < CNT_OBJ; i ++) { 
            if (rcObjs[i].refCount > 1) {
                assert(rcObjs[i].refCount <= 1);
                printf("over increment found!!\n");
            }
            // rcObjs[i].refCount = 0;
        }
    }
}

void* testLoop_exhg(void* argument) {
    RCObj* rcObjs = (RCObj*)argument;
    for (int i = 0; i < CNT_OBJ; i ++) { 
        rcObjs[i].idx = i;
        rcObjs[i].cntIncrement = 0;
        rcObjs[i].refCount = 0;        
    }

    for (int k = 0; k < TEST_LOOP; k ++) {
        for (int i = 0; i < CNT_OBJ; i ++) {
            while (true) {
                RCObj* b = rcObjs + i;
                b->refCount = 0;
                RCObj* old = atomic_exchange_explicit((_Atomic(RCObj*)*)&sharedObj.obj, b, memory_order::memory_order_seq_cst);
                if (old != NULL) {
                    old->refCount ++;
                }
                break;
            }
        }
        sharedObj.obj = NULL;
        // assign_ref(&dummy);
        for (int i = 0; i < CNT_OBJ; i ++) { 
            // Fail: Over Decrement 발생!!
            // assert(rcObjs[i].refCount <= 1);
        }
    }
}

void doTestOverDecrement() {
    RCObj tc0[CNT_OBJ];
    RCObj tc1[CNT_OBJ];
    RCObj tc2[CNT_OBJ];
    RCObj tc3[CNT_OBJ];

    for (int type = 0; type < 2; type ++) {
        sharedObj.obj = NULL;

        pthread_t t0, t1, t2, t3;
        void * (* thread_main)(void *) = type == 1 ? testLoop_noExhg : testLoop_exhg;
        pthread_create(&t0, NULL, thread_main, tc0);
        pthread_create(&t1, NULL, thread_main, tc1);
        pthread_create(&t2, NULL, thread_main, tc2);
        pthread_create(&t3, NULL, thread_main, tc3);
        
        SimpleTimer timer;
        timer.reset();
        void* thread_result;
        pthread_join(t0, &thread_result);
        pthread_join(t1, &thread_result);
        pthread_join(t2, &thread_result);
        pthread_join(t3, &thread_result);
        int elapsed = timer.reset();
        printf ("[tp:%d] elapsed: %d collision: %d\n", type, elapsed, tc0->idx + tc1->idx + tc2->idx + tc3->idx);
        tc0->idx = tc1->idx = tc2->idx = tc3->idx = 0;
    }

    // 단일 쓰레드 (no collision) Test 결과.
    // [atomic_exchange]    elapsed: 27378 collision: 0
    // [acquire/release]    elapsed: 15961 collision: 0    
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


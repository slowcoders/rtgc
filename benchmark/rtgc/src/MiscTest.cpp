// #include "GCNode.hpp"
// #include <thread>

// using namespace RTGC;

// static int _cntAllocatedNodes = 0;
// static int gCntDealloc = 0;

// class TestObj2 : public GCObject {
//   public:
//   const char* _id;
//   TestObj2* init$(const char* id) {
//     this->_id = id;
//     _cntAllocatedNodes ++;
//     //GCObject::onReplaceRootVariable(this, nullptr);
//     return this;
//   }
  
//   ~TestObj2() {
//     gCntDealloc ++;
//   }
// #define PP_CLASS TestObj2
// #include "preprocess/BeginFields.hpp"
//     #include "CircuitTest_Node.hpp"
// #include "preprocess/EndDefine.hpp"
// };

// #include "preprocess/BeginScanInfo.hpp"
//     #include "CircuitTest_Node.hpp"
// #include "preprocess/EndDefine.hpp"
// #include "preprocess/BeginStatic.hpp"
//     #include "CircuitTest_Node.hpp"
// #include "preprocess/EndDefine.hpp"
// #undef PP_CLASS

// #if 0
// int set_thread_affinity(int core_id) {
//   int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
//   cpu_set_t cpus;
//   CPU_ZERO(&cpus);

//   int core = core_id < 0 ? ~core_id : core_id;
//   if (core >= num_cores) {
//     return EINVAL;
//   }

//   if (core == core_id) {
//     CPU_SET(core, &cpus);
//   }
//   else {
//     for (int i = 0; i < num_cores; i ++) {
//       if (core != i) {
//         CPU_SET(i, &cpus);
//       }
//     }
//   }

//   pthread_t current_thread = pthread_self();    
//   int rs = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpus);
//   return rs;
// }
// #endif

// struct VarReplace {
//     GCObject* owner;
//     GCObject* assigned;
//     GCObject* erased;
// };

// template <class T, int size>
// class BufferedPipe {
// protected:  
//   int64_t _readPos;
//   //int dummy1;
//   int64_t _writePos;
//   //int dummy2;
//   T _q[size];

// public:  
//   BufferedPipe() { 
//     _readPos = _writePos = 0; 
//     cntPush = 0;
//   }
  
//   void push(const T& v) {
//     int pos = _writePos;
//     _q[pos] = v;
//     shift_write_pos(pos);
//   } 
  
//   T& pop() {
//     int pos = wait_readable();
//     T& v = _q[pos];
//     _readPos = (pos + 1) % size;
//     return v;
//   }

//   void clear() {
//     _readPos = _writePos;
//   }

// protected:
// int cntPush;
//   void shift_write_pos(int pos) {  
//     pos = (pos + 1) % size;
//  cntPush ++;
 
//     if (pos == _readPos) {
//         int cntSpin = 0;
//       while (pos == ((std::atomic<int>*)&_readPos)->load()) {
//         if (++cntSpin % 100 == 0) {
//           printf("-- %d\n", cntSpin);
//         }
//         //pthread_yield();
//       }
//     }
//     _writePos = pos;
//   }
//   int wait_readable() {
//     int pos = _readPos;
//     while (pos == ((std::atomic<int>*)&_writePos)->load()) {
//         //pthread_yield();
//     }
//     return pos;
//   }
// };

// class VarReplaceInfoQ : public BufferedPipe<VarReplace, 4096>{
// public:
//   void NO_INLINE push(GCObject* owner, GCObject* assigned, GCObject* erased) {
//     int64_t pos = _writePos;
//     VarReplace vci = _q[pos];
//     vci.owner = owner;
//     vci.assigned = assigned;
//     vci.erased = erased;
//     //BufferedPipe::push(vci);
//     shift_write_pos((int)pos);
//   }
// };

// //__thread 
// VarReplaceInfoQ* tlsRefQ;


// struct MiscTest {

//   static void* alloc(size_t, size_t) {
//     return RTGC_NEW(TestObj2, ("AAA"));
//   }
  
//   static void dealloc(void* obj) {
//     delete (TestObj2*)obj;
//   }

//   TestObj2* root1;
//   TestObj2* root2;
//   TestObj2* root3;
//   volatile int cntLoad;

//   MiscTest() {
//     count = 1;
//     root1 = RTGC_NEW(TestObj2, ("root-1"));
//     root2 = RTGC_NEW(TestObj2, ("root-2"));
//     root3 = RTGC_NEW(TestObj2, ("root-3"));
//     GCRuntime::onReplaceRootVariable(root1, nullptr);
//     GCRuntime::onReplaceRootVariable(root2, nullptr);
//     GCRuntime::onReplaceRootVariable(root3, nullptr);
//   }

//   int count;

//   void test_misc() {
//     if (false) {
//       // 290ms (load only: 15ms);
//       for (int i = 0; i < 20; i ++) {
//         int x = ((std::atomic<int>*)&count)->load();
//         //cntLoad += x;
//         *((std::atomic<int>*)&count) = x + 1;
//       }
//     }

//     if (false) {
//       // 81ms. without TLS Buffer : 300ms
//       for (int i = 0; i < 20; i ++) {
//         tlsRefQ->push(root1, root2, root3);
//       }
//       tlsRefQ->clear();
//     }

//     if (false) {
//       // 650ms (without circuit detection : 305ms)
//       for (int i = 0; i < 20; i ++) {
//         root1->set_var1(root2);
//         root1->set_var1(nullptr);
//       }
//     }

//     if (false) {
//       // 1240ms (without circuit detection : 695ms)
//       for (int i = 0; i < 20; i ++) {
//         root1->set_var1(root2);
//         root1->set_var2(root3);
//         root1->set_var1(nullptr);
//         root1->set_var2(nullptr);
//       }
//     }

//     if (false) {
//       // 80ms (std:atomic 사용시 : 250ms)

//       for (int i = 0; i < 20; i ++) {
//         if (false) {
//           GCRuntime::onAssignRootVariable(root1);
//           GCRuntime::onEraseRootVariable(root1);
//         }
//         else {
//           root1->incrementRootRefCount();
//           root1->decrementRootRefCount();
//         }
//       }
//     }

//     if (false) {
//       // 860ms
//       void*(* volatile allocFoo_)(size_t, size_t) = MiscTest::alloc;
//       void(* volatile freeFoo_)(void*) = MiscTest::dealloc;
//       void* objs[20];

//       for (int i = 0; i < 20; i ++) {
//         objs[i] = allocFoo_(sizeof(TestObj2), 1);
//       }
//       for (int i = 0; i < 20; i ++) {
//         freeFoo_(objs[i]);
//       }
//     }    
//   }

// };




// void test_misc() {
// #if 0
//     tlsRefQ = new VarReplaceInfoQ();
//     std::thread myThread([](){ 
//       set_thread_affinity(0);
//       while (true) {
//         tlsRefQ->pop();
//       }
//     });
//     myThread.detach();
//     set_thread_affinity(~0);
//     usleep(100*1000);
// #endif

//   int cntObj = 1600 * 1000;
//   MiscTest test;
//   // int increment = 100 * 1000;
//   // for (int i = 0; i < 6; i ++) {
//     test.test_misc();
//   //   cntObj += increment;
//   // }
//   //usleep(100*1000*1000);
// }




// #define USE_NEW false
// #define USE_MALLOC true

// const int MAX_OBJ_CNT = 1024 * 1024 / 10; // 적당한 크기로 변경하세요.
// class Foo {
// public:    
//     int8_t values[1024 * 4];
//      ~Foo() {}
//     void* operator new(size_t size) {
//         return malloc(size);
//     }
//     void operator delete(void* ptr) {
//         return free(ptr);
//     }
// };
// //typedef struct { int values[1024]; } Foo;
// Foo* memPool;
// Foo* freeItems[MAX_OBJ_CNT];
// int cntFree = MAX_OBJ_CNT;
// void initMemPool() {
//    memPool = (Foo*)malloc(sizeof(Foo) * MAX_OBJ_CNT);
//    for (int i = 0; i < MAX_OBJ_CNT; i++) {
//       freeItems[i] = memPool + i;
//    }
// }
// void markFoo(Foo* foo) { foo->values[0] = cntFree; }
// Foo* allocFoo() {
//     if (USE_NEW) return new Foo();
//    return USE_MALLOC ? (Foo*)malloc(sizeof(Foo))
//       : freeItems[--cntFree];
// }
// void freeFoo(Foo* foo) {
//     if (USE_NEW) {
//         delete foo;
//     }
//     else if (USE_MALLOC) {
//         free(foo);
//     }
//     else {
//         freeItems[cntFree++] = foo;
//     }
// }

// Foo*(* volatile allocFoo_)() = allocFoo;
// void(* volatile markFoo_)(Foo*) = markFoo;
// void(* volatile freeFoo_)(Foo*) = freeFoo;
// void testA() {
//    for (int i = 0; i < MAX_OBJ_CNT; i ++) {
//       Foo* foo = allocFoo_(); markFoo_(foo); freeFoo_(foo);
//    }
// }
// void testB() {
//    Foo** items = (Foo**)malloc(sizeof(Foo*) * MAX_OBJ_CNT);
//    for (int i = 0; i < MAX_OBJ_CNT; i ++) items[i] = allocFoo_();
//    for (int i = 0; i < MAX_OBJ_CNT; i ++) {
//       Foo* foo = items[i];
//       markFoo_(foo); freeFoo_(foo);
//    }
// }

// void execTreeTest() {
//     SimpleTimer timer;

//     timer.reset();
//     timer.reset();
//     testA();
//     printf("testA elapsed: %d\n", timer.reset());
//     testA();
//     printf("testA elapsed: %d\n", timer.reset());
//     testA();
//     printf("testA elapsed: %d\n", timer.reset());
//     testB();
//     printf("testB elapsed: %d\n", timer.reset());
//     testB();
//     printf("testB elapsed: %d\n", timer.reset());
//     testB();
//     printf("testB elapsed: %d\n", timer.reset());

// }

// struct MemBucket {
//   MemBucket* next;
//   char dummy[1024-sizeof(void*)];
// };
// static const int _1M = 1024*1024;
// void checkMaxMemory() {
//   int idx = 0;
//   MemBucket* root = nullptr;
//   for (;; idx++) {
//     MemBucket* mem = (MemBucket*)malloc(sizeof(MemBucket));
//     if (mem == nullptr) break;
//     if ((idx % 4096) == 0) {
//       printf("mem size: %dM\n", idx * (int)sizeof(MemBucket) / _1M);
//     }
//     mem->next = root;
//     root = mem;
//   }
//   for (; root != nullptr;) {
//     MemBucket* mem = root;
//     root = mem->next;
//     free(mem);
//   }
//   printf("maxMemory: %dM\n", idx);

// }

// static void testAllocFreeSpeed(int extraSpace) {
//   int TEST_NODE_COUNT = 1024*1024;
//   GCObject** baggages = (GCObject**)rtgc_calloc(sizeof(GCObject*), TEST_NODE_COUNT);

//   SimpleTimer timer;
//   timer.reset();
//   for (int i = 0; i < TEST_NODE_COUNT; i++) {
//     baggages[i] = GCPrimitiveArray<char>::alloc(1024);
//   }
//   for (int i = 0; i < TEST_NODE_COUNT; i++) {
//     delete baggages[i];
//   }
//   int init_time = timer.reset();

//   for (int i = 0; i < TEST_NODE_COUNT; i++) {
//     baggages[i] = GCPrimitiveArray<char>::alloc(1024);
//   }
//   int alloc_time = timer.reset();

//   for (int i = 0; i < TEST_NODE_COUNT; i++) {
//     delete baggages[i];
//   }
//   int gc_time = timer.reset();
//   free(baggages);

//   printf("-------------\n");
//   printf("Memory manager test %dM nodes.\n", TEST_NODE_COUNT/_1M);
//   //printf(" - Test initialization: %d ms\n", init_time);
//   printf(" - Allocation: %d ms\n", alloc_time);
//   printf(" - Deletion: %d ms\n", gc_time);
//   printf(" - Total elaspsed: %d ms\n", (alloc_time + gc_time));
//   printf("-------------\n\n");
// }
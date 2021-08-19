#include "GCTest.hpp"

using namespace RTGC;

class TestObj;
static int cntObjs = 0;
static TestObj* objs[1024];
static const char* objNames[1024];
void dumpTestResults();

class TestObj : public GCObject {
friend struct TestSet;    
public:

#if !GC_DEBUG
    const char* _id;
#endif
    int _idx;

    TestObj* init$(const char* id) {
        this->_id = id;
        this->_idx = cntObjs;
        objs[cntObjs] = this;
        objNames[cntObjs] = id;
        cntObjs++;
        // for test purpose only;
        decrementRootRefCount();
        return this;
    }

    TestObj* init$(int id) {
        return init$((const char*)(intptr_t)id);
    }

    ~TestObj() {
        // printf("-- %s deleted\n", _id);
        objs[this->_idx] = nullptr;
    }
#define PP_CLASS TestObj
#include "preprocess/BeginFields.hpp"
    #include "CircuitTest_Node.hpp"
#include "preprocess/EndDefine.hpp"
};

#include "preprocess/BeginScanInfo.hpp"
    #include "CircuitTest_Node.hpp"
#include "preprocess/EndDefine.hpp"
#include "preprocess/BeginStatic.hpp"
    #include "CircuitTest_Node.hpp"
#include "preprocess/EndDefine.hpp"
#undef PP_CLASS

enum CreateFlag {
    DerivedTracks = 1,
    SpareTracks = 2,
    All = 3,
};

struct TestSet {
    TestObj* A1;
    TestObj* A2;
    TestObj* A3;
    TestObj* A4;
    TestObj* A5;

    TestObj* B1;
    TestObj* B2;

    TestObj* K1;
    TestObj* K2;

    TestObj* X1;
    TestObj* X2;

    TestObj* C1;
    TestObj* C2;
    TestObj* C3;
    TestObj* C4;
    TestObj* C5;
    TestObj* C6;
    TestObj* C7;

    TestObj* D1;
    TestObj* E1;
    TestObj* F1;
    TestObj* G1;
    TestObj* H1;

    TestObj::PTR holder;

    TestSet(CreateFlag flags = DerivedTracks) {
        cntObjs = 0;

		A1 = (new TestObj())->init$("A1");
		A2 = (new TestObj())->init$("A2");
		A3 = (new TestObj())->init$("A3");
		A4 = (new TestObj())->init$("A4");
		A5 = (new TestObj())->init$("A5");
 
		B1 = (new TestObj())->init$("B1");
 		B2 = (new TestObj())->init$("B2");

        C1 = (new TestObj())->init$("C1");
		C2 = (new TestObj())->init$("C2");
		C3 = (new TestObj())->init$("C3");
		C4 = (new TestObj())->init$("C4");
		C5 = (new TestObj())->init$("C5");
		C6 = (new TestObj())->init$("C6");
		C7 = (new TestObj())->init$("C7");

 		K1 = (new TestObj())->init$("K1");
 		K2 = (new TestObj())->init$("K2");

        connect(A1, A2);
        connect(A2, A3);
        connect(A3, A4);
        connect(A4, A5);
        connect(A5, A1);

        connect(A1, B1);
        connect(B1, B2);
        connect(B2, B1);

        connect(B2, C1);
        connect(C1, C2);
        connect(C2, C3);
        connect(C3, C4);
        connect(C4, C5);
        connect(C5, C6);
        connect(C6, C7);
        connect(C7, K1);

        connect(K1, K2);
        connect(K2, K1);
        connect(K2, A4);

        holder = (new TestObj())->init$("Holder");
        holder->set_var1(A5);

        if (flags & CreateFlag::DerivedTracks) {
            D1 = (new TestObj())->init$("D1");
            E1 = (new TestObj())->init$("E1");
            F1 = (new TestObj())->init$("F1");
            G1 = (new TestObj())->init$("G1");
            H1 = (new TestObj())->init$("H1");

            connect(C2, D1);
            connect(D1, C1);

            connect(C2, E1);
            connect(E1, C3);

            connect(E1, F1);
            connect(F1, C4);

            connect(C6, G1);
            connect(G1, C5);

            connect(C7, H1);
            connect(H1, G1);
        }

        if (flags & CreateFlag::SpareTracks) {
            X1 = (new TestObj())->init$("X1");
            X2 = (new TestObj())->init$("X2");

            connect(A1, X1);
            connect(X1, A2);

            connect(A4, X2);
            connect(X2, A3);

            connect(C3, C5);
        }
    }


    void connect(TestObj* anchor, TestObj* link) {
        if (anchor->_var1.getPointer() == nullptr) {
            anchor->set_var1(link);
        }
        else if (anchor->_var2.getPointer() == nullptr) {
            anchor->set_var2(link);
        }
        else if (anchor->_var3.getPointer() == nullptr) {
            anchor->set_var3(link);
        }
        else {
            throw "something wrong";
        }
    }

    void disconnect(TestObj* anchor, TestObj* link) {
        printf("===============================\n");
        printf("Disconnected: %s -> %s\n", anchor->_id, link->_id);
        if (anchor->_var1.getPointer() == link) {
            anchor->set_var1(nullptr);
        }
        else if (anchor->_var2.getPointer() == link) {
            anchor->set_var2(nullptr);
        }
        else if (anchor->_var3.getPointer() == link) {
            anchor->set_var3(nullptr);
        }
        else {
            throw "something wrong";
        }
    }

    static void testCircularTree() {
        TestObj::PTR root = createCircularTree(nullptr, 0, 0);
    }

    static TestObj* createCircularTree(TestObj* parent, int layer, int id) {
        TestObj* obj = (new TestObj())->init$(id);
        obj->set_parent(parent);
        if (++layer < 3) {
            id = id << 8;
            obj->set_var1(createCircularTree(obj, layer, id + 1));
            obj->set_var2(createCircularTree(obj, layer, id + 2));
            obj->set_var3(createCircularTree(obj, layer, id + 3));
        }
        return obj;
    }

    static void testCircularTree2() {
        TestObj::PTR root = createCircularTree2(nullptr, nullptr, 0, 0);
    }

    static TestObj* createCircularTree2(TestObj* root, TestObj* parent, int layer, int id) {
        TestObj* obj = (new TestObj())->init$(id);
        if (root == nullptr) {
            root = obj;
        }
        obj->set_parent(root);
        if (++layer < 3) {
            id = id << 8;
            obj->set_var1(createCircularTree2(root, obj, layer, id + 1));
            obj->set_var2(createCircularTree2(root, obj, layer, id + 2));
            obj->set_var3(createCircularTree2(root, obj, layer, id + 3));
        }
        return obj;
    }

    static void testCircularTree3() {
        TestObj::PTR root = createCircularTree3(nullptr, 0, 1);
    }

    static TestObj::PTR createCircularTree3(TestObj* parent, int layer, int id) {
        TestObj::PTR obj = (new TestObj())->init$(id);
        if (++layer < 3) {
            id = id << 4;
            TestObj::PTR loop1 = (new TestObj())->init$(id + 1);
            TestObj::PTR loop2 = (new TestObj())->init$(id + 2);
            TestObj::PTR loop3 = (new TestObj())->init$(id + 3);
            obj->set_var1(loop1);
            loop1->set_var1(loop2);
            loop2->set_var1(loop3);
            loop3->set_parent(obj);

            id = id << 4;
            loop2->set_var1(createCircularTree3(loop2, layer, id + 1));
            loop2->set_var2(createCircularTree3(loop2, layer, id + 2));
            loop2->set_var3(createCircularTree3(loop2, layer, id + 3));
        }
        return obj;
    }

};

void dumpTestResults() {
    printf("-------------------------------\n");
    for (int i = 0; i < cntObjs; i++) {
        printf("%s: ", objNames[i]);

        GCObject* node = objs[i];
        if (node == nullptr) {
            printf("<deleted>\n");
            continue;
        }

        if (node->inCircuit()) {
            printf("Circuit-%d, track=%d, prev=%s, next=%s\n", 
                node->getParentId(), node->getTrackId(), 
                ((TestObj*)node->getPrevNodeInTrack())->_id, 
                ((TestObj*)node->getNextNodeInTrack())->_id);
        }
        else {
            printf("not circular ref\n");
        }
    }
}


void testCircuitScanner() {
    // {
    //     TestSet::testCircularTree3();
    // }
    // {
    //     TestSet::testCircularTree2();
    // }
    // {
    //     TestSet::testCircularTree();
    // }
    {
        TestSet ts(CreateFlag::SpareTracks);
		dumpTestResults();
    }
    {
        TestSet ts(CreateFlag::SpareTracks);
		ts.disconnect(ts.A1, ts.A2);
		dumpTestResults();
    }
    {
        TestSet ts(CreateFlag::SpareTracks);
		ts.disconnect(ts.A2, ts.A3);
		dumpTestResults();
    }
    {
        TestSet ts(CreateFlag::SpareTracks);
		ts.disconnect(ts.C3, ts.C4);
		dumpTestResults();
    }
    {
        TestSet ts;
		ts.disconnect(ts.C1, ts.C2);
		dumpTestResults();
    }
    {
        TestSet ts;
		ts.disconnect(ts.C2, ts.C3);
		dumpTestResults();
    }
    {
        TestSet ts;
        ts.disconnect(ts.C3, ts.C4);
        dumpTestResults();
    }
    {
        TestSet ts;
        ts.disconnect(ts.C4, ts.C5);
        dumpTestResults();
    }
    {
        TestSet ts;
		ts.disconnect(ts.C5, ts.C6);
		dumpTestResults();
    }
    {
        TestSet ts;
        ts.disconnect(ts.C6, ts.C7);
        dumpTestResults();
    }
    exit(0);
}


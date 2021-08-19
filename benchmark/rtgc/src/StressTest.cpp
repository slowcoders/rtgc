#include "GCTest.hpp"
#include <thread>
#include <random>

using namespace RTGC;

static const int _1K = 1024;
static const int _1M = _1K * _1K;
static int g_cntDealloc = 0;

struct StressTest {
    int _cntAllocatedNodes;
    const StressTestOptions* _options;

    class Node : public GCObject {
        #define PP_CLASS Node
        #include "preprocess/BeginFields.hpp"
        #include "StressTest_Node.hpp"
        #include "preprocess/EndDefine.hpp"
    public:
        Node* init$(StressTest* test, Node* parent) {
            this->set_id(test->_cntAllocatedNodes ++);
            this->set_primitives(GCPrimitiveArray<uint8_t>::alloc(test->_options->primitiveBytes));
            if (parent != nullptr) {
                this->set_parent(parent);
                this->set_root(parent->get_root());
            }
            return this;
        }
        #if GC_DEBUG
        ~Node() {
            g_cntDealloc ++;
        }
        #endif
    };

    class BaggageNode : public Node {
        int _cntNodes; 
        int _cntCircularRef;
        StressTest* _test;
    public:

        BaggageNode* init$(StressTest* test, int cntNodes) {
            Node::init$(test, nullptr);
            this->_test = test;
            this->_cntNodes = cntNodes;
            this->_cntCircularRef = test->_options->circularRefPercent * cntNodes / 100;
            this->set_root(this);
            attachChildren(this, 0);
            return this;
        }

        void attachChildren(Node* node, int layer) {
            node->set_var1(createChildNode(node, layer));
            node->set_var2(createChildNode(node, layer));
            node->set_var3(createChildNode(node, layer));
        }

        RETAINED_PTR<Node> createChildNode(Node* parent, int layer) {
            int idx_child = _test->_cntAllocatedNodes - this->get_id();
			if (idx_child >= _cntNodes) {
                return RETAINED_PTR<Node>::make(nullptr);
            }

            if (idx_child >= _cntCircularRef) {
                parent = nullptr;
            }
            RETAINED_PTR<Node> node = RTGC_NEW(Node, (_test, parent));
            if (++layer < 7) {
                attachChildren(node, layer);
            }
            return node;
        }
    };

    StressTest(const StressTestOptions* options) {
        this->_cntAllocatedNodes = 0;
        this->_options = options;
    }

    RETAINED_PTR<BaggageNode> createBaggage(int cntNodes) {
        if (cntNodes <= 0) {
            return RETAINED_PTR<BaggageNode>::make(nullptr);
        }
        RETAINED_PTR<BaggageNode> root = RTGC_NEW(BaggageNode, (this, cntNodes));
        return root;
    }

    int64_t stressTest(const int payload) {
		printf("--------------------------------------\n");
		printf("Preparing test...\n");

        SimpleTimer timer;
        SimpleTimer timer2;
        GCObject::ARRAY baggages = GCObjectArray<GCObject>::alloc(payload);
        StressTest::Node::set_testNodes(baggages);
        _cntAllocatedNodes = 0;
        
        for (int idx = 0; idx < payload; idx++) {
            baggages._set(idx, createBaggage(_1K));
        }

		printf("[RTGC] StressTest started. nodes=%dk\n", _cntAllocatedNodes/1024);

        timer.reset();
        timer2.reset();
        int updateLimit = payload * _options->replacePercent / 100;
        if (updateLimit == 0) {
            updateLimit = 1;
        }
        for (int repeat = 0; repeat++ < 5; ) {
            for (int i = 0; i < payload; i++) {
                int idx = i % updateLimit;
                baggages._set(idx, createBaggage(_1K));
            }
            printf("%d) %d ms\n", repeat, timer.reset());
            GCRuntime::dumpDebugInfos();
        }
        int64_t elapsed = timer2.reset();
        printf("Total elapsed time: %lld\n", elapsed);
        return elapsed;
    }  
};

#undef PP_CLASS
#define PP_CLASS StressTest::Node
#include "preprocess/BeginScanInfo.hpp"
    #include "StressTest_Node.hpp"
#include "preprocess/EndDefine.hpp"
#include "preprocess/BeginStatic.hpp"
    #include "StressTest_Node.hpp"
#include "preprocess/EndDefine.hpp"
#undef PP_CLASS

void testGCPerforamace(const StressTestOptions* options) {


    printf("[RTGC] StressTest. \n");
    printf(" - Base payload: %ik nodes\n", options->basePayload);
	printf(" - Primitive bytes: %i byte\n", options->primitiveBytes);
	printf(" - Circular ref percent: %i%%\n", options->circularRefPercent);
    printf(" - Replace percent: %d%%\n", options->replacePercent);

    try {
        StressTest test(options);
        fprintf(stderr, "#[RTGC]");
		for (int payload = 0;;) {
            payload += options->basePayload;
            int64_t elapsed = test.stressTest(payload);
            fprintf(stderr, ", %lld", elapsed);
            fflush(stderr);
        }
    }
    catch (const char* e) {
        printf("Application terminated by %s\n", e);
    }
}


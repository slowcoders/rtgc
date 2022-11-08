
#include <stdio.h>
#include "GCTest.hpp"

static const int _1K = 1024;
static const int _1M = _1K * _1K;

struct StressTest {
    int _cntAllocatedNodes;
    const StressTestOptions* _options;

    class Node {
    public:
        int _id;
        Node *_var1;
        Node *_var2;
        Node *_var3;
        Node *_parent;
        Node *_root;
        char *_primitives;

        Node(StressTest* test, Node* parent) {
            this->_id = test->_cntAllocatedNodes ++;
            this->_primitives = new char[test->_options->primitiveBytes];
            if (parent != nullptr) {
                this->_parent = parent;
                this->_root = parent->_root;
            }
            else {
                _parent = _root = nullptr;
            }

            _var1 = _var2 = _var3 = nullptr;
        }

        ~Node() {
            delete _var1;
            delete _var2;
            delete _var3;
            delete[] _primitives;
        }
    };

    class BaggageNode : public Node {
        int _cntNodes; 
        int _cntCircularRef;
        StressTest* _test;
    public:

        BaggageNode(StressTest* test, int cntNodes) : Node(test, nullptr) {
            this->_test = test;
            this->_cntNodes = cntNodes;
            this->_cntCircularRef = test->_options->circularRefPercent * cntNodes / 100;
            this->_root = this;
            attachChildren(this, 0);
        }

        void attachChildren(Node* node, int layer) {
            node->_var1 = createChildNode(node, layer);
            node->_var2 = createChildNode(node, layer);
            node->_var3 = createChildNode(node, layer);
        }

        Node* createChildNode(Node* parent, int layer) {
            int idx_child = _test->_cntAllocatedNodes - this->_id;
			if (idx_child >= _cntNodes) {
                return nullptr;
            }

            if (idx_child >= _cntCircularRef) {
                parent = nullptr;
            }
            Node* node = new Node(_test, parent);
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

    BaggageNode* createBaggage(int cntNodes) {
        if (cntNodes <= 0) {
            return nullptr;
        }
        BaggageNode* root = new BaggageNode(this, cntNodes);
        return root;
    }

    int64_t stressTest(const int payload) {
		printf("--------------------------------------\n");
		printf("Preparing test...\n");

        SimpleTimer timer;
        SimpleTimer timer2;
        Node** baggages = new Node*[payload];
        _cntAllocatedNodes = 0;
        
        for (int i = 0; i < payload; i++) {
            baggages[i] = createBaggage(_1K);
        }

		printf("[C++] StressTest started. nodes=%dk\n", _cntAllocatedNodes/1024);

        timer.reset();
        timer2.reset();
        int updateLimit = payload * _options->replacePercent / 100;
        if (updateLimit == 0) {
            updateLimit = 1;
        }
        for (int repeat = 0; repeat++ < 5; ) {
            for (int i = 0; i < payload; i++) {
                int idx = i % updateLimit;
                delete baggages[idx];
                baggages[idx] = createBaggage(_1K);
            }
            printf("%d) %d ms\n", repeat, timer.reset());
        }
        int64_t elapsed = timer2.reset();
        printf("Total elapsed time: %d\n", elapsed);

        for (int i = 0; i < payload; i++) {
            Node *n = baggages[i];
            delete n;
        }
        delete[] baggages;

        return elapsed;
    }  

};

void testNoGCPerformance(const StressTestOptions* options) {

    printf("[C++] StressTest \n");
    printf(" - Base payload: %ik nodes\n", options->basePayload);
	printf(" - Primitive bytes: %i byte\n", options->primitiveBytes);
	printf(" - Circular ref percent: %i%%\n", options->circularRefPercent);
    printf(" - Replace percent: %d%%\n", options->replacePercent);

    try {
        StressTest test(options);
        fprintf(stderr, "#[C++]");
        for (int payload = 0;;) {
            payload += options->basePayload;
            int64_t elapsed = test.stressTest(payload);
            fprintf(stderr, ", %d", elapsed);
            fflush(stderr);
        }
    }
    catch (...) {
        printf("Application terminated by OutOfMemoryError. \n");
    }
}

#include "GCNode.hpp"

struct StressTestOptions {
    int basePayload;
    int primitiveBytes;
    int circularRefPercent;
    int replacePercent;
};

class SimpleTimer {
    int64_t t1;
    int64_t t2;
public:
    SimpleTimer() {
        reset();
        t1 = t2;
    }

    int reset() {
        t1 = t2;
        t2 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return (int)(t2 - t1);
    }
};

void testGCPerforamace(const StressTestOptions* option);
void testCircuitScanner();

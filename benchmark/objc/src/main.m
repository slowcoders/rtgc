
#import "GCTest.h"

int main(int argc, const char * args[]) {
    StressTestOptions options;
    options.basePayload = atoi(args[1]); 
    options.primitiveBytes = atoi(args[2]); 
    options.circularRefPercent = atoi(args[3]);
    options.replacePercent = atoi(args[4]);
    
    @autoreleasepool {
        testGCPerforamace(&options);
    }
    return 0;
}

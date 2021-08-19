#import <Foundation/Foundation.h>

typedef struct _StressTestOptions {
    int basePayload;
    int primitiveBytes;
    int circularRefPercent;
    int replacePercent;
} StressTestOptions;

void testGCPerforamace(const StressTestOptions* options);


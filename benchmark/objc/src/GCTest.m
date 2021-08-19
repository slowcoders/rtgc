
#import "GCTest.h"



static const int _1K = 1024;
static const int _1M = _1K * _1K;
static char *g_testTitle = "[objective-c]";
static NSMutableArray* g_testNodes;

@interface SimpleTimer: NSObject

@end

@implementation SimpleTimer {
  NSInteger t1;
  NSInteger t2;
}

- (instancetype)init
{
  self = [super init];
  if (self) {
    [self reset];
    t1 = t2;
  }
  return self;
}

- (NSInteger)reset
{
  t1 = t2;
  t2 = NSDate.date.timeIntervalSince1970 * 1000;
  return t2 - t1;
}

@end

@interface StressTest: NSObject {
    @public NSInteger _cntAllocatedNodes;
    @public const StressTestOptions* _options;
}
@end


@interface Node: NSObject {
    @public int32_t _id;
    @public Node* _var1;
    @public Node* _var2;
    @public Node* _var3;
    @public __weak Node* _parent;
    @public __weak Node* _root;
    @public NSData* _primitives;
}
@end

@interface BaggageNode: Node {
    @public int _cntNodes;
    @public int _cntCircularRef;
    @public StressTest* _test;
}
@end

@implementation Node

- (instancetype)initWithTest:(StressTest*)test parent:(Node*)parent
{
    self = [super init];
    if (self) {
        self->_id = (int32_t)(test->_cntAllocatedNodes++);
        int len = test->_options->primitiveBytes;
        int8_t* bytes = (int8_t*)calloc(1, len);
        self->_primitives = [NSData dataWithBytesNoCopy: bytes length: len];
        if (parent != nil) {
            self->_parent = parent;
            self->_root = parent->_root;
        }
    }
    return self;
}

@end


@implementation BaggageNode

- (instancetype) initWithTest:(StressTest*)test cntNodes:(int)cntNodes {
    self = [super initWithTest:test parent:nil];
    self->_test = test;
    self->_cntNodes = cntNodes;
    self->_cntCircularRef = test->_options->circularRefPercent * cntNodes / 100;
    self->_root = self;
    [self attachChildrenTo:self layer:0];
    return self;
}

- (void) attachChildrenTo:(Node*)node layer:(int)layer {
    node->_var1 = [self createChildNodeOf:node layer: layer];
    node->_var2 = [self createChildNodeOf:node layer: layer];
    node->_var3 = [self createChildNodeOf:node layer: layer];
}

- (Node*) createChildNodeOf:(Node*)parent layer:(int)layer {
    int idx_child = (int32_t)(_test->_cntAllocatedNodes - self->_id);
    if (idx_child >= _cntNodes) {
        return nil;
    }

    if (idx_child >= _cntCircularRef) {
        parent = nil;
    }
    Node* node = [[Node alloc] initWithTest:_test parent: parent];
    if (++layer < 7) {
        [self attachChildrenTo:node layer:layer];
    }
    return node;
}

@end

@implementation StressTest

- (instancetype) init:(const StressTestOptions*)options {
    self = [super init];
    if (self != nil) {
        self->_cntAllocatedNodes = 0;
        self->_options = options;
    }
    return self;
}

- (Node*) createBaggage:(int)cntNodes {
    if (cntNodes <= 0) {
        return nil;
    }
    Node* root = [[BaggageNode alloc] initWithTest:self cntNodes:cntNodes];
    return root;
}

- (void)stressTest:(int)payload {
    printf("--------------------------------------\n");
    printf("Preparing test...\n");

    SimpleTimer *timer = [[SimpleTimer alloc] init];
    SimpleTimer *timer2 = [[SimpleTimer alloc] init];
    NSMutableArray *baggages = [NSMutableArray array];
    g_testNodes = baggages;
    _cntAllocatedNodes = 0;

    for (int i = 0; i < payload; i++) {
        NSObject *node = [self createBaggage:_1K];
        [baggages addObject:node];
    }
    
    printf("%s StressTest started. nodes: %ik\n", g_testTitle, _cntAllocatedNodes/1024);
    _cntAllocatedNodes = 0;
    
    [timer reset];
    [timer2 reset];
    
    int updateLimit = payload * _options->replacePercent / 100;
    if (updateLimit == 0) {
        updateLimit = 1;
    }
    for (int repeat = 0; repeat++ < 5; ) {
        for (int i = 0; i < payload; i++) {
            int idx = i % updateLimit;
            baggages[idx] = [self createBaggage:_1K];
        }
        printf("%i) %lu ms\n", repeat, [timer reset]);
    }
    baggages = nil;
    
    NSInteger elapsed = [timer2 reset];
    printf("Total elapsed time: %lu\n", elapsed);
}

@end

void testGCPerforamace(const StressTestOptions* options) {
  printf("%s StressTest\n", g_testTitle);
  printf(" - Base payload: %ik nodes\n", options->basePayload);
  printf(" - Primitive bytes: %i byte\n", options->primitiveBytes);
  printf(" - Circular ref percent: %i byte\n", options->circularRefPercent);
  printf(" - Replace percent: %i%%\n", options->replacePercent);
  
  @try {
    StressTest* test = [[StressTest alloc] init: options];
    for (int payload = 0, level = 0; level ++ < 10;) {
        payload += options->basePayload;
        [test stressTest:payload];
    }
  }
  @catch (NSException *e) {
    printf("\nTest is terminated by OutOfMemoryError");
  }
}


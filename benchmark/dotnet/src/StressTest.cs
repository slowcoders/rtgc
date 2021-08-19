using System;

#pragma warning disable CS0169
#pragma warning disable CS0414
#pragma warning disable CS0649

namespace gctest
{
    public class StressTest {

        const int _1K = 1024;
        const int _1M = _1K * _1K;
        public int _cntAllocatedNodes;
        public Options _options;
        public static String g_testTitle;

        public struct Options {
            public int basePayload;
            public int primitiveBytes;
            public int circularRefPercent;
            public int replacePercent;
        };

        class Node { 
            public int _id;
            public Node var1;
            public Node var2;
            public Node var3;
            public Node parent;
            public Node root;
    		public Object primitives;
            
            public Node(StressTest test, Node parent) {
                this._id = test._cntAllocatedNodes ++;
                this.primitives = new byte[test._options.primitiveBytes];
                if (parent != null) {
                    this.parent = parent;
                    this.root = parent.root;
                }
            }
        };

        class BaggageNode : Node {
            int _cntNodes; 
            int _cntCircularRef;
            StressTest _test;

            public BaggageNode(StressTest test, int cntNodes) : base(test, null) {
                this._test = test;
                this._cntNodes = cntNodes;
                this._cntCircularRef = test._options.circularRefPercent * cntNodes / 100;
                this.root = this;
                attachChildren(this, 0);
            }

            void attachChildren(Node node, int layer) {
                node.var1 = createChildNode(node, layer);
                node.var2 = createChildNode(node, layer);
                node.var3 = createChildNode(node, layer);
            }

            Node createChildNode(Node parent, int layer) {
                long idx_child = _test._cntAllocatedNodes - this._id;
                if (idx_child >= _cntNodes) {
                    return null;
                }

                if (idx_child >= _cntCircularRef) {
                    parent = null;
                }
                Node node = new Node(_test, parent);
                if (++layer < 7) {
                    attachChildren(node, layer);
                }
                return node;
            }
        };        

        StressTest(Options options) {
            this._cntAllocatedNodes = 0;
            this._options = options;
        }

        Node createBaggage(int cntNodes) {
            if (cntNodes <= 0) {
                return null;
            }
            BaggageNode root = new BaggageNode(this, cntNodes);
            return root;
        }

        static Object[] g_testNodes;
        public long stressTest(int payload) {
            Console.WriteLine("--------------------------------------\n");
            Console.WriteLine("Preparing test...");
            SimpleTimer timer = new SimpleTimer();
            SimpleTimer timer2 = new SimpleTimer();

            Object[] baggages = new Object[payload];
            g_testNodes = baggages;
            _cntAllocatedNodes = 0;
            GC.Collect();
            GC.Collect();

            for (int i = 0; i < payload; i++) {
                baggages[i] = createBaggage(_1K);
            }
            Console.WriteLine(g_testTitle + " StressTest started. " + 
                "nodes: " + _cntAllocatedNodes/1024 + "k");

            timer.reset();
            timer2.reset();
            int updateLimit = payload * _options.replacePercent / 100;
            if (updateLimit == 0) {
                updateLimit = 1;
            }
            for (int repeat = 0; repeat++ < 5; ) {
                for (int i = 0; i < payload; i++) {
                    int idx = i % updateLimit;
                    baggages[idx] = createBaggage(_1K);
                }
                Console.WriteLine(repeat + ") " + timer.reset() + " ms");
            }

            long elapsed = timer2.reset();
            Console.WriteLine("Total elapsed time: " + elapsed);
            return elapsed;
        }

        public static void doTest(Options options) {
            
            String gcMode = System.Runtime.GCSettings.IsServerGC ? "Server" : "Workstation";
            g_testTitle = "[.Net v" + Environment.Version.ToString() + "/" + gcMode + "]";

            Console.WriteLine(g_testTitle + " StressTest");
            Console.WriteLine(" - Base payload: " + options.basePayload + "k nodes");
		    Console.WriteLine(" - Primitive bytes: " + options.primitiveBytes + " byte");
            Console.WriteLine(" - Circular ref percent: " + options.circularRefPercent + "%");
            Console.WriteLine(" - Replace percent: " + options.replacePercent + "%");
            Console.WriteLine(" - TotalAvailableMemory: " + GC.GetGCMemoryInfo().TotalAvailableMemoryBytes/_1M + "m");

            try {
                Console.Error.Write("#" + g_testTitle);
                StressTest test = new StressTest(options);
                for (int payload = 0;;) {
                    payload += options.basePayload;
                    long elapsed = test.stressTest(payload);
                    Console.Error.Write(", " + elapsed);
                    Console.Error.Flush();
                }
            }
            catch (OutOfMemoryException) {
                Console.WriteLine("\nTest is terminated by OutOfMemoryException");
            }
        }
    }
}
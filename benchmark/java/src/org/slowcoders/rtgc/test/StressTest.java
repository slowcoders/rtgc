package org.slowcoders.rtgc.test;


public class StressTest {
	static final int _1K = 1024;
	static final int _1M = _1K * _1K;
	
	public static String g_testTitle;
	public int _cntAllocatedNodes;
	public Options _options;

	public static class Options {
		public int basePayload;
		public int primitiveBytes;
		public int circularRefPercent;
		public int replacePercent;
	};
	
	static class Node { 
		public int  _id;
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
	}

	static class BaggageNode extends Node {
		int _cntNodes; 
		int _cntCircularRef;
		StressTest _test;

		public BaggageNode(StressTest test, int cntNodes) {
			super(test, null);
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
	private long stressTest(final int payload) {
		System.out.println("--------------------------------------\n");
		System.out.println("Preparing test...");

		SimpleTimer timer = new SimpleTimer();
		SimpleTimer timer2 = new SimpleTimer();

		Object[] baggages = new Object[payload];
		g_testNodes = baggages;
		_cntAllocatedNodes = 0;
		System.gc();
		System.gc();

		for (int i = 0; i < payload; i++) {
			baggages[i] = createBaggage(_1K);
		}

		long freeMemory = Runtime.getRuntime().freeMemory() / _1M;
		System.out.println(g_testTitle + " StressTest started. " + 
			"nodes: " + _cntAllocatedNodes/1024 + "k, " +
		  	"freeMemory: " + freeMemory + "m");

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
			System.out.println(repeat + ") " + timer.reset() + " ms");
		}

		long elapsed = timer2.reset();
		System.out.println("Total elapsed time: " + elapsed);
		return elapsed;
	}

	public static void doTest(Options options) {
            
		g_testTitle = "[Java]"; 

		System.out.println(g_testTitle + " StressTest");
		System.out.println(" - Base payload: " + options.basePayload + "k nodes");
		System.out.println(" - Primitive bytes: " + options.primitiveBytes + " byte");
		System.out.println(" - Circular ref percent: " + options.circularRefPercent + "%");
		System.out.println(" - Replace percent: " + options.replacePercent + "%");
		System.out.println(" - maxMemory: " + Runtime.getRuntime().maxMemory() / _1M + "m");

		try {
			System.err.print("#" + g_testTitle);
			StressTest test = new StressTest(options);
			for (int payload = 0;;) {
				payload += options.basePayload;
				long elapsed = test.stressTest(payload);
				System.err.print(", " + elapsed);
				System.err.flush();
			}
		}
		catch (OutOfMemoryError e) {
			System.out.println("\nTest is terminated by OutOfMemoryError");
		}
	}	
}

package org.slowcoders.rtgc.test;

public class GCTest {

	public static void main(String args[]) throws Exception {
        StressTest.Options options = new StressTest.Options();
        options.basePayload = Integer.parseInt(args[0]); 
        options.primitiveBytes = Integer.parseInt(args[1]); 
        options.circularRefPercent = Integer.parseInt(args[2]);
        options.replacePercent = Integer.parseInt(args[3]);
        StressTest.doTest(options);
    }
}

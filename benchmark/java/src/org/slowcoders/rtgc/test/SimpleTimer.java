package org.slowcoders.rtgc.test;

public class SimpleTimer {
    long t1, t2;

    SimpleTimer() {
        reset();
        t1 = t2;
    }

    public long reset() {
        t1 = t2;
        t2 = System.currentTimeMillis();
        long elapsed = t2 - t1;
        return elapsed;
    }
}

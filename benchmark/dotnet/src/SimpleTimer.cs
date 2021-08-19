using System;

namespace gctest
{
    public class SimpleTimer {
        long t1, t2;

        public SimpleTimer() {
            reset();
            t1 = t2;
        }

        public long reset() {
            t1 = t2;
            t2 = Environment.TickCount;
            long elapsed = t2 - t1;
            return elapsed;
        }
    }
}
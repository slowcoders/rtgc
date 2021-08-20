using System;

namespace gctest
{
    public class Program
    {
        public static void Main(string[] args)
        {     
            StressTest.Options options = new StressTest.Options();
            options.basePayload = Int32.Parse(args[0]); 
            options.primitiveBytes = Int32.Parse(args[1]); 
            options.circularRefPercent = Int32.Parse(args[2]);
            options.replacePercent = Int32.Parse(args[3]);

            StressTest.doTest(options);
        }
    }
}
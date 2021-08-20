#/bin/bash
set -e

javac src/org/slowcoders/rtgc/test/*.java -d classes
if [ "$GC_TEST_MEM_SIZE" == "" ]; then
    GC_TEST_MEM_SIZE=8388608 
fi
# Java runtime needs extra system memory. 
let HEAP_SIZE=($GC_TEST_MEM_SIZE*31/32 - 520*1024)*1024

set +e
# ulimit -v $GC_TEST_MEM_SIZE

# CompressedOops is disabled
java -cp classes -XX:-UseCompressedOops \
    -Xms$HEAP_SIZE -Xmx$HEAP_SIZE \
    org.slowcoders.rtgc.test.GCTest \
    $* 2> test-result.csv

echo
echo ------------------------------------------------------------------
cat test-result.csv
echo
echo ==================================================================

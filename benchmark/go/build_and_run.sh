#/bin/bash
set -e

rm -f ./gctest
go build .

# For successful test execution, the value of GOGC must be less than 16.
export GOGC=10

set +e
if [ "$GC_TEST_MEM_SIZE" == "" ]; then
    GC_TEST_MEM_SIZE=8388608 
fi
ulimit -v $GC_TEST_MEM_SIZE
./gctest.out $* 2> test-result.csv

echo
echo ------------------------------------------------------------------
cat test-result.csv
echo
echo ==================================================================

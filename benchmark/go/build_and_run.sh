#/bin/bash
set -e

rm -f ./gctest
go build .


set +e
if [ "$GC_TEST_MEM_SIZE" == "" ]; then
    GC_TEST_MEM_SIZE=8388608 
fi
ulimit -v $GC_TEST_MEM_SIZE
# 8G mem: the value of GOGC must be less or equal to 15.
# 24G mem: the value of GOGC must be less or equalt to 8.
GC_PERCENT=`echo "$GC_TEST_MEM_SIZE" | awk '{print 24 - int(sqrt($1) * 7 / 2071)}'`
export GOGC=$GC_PERCENT
./gctest.out $* 2> test-result.csv

echo
echo ------------------------------------------------------------------
cat test-result.csv
echo
echo ==================================================================

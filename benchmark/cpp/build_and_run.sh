#/bin/bash
set -e

mkdir -p obj
make clean
make

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

#/bin/bash
set -e

mkdir -p obj
echo --make clean
make clean
echo --make
make

set +e
if [ "$GC_TEST_MEM_SIZE" == "" ]; then
    GC_TEST_MEM_SIZE=8388608 
fi
ulimit -v $GC_TEST_MEM_SIZE
./gctest.out
#./gctest.out $* 2> test-result.csv

echo
echo ------------------------------------------------------------------
cat test-result.csv
echo
echo ==================================================================

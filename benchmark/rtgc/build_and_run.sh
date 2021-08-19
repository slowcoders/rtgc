#/bin/bash
set -e

mkdir -p obj
make clean
make

set +e
ulimit -v $GC_TEST_MEM_SIZE
./gctest.out $* 2> test-result.csv

echo
echo ------------------------------------------------------------------
cat test-result.csv
echo
echo ==================================================================

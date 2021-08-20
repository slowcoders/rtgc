
#/bin/bash
set -e

TARGET="--configuration Release"
dotnet clean $TARGET gctest.csproj 
SERVER_MODE=$5
if [ "$SERVER_MODE" == "" ]; then
    SERVER_MODE=true
fi
if [ "$GC_TEST_MEM_SIZE" == "" ]; then
    GC_TEST_MEM_SIZE=8388608 
fi
let MAX_HEAP=($GC_TEST_MEM_SIZE-384*1024)*1024
cat <<EOF > runtimeconfig.template.json
{
  "configProperties": {
    "System.GC.Server": $SERVER_MODE,
    "System.GC.HeapHardLimit": $MAX_HEAP
  }
}
EOF
dotnet build $TARGET gctest.csproj

set +e
# ulimit -v $GC_TEST_MEM_SIZE
dotnet run $TARGET $* 2> test-result.csv

echo
echo ------------------------------------------------------------------
cat test-result.csv
echo
echo ==================================================================

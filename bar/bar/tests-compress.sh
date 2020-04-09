#!/bin/bash

# test script to run different compression algorithms and measure
# metrics

TEST_FILE_BAR=/tmp/test.bar
TEST_FILES="/usr/bin /home/virtualbox/iso/KNOPPIX_V7.0.5DVD-2012-12-21-EN.iso"

TEST_COMPRESS_ALGORITHMS="none lzo1 lzo5 lz4-1 lz4-16 zstd1 zstd19 zip9 bzip1 bzip9 lzma1 lzma9"
#TEST_COMPRESS_ALGORITHMS="zstd1"

sizeOriginal=`du -sbc $TEST_FILES|tail -1|cut -f 1`

#set -x

echo "Algorithm  Size  Ratio Time     Memory"
echo "--------------------------------------"
for z in $TEST_COMPRESS_ALGORITHMS; do
  timeMemory=`/usr/bin/time -f "%e %M" sh -c "./bar -c -o $TEST_FILE_BAR -z $z $TEST_FILES 1>/dev/null 2>/dev/null" 2>&1`
  time=`echo $timeMemory|cut -d " " -f 1`
  memory=`echo $timeMemory|cut -d " " -f 2`
  memoryM=`echo $memory/1024|bc`
  size=`du -sb $TEST_FILE_BAR|cut -f 1`
  sizeM=`echo $size/1024/1024|bc`
  ratio=`echo "(1-$size/$sizeOriginal)*100"|bc -l`
  printf "%-10s %4dM %5.2f %-7ss %5dM\n" $z $sizeM $ratio $time $memoryM
done
rm -f $TEST_FILE_BAR

exit 0

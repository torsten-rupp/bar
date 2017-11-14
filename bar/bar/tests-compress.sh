#!/bin/bash

TEST_DIRECTORY=/home/torsten/text
TEST_DIRECTORY=/usr/bin
#TEST_DIRECTORY=test/data

sizeOriginal=`du -sb $TEST_DIRECTORY|cut -f 1`

#set -x

echo "Algorithm  Size  Ratio Time   Memory"
echo "---------------------------------------"
for z in none lzo1 lzo5 lz4-1 lz4-16 zstd-1 zstd-19 zip9 bzip1 bzip9 lzma1 lzma9; do
#for z in lz4-1 lz4-16 zstd-1 zstd-19; do
#echo ./bar -c -o test.bar -z $z $TEST_DIRECTORY
  timeMemory=`/usr/bin/time -f "%e %M" sh -c "./bar -c -o test.bar -z $z $TEST_DIRECTORY >/dev/null" 2>&1`
  time=`echo $timeMemory|cut -d " " -f 1`
  memory=`echo $timeMemory|cut -d " " -f 2`
  memoryM=`echo $memory/1024|bc`
  size=`du -sb test.bar|cut -f 1`
  sizeM=`echo $size/1024/1024|bc`
  ratio=`echo "(1-$size/$sizeOriginal)*100"|bc -l`
  printf "%-10s %4dM %5.2f %-5ss %5dM\n" $z $sizeM $ratio $time $memoryM
done

exit 0

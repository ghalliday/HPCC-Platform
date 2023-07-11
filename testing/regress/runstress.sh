#!/bin/bash

rm -f summary.csv
rm -f full.csv
samples=9
for i in {1,2,3,4,5,6,8,10,12,15,20,25,30,35,40,45,50,60}; do
  rm -f timings.txt
  for j in {1..9}; do
     timing=$(~/hpccrel/opt/HPCCSystems/bin/testsocket . "<stresstext_$i summaryStats='1'/>" | grep -o "complete in [0-9]*" | grep -o "[0-9]*")
     echo $timing >> timings.txt
  done
  sort -n timings.txt > sorted.txt
  readarray -t sorted < sorted.txt
  minvalue=${sorted[0]}
  medianvalue=${sorted[$(((${#sorted[@]})/2))]}
  echo $i,$minvalue >> summary.csv
  echo "$i,${sorted[@]}" >> full.csv
done

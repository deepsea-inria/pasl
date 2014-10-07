#!/bin/bash

FLG=$1

if [[ $FLG == "" ]] 
then
    echo "usage:"
    echo "hyperthreading.sh 0           to enable"
    echo "hyperthreading.sh 1           to disable"
    exit 1
fi

for i in {40..79}
do
   cmd="echo $FLG > /sys/devices/system/cpu/cpu$i/online"
   echo "$cmd"
   sudo sh -c "$cmd"
done

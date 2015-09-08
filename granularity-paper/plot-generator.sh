#!/bin/bash

make -j CONTROL_OPT='PREDICTION' LOOP_OPT='BINARY' LIB='LITE' bench.log  bench.dlog

c=100000000
ns=( 1 10 100 1000 10000 100000 1000000 10000000 100000000 )
#procs=( 1 2 4 8 40 )
procs=( 1 4 )

for n in "${ns[@]}"
do
   for proc in "${procs[@]}"
   do
       echo $n $proc

       ./bench.log -n ${n} -c ${c} -bench synthetic -algo parallel_for -proc ${proc} --log_estims --log_text
       python ../granularity/plot.py -d ../granularity-paper/ -p normal-$n -proc ${proc} -s
                                                                                    
       ./bench.dlog -n ${n} -c ${c} -bench synthetic -algo parallel_for -proc ${proc} --log_estims --log_text
       python ../granularity/plot.py -d ../granularity-paper/ -p dual-$n -proc ${proc} -s
   done
done
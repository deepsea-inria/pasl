PASL: Parallel Algorithm Scheduling Library
===========================================

You can find the package documentation on the [PASL web
page](http://deepsea.inria.fr/pasl/).

Directory structure
-------------------

---------------------------------------------------------------------
   Folder                Contents
-------------            --------------------------------------------
`sequtil`                command-line interface, printing interface, 
                         timers, control operators, container data 
                         structures

`parutil`                locks, barriers, concurrent containers,
                         worker threads, worker-local storage,
                         platform-specific configuration mining

`sched`                  threading primitives, DAG primitives, 
                         thread-scheduling algorithms, granularity 
                         control

`example`                example programs

`test`                   regression tests

`tools`                  log visualization, shared PASL makefiles,
                         ported code from Problem Based Benchmark 
                         Suite, custom malloc count, script to
                         enable/disable hyperthreading, matrix 
                         market file IO, export scripts

`chunkedseq`             chunked-sequence library

`graph`                  graph-processing library

----------------------------------------------------------------------
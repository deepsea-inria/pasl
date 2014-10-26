% PASL Graph Algorithms Benchmarking User's Guide
% Umut Acar, Arthur Charguéraud, Mike Rainey
% 4 September 2014

Synopsis
========

graph.byte [*ACTION*] [*PARAMETERS*]

Description
===========

graph.byte is a script whose purpose is to benchmark a number of graph
algorithms of the PASL graph library. The script automates all aspects of
the benchmarking: building binaries, generation of input data, running
of experiments, and output of experimental data, including plots, tables,
and raw data.

Options
=======

Actions
-------

The action selects the overall behavior of the script.
*ACTION* can be one of the following:

`generate`
:   Generate graph files that are to be used by the graph-search
    experiments.

`accessible`
:   Compute for each benchmarking graph the number of vertices that
    are accessible from the specified source vertex. The purpose of
    this experiment is to use a trusted sequential algorithm to
    generate reliable data to be used for sanity checks.

`baselines`
:   Run experiments which collects data points for baseline (i.e., best
    sequential) graph-search algorithms.

`work_efficiency`
:   Run the experiment which evaluates the work efficiency of the parallel
    graph algorithms.

`lack_parallelism`
:   Run the experiment which evaluates the performance of the parallel
    graph algorithms when applied to graphs that provide few opportunities
    for parallelism.

`speedups_dfs`
:   Run the experiment which reports the speedups relative to a fast
    sequential baseline DFS of the parallel DFS algorithms.

`speedups_bfs`
:   Run the experiment which reports the speedups relative to a fast
    sequential baseline BFS of the parallel BFS algorithms.

`idempotence`
:   Run the experiment which reports various strategies for reducing
    synchronization overheads.

Parameters
----------

Parameters select finer details of the behavior of the script.
*PARAMETERS* can be zero or more of the following:

`-runs` *n*
:   Specifies the number of times *n* to execute for each combination of 
    benchmark parameters. Default is 1.

`-timeout` *n*
:   Force a specific timeout for runs. The timeout value *n* is measured
    in seconds.

`--virtual_run`
:   Only show the list of commands that would be called to run the benchmarks.

`--use_cilk`
:   Run all benchmarks using Cilk Plus (instead of PASL); disabled by
    default.

`-skip` *a1*,*a2*
:   Skip selected actions (actions: make, run, check, plot).
    Note: `-skip run` automatically activates `-skip make`.

`-only` *a1*,*a2*
:   Perform only selected actions.

`-size` *s1*,*s2*
:   Consider only graph of selected sizes
    (sizes: `small`, `medium`, `large`, `all`)

`-kind` *k1*,*k2*
:   Consider only graph of selected kinds (kinds:
    `grid_sq`, `cube`, ... and also: `manual` and `generated` and `all`)

`-idempotent` *x1*,*x2*
:   (values: 0 or 1 or 0,1)

`-algo` *x1*,*x2*
:   Consider only selected algorithms (used by sequential and parallel experiments).

`-exp x1,x2`  
:   Consider only selected experiments (used by cutoff and `lack_parallelism` experiments).

`-proc` *n*
:   Specify the number of processors that can be used in parallel runs.
    Default value is based on the host machine.

`-timeout` *n*
:   Force a specific timeout for runs
    Default values depend on the `size` argument provided.

`-mode` *m*
:   Where *m* is `normal` (discard all previous results) or `append`.
    (append to previous results) or `replace` (discard results that are ran again)
    or `complete` (add results that are missing).

`--complete`  
:   Is a shorthand for `-mode complete`.

`--replace`  
:   Is a shorthand for `-mode replace`.

`--our_pseudodfs_preemptive`  
:   Enable the algorithm `our_pseudodfs_preemptive`.

`--sequential_full`
:   Include competition algorithms in sequential experiment.

`--nocilk`  
:   Skip runs with cilk binaries in the lack_parallelism experiment

`--details`  
:   Report the execution time in the tables.

`-our_pbfs_cutoff` *n*
:   Specify cutoff *n* for our parallel BFS.

`-ls_pbfs_cutoff` *n*
:   Specify *n* for the frontier cutoff of Leiserson and Shardl's BFS.

`-ls_pbfs_loop_cutoff` *n*
:   Specify *n* for the edgelist cutoff of Leiserson and Shardl's BFS.


Sample applications
===================

Run the parallel DFS speedup experiment for PASL binaries
---------------------------------------------------------

	graph.byte -size large speedups_dfs -proc 40 -runs 1

Run the parallel BFS speedup experiment for Cilk binaries
---------------------------------------------------------

	graph.byte -size large speedups_bfs -proc 40 -runs 1 --use_cilk

See also
========

The relevant source code and documentation can be downloaded from
<http://deepsea.inria.fr/graph/>

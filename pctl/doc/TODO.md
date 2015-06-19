% The pctl todo list
% Deepsea Project
% 2015

Introduction
============

The purpose of this file is to maintain a list of tasks to be done on
the pctl.

Please help us keep this file up to date.

Tasks for anyone
================

- Create makefiles for example programs, unit test programs, and
  benchmark programs.

- Unit tests should support the following modes:
  - parallel-algorithm only
  - sequential-algorithm only
  - parallel + sequential algorithm (by granularity control)

- Think about adding the following extra compilation mode: parallel
  algorithm only + sequential elision. In this model, all parallelism
  should be compiled away, yet the parallel algorithm should
  remain. The purpose is to enable the programmer to debug their
  parallel algorithm without having to deal with noise introduced by
  parallel constructs, e.g., fork2(), which interfere with, say, `gdb`
  sessions.

- Expand unit tests for `scan` function to test for correct handling
  of indices (e.g., `scani`) and to test the `lift` operator.

- In nearestneighbors, find places where complexity function is not
  constant time and modify accordingly.

- Complete and document the destination-passing style interface of
  datapar (i.e., dpsdatapar).

- Complete remaining parray constructors

Tasks for Mike
==============

- Update psort benchmarking program to use new interface.

- Later: introduce "weighted parray" and "weighted pchunkedseq"
  structures. The idea is that each of these structures is enriched
  with a function that can assign a weight for any given sequence
  item. We cache internally the prefix array of the weights in order
  to speed up aggregate weight queries. Using this technique, we can
  specialize our data-parallel operations to use the cached aggregate
  weight values.


- Implement Vitaly and Anna's extension to granularity control for
  dealing with nested parallelism.

- add pushn/popn methods to pchunkedseq

- remove dead code from fixedcapacity module
PASL: Parallel Algorithm Scheduling Library
===========================================

The full package documentation can be found on the [PASL web
page](http://deepsea.inria.fr/pasl/).

For best results, render this document using
[pandoc](http://johnmacfarlane.net/pandoc/). For example, to generate
a PDF, run from the command line the following.

    pandoc README.md -o README.pdf

Directory structure
-------------------

---------------------------------------------------------------------
       Folder            Contents
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

Supported platforms
-------------------

Currently, PASL supports only x86-64 machines. Furthermore, full
support is available only for recent versions of Linux. Limited
support is available for Mac OS.

Package dependencies
--------------------

----------------------------------------------------------------------------------------
                        Package   Version     Details
-------------------------------   ----------    ----------------------------------------
[gcc](https://gcc.gnu.org/)       >= 4.9.0      Recent gcc is required because PASL 
                                                makes heavy use of recent features
                                                of C++ / C++1x, such as lambda 
                                                expressions and higher-order templates.

[php](http://www.php.net/)        >= 5.3.10     PHP is currently used by the build system 
                                                of PASL. In specific, the Makefiles used 
                                                by PASL make calls to PHP programs for 
                                                purposes of dependency analysis. As such, 
                                                PHP is not required in situations where
                                                alternative build systems, such as XCode, 
                                                are used.

[ocaml](http://www.ocaml.org/)    >= 4.00       Ocaml is required by the log-file 
                                                visualization tool, but currently
                                                nothing else.

-----------------------------------------------------------------------------------------


### Installing dependencies on Ubuntu

    $ sudo add-apt-repository ppa:ubuntu-toolchain-r/test
    $ sudo apt-get update
    $ sudo apt-get install g++-4.9 ocaml php5-cli


Getting started
---------------

### Building from the command line

Let us start by building the fibonacci example. First, change to the
examples directory.

    $ cd pasl/example

Then, build the binary for the source file, which is named `fib.cpp`.

    $ make fib.opt -j

Now we can run the program. In this example, we are computing `fib(45)`
using eight cores.

    $ ./fib.opt -n 45 -proc 8

In general, you can add a new build target, say `foo.cpp`, by doing
the following.

1. Create the file `foo.cpp` in the folder `example`.
2. Add to `foo.cpp` the appropriate initialization code. For an
   example, see `fib.cpp`.
3. In the Makefile in the same folder, add a reference to the new
   file.

        PROGRAMS=\
                fib.cpp \
                hull.cpp \
                bhut.cpp \
                ...
                foo.cpp

4. Now you should be able to build and run the new program just like
   we did with fibonacci.

### Building on Mac OS

Currently, building from the command line is not yet
supported. However, it is possible to build from an XCode project.

#### XCode

1. Create a new project.
2. Create a new target binary.
3. In the "build phases" configuration page of the new target binary,
   add to the "Compile Sources" box the contents of the following
   directories.

- `sequtil`
- `parutil`
- `sched`       (except for `pasl.S`; this file should not be imported)

4. Add to the `CFLAGS` of the target binary the following flags.

- `-DTARGET_MAC_OS`
- `-D_XOPEN_SOURCE`

5. Add to your project the remaining source files that you want to
   compile. Remember to add one file which defines the `main()`
   routine. For example, you could add `examples/fib.cpp` if you want
   to run the fibonacci program.
6. Click the "play" button to compile and run the program.

The log visualizer: `pview`
---------------------------

We implemented `pview` to help diagnose performance issues relating
to parallelism. The tool reads logging data that is generated by
the PASL runtime and renders from the data a breakdown of how the 
scheduler spends its time during the run of the program. The tool
and corresponding documentation can be found in the folder named
`tools/pview/`.

% PASL log visualizer
% Umut Acar, Arthur Charguéraud, Mike Rainey
% 13 August 2014


Introduction
============

For best results, render this document using
[pandoc](http://johnmacfarlane.net/pandoc/). For example, to generate
a PDF, run from the command line the following.

    pandoc README.md -o README.pdf


Synopsis
========

pview [*PARAMETER*]...

Description
===========

`pview` helps to visualize the activity/idle time of processors
during a run.

The input of pview program is a log file in binary format.  This log
file is generated when the option `--pview` (or `--log_phases`) is
enabled during the execution of the program.  If you also
log other events, it is likely that the log file will get too big and
that the pview program will not be able to handle it.

The pview program is usually called without arguments, but you can
specify a different log file if you want.

Options
=======

Each *PARAMETER* can be one of the following.

`-input` *PATH*
:    Specify the path to the binary log file; default is `LOG_BIN`.
`-width` *n*
:    Specify the width in number of pixels of the window; default *n*=1000.
`-height` *n*
:    Specify the height in number of pixels of the window; default *n*=1000.
`-proc_height` *n*
:    Specify the height of a line describing one processor; default *n*=30. 

Controls
========

The controls to be used in the program are:

- click to zoom-in
- spacebar to reinitialize zoom
- press 'o' to zoom-out one step
- press 'r' to reload the log file (for example, after a new run)
- press 'q' to quit

Sample applications
===================

From the PASL root folder, run:

    make -C ../tools/pview 
    make fib.log -j
    ./fib.log -n 45 -proc 2 --pview
    ../tools/pview/pview

For frequent use, you may add the pview folder to your path, e.g., adding the
following line to the bottom `~/.bashrc`:

    PATH=$PATH:~/pasl/tools/pview


See also
========

The PASL web page is located at
<http://deepsea.inria.fr/pasl/>


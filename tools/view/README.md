% PASL log visualizer
% Umut Acar, Arthur Charguéraud, Mike Rainey
% 13 August 2014

Synopsis
========

view.out [*PARAMETER*]...

Description
===========

`view.out` helps to visualize the activity/idle time of processors
during a run.

The input of view program is a log file in binary format.  This log
file is generated when the option `--view` (or `--log_phases`) is
enabled during the execution of the program.  If you also
log other events, it is likely that the log file will get too big and
that the view program will not be able to handle it.

The view program is usually called without arguments, but you can
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

Let `PASL_HOME` be the path to the PASL root folder.

    cd $PASL_HOME/example
    make fib.log -j
    ./fib.log -n 48 -proc 40 --view
    make -C $PASL_HOME/tools/view
    $PASL_HOME/tools/view.out

See also
========

The PASL web page is located at
<http://deepsea.inria.fr/pasl/>


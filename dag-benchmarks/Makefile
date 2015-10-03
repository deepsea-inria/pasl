
####################################################################
# Configuration

# Paths to auxiliary Makefile definitions

TOOLS_BUILD_FOLDER=../tools/build


####################################################################
# Mandatory options

USE_PTHREADS=1
USE_MATH=1


####################################################################
# Default options

USE_ALLOCATOR=
USE_HWLOC=0
USE_NUMA=0

# todo activate? USE_ANSI=1

USE_FATAL_ERRORS=1
VERBOSE_MAKE=0
VERBOSE_DEPENDENCIES=0

PROGRAMS=\
	bench.cpp
#       add reference to your cpp source here

####################################################################
# Makefile options

# Create a file called "settings.sh" in this folder if you want to
# configure particular options. See section below for options.

-include settings.sh

# Include here

# Options are then configured by the auxiliary file below

include $(TOOLS_BUILD_FOLDER)/Makefile_options


####################################################################
# Modes

# What are the compilation mode supported, i.e. the "modes"
# (If extending the list, need to add cases for the definition
# of COMPILE_OPTIONS_FOR further below, and also for "clean".

MODES=dbg log sta opt seq cilk opt_4_8 opt_4_128 opt_12_8 opt_12_128 opt_4_32 opt_12_32 opt_rng

# Compilation options for each mode

COMPILE_OPTIONS_COMMON=-DDISABLE_INTERRUPTS -DSTATS_IDLE $(OPTIONS_ALL)
COMPILE_OPTIONS_FOR_dbg=$(OPTIONS_DEBUG) -DSTATS -DDEBUG # -DUSE_UCONTEXT  -DLOGGING
COMPILE_OPTIONS_FOR_log=$(OPTIONS_O2) -DSTATS -DLOGGING
COMPILE_OPTIONS_FOR_sta=$(OPTIONS_O2) -DSTATS
COMPILE_OPTIONS_FOR_seq=$(OPTIONS_O2) -DSTATS -DSEQUENTIAL_ELISION
COMPILE_OPTIONS_FOR_opt=$(OPTIONS_O2)
COMPILE_OPTIONS_FOR_cilk=$(OPTIONS_cilk) $(OPTIONS_O2)

COMPILE_OPTIONS_FOR_opt_rng=$(OPTIONS_O2) -DUSE_STL_RANDGEN

COMPILE_OPTIONS_FOR_opt_4_8=$(COMPILE_OPTIONS_FOR_opt)    -DDYNTREEOPT_BRANCHING_FACTOR=4  -DDYNTREEOPT_AMORTIZATION_FACTOR=8
COMPILE_OPTIONS_FOR_opt_4_64=$(COMPILE_OPTIONS_FOR_opt)   -DDYNTREEOPT_BRANCHING_FACTOR=4  -DDYNTREEOPT_AMORTIZATION_FACTOR=64
COMPILE_OPTIONS_FOR_opt_4_128=$(COMPILE_OPTIONS_FOR_opt)  -DDYNTREEOPT_BRANCHING_FACTOR=4  -DDYNTREEOPT_AMORTIZATION_FACTOR=128
COMPILE_OPTIONS_FOR_opt_12_8=$(COMPILE_OPTIONS_FOR_opt)   -DDYNTREEOPT_BRANCHING_FACTOR=12 -DDYNTREEOPT_AMORTIZATION_FACTOR=8
COMPILE_OPTIONS_FOR_opt_12_64=$(COMPILE_OPTIONS_FOR_opt)  -DDYNTREEOPT_BRANCHING_FACTOR=12 -DDYNTREEOPT_AMORTIZATION_FACTOR=64
COMPILE_OPTIONS_FOR_opt_12_128=$(COMPILE_OPTIONS_FOR_opt) -DDYNTREEOPT_BRANCHING_FACTOR=12 -DDYNTREEOPT_AMORTIZATION_FACTOR=128

# Folders where to find all the header files and main sources

INCLUDES=. $(SEQUTIL_PATH) $(PARUTIL_PATH) $(SCHED_PATH) $(PBBS_PATH) $(MALLOC_COUNT_PATH)

# Folders where to find all the source files

FOLDERS=$(INCLUDES)


####################################################################
# Targets

PBENCH_OCAMLCOMP=ocamlc

ifeq ($(PBENCH_PATH),)
   PBENCH_PATH=~/pbench
endif

include $(PBENCH_PATH)/Makefile_common

all: opt

opt: $(PROGRAMS:.cpp=.opt)

fib.all: $(call all_modes_for,fib)

# example
# make fib.opt && make benchtools
# ./fib.sta -n 43 -cutoff 27 -proc 40
# ./run -prog ./fib.opt -n 43 -cutoff 27 -proc 0,1,2,3,4
# ./plot --speedup --open


####################################################################
# Clean

clean_generated:
	rm -Rf generated/*

clean: clean_build clean_modes clean_generated


####################################################################
# Main rules for the makefile

include $(TOOLS_BUILD_FOLDER)/Makefile_modes

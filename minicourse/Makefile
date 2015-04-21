
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
USE_HWLOC=
USE_NUMA=0

# todo activate? USE_ANSI=1

USE_FATAL_ERRORS=1
VERBOSE_MAKE=0
VERBOSE_DEPENDENCIES=0

PROGRAMS=\
	example.cpp \
	check.cpp \
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

MODES=opt optfp elision baseline log dbg dbgfp dbgfs cilk

# Compilation options for each mode

COMPILE_OPTIONS_COMMON=-DDISABLE_INTERRUPTS -DSTATS_IDLE -DDISABLE_CONG_PSEUDODFS $(OPTIONS_ALL) $(OTHER_OPTIONS)
COMPILE_OPTIONS_FOR_opt=$(OPTIONS_O2)
COMPILE_OPTIONS_FOR_optfp=$(OPTIONS_O2) -DCONTROL_BY_FORCE_PARALLEL
COMPILE_OPTIONS_FOR_elision=$(OPTIONS_O2) -DSEQUENTIAL_ELISION
COMPILE_OPTIONS_FOR_baseline=$(OPTIONS_O2) -DSEQUENTIAL_BASELINE
COMPILE_OPTIONS_FOR_log=$(OPTIONS_O2) -DSTATS -DLOGGING
COMPILE_OPTIONS_FOR_dbg=$(OPTIONS_DEBUG) -DSTATS -DDEBUG
COMPILE_OPTIONS_FOR_dbgfp=$(OPTIONS_DEBUG) -DSTATS -DDEBUG -DCONTROL_BY_FORCE_PARALLEL
COMPILE_OPTIONS_FOR_dbgfs=$(OPTIONS_DEBUG) -DSTATS -DDEBUG -DCONTROL_BY_FORCE_SEQUENTIAL
COMPILE_OPTIONS_FOR_cilk=$(OPTIONS_cilk) $(OPTIONS_O2)

# Folders where to find all the header files and main sources

INCLUDES=. $(SEQUTIL_PATH) $(PARUTIL_PATH) $(SCHED_PATH) $(PBBS_PATH) $(MALLOC_COUNT_PATH) $(QUICKCHECK_PATH)

# Folders where to find all the source files

FOLDERS=$(INCLUDES)


####################################################################
# Targets

all: opt

opt: $(PROGRAMS:.cpp=.opt)

####################################################################
# Clean

clean_generated:
	rm -Rf generated/*

clean: clean_build clean_modes clean_generated


####################################################################
# Main rules for the makefile

include $(TOOLS_BUILD_FOLDER)/Makefile_modes

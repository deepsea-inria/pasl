
####################################################################
# Configuration

# Paths to auxiliary Makefile definitions

TOOLS_BUILD_FOLDER=../../tools/build

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

MODES=exe

# Compilation options for each mode

COMPILE_OPTIONS_COMMON=$(OPTIONS_ALL)
COMPILE_OPTIONS_FOR_exe=$(OPTIONS_DEBUG) -DSTATS -DSEQUENTIAL_ELISION

# Folders where to find all the header files and main sources

INCLUDES=. ../include/ ../bench/ $(SEQUTIL_PATH) $(PARUTIL_PATH) $(SCHED_PATH) $(CHUNKEDSEQ_PATH) $(QUICKCHECK_PATH) $(MATRIX_MARKET_PATH) $(PBBS_PATH)

# Folders where to find all the source files

FOLDERS=$(INCLUDES)


####################################################################
# Targets

all: progs

progs: tests.exe

test: progs
	./tests.exe

check_graph: tests.exe
	valgrind --leak-check=full ./tests.exe


####################################################################
# Clean

clean: clean_build clean_modes


####################################################################
# Main rules for the makefile

include $(TOOLS_BUILD_FOLDER)/Makefile_modes




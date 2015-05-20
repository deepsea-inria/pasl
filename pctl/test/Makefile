
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

MODES=dbg opt2 elision2 sta cilk log mct2 dbgfp dbgfs
OTHER_MODES=

# for debugging faster, add to settings.sh the line (for whatever extensions are needed): 
#    MY_MODES=log opt2  

ifeq ($(MY_MODES),)
else
   MODES=$(MY_MODES)
endif

# Compilation options for each mode

# we deactivate jemalloc in debug mode
COMPILE_OPTIONS_COMMON=$(OPTIONS_COMPILATION) $(OPTIONS_ARCH_DEPENDENT) $(OPTIONS_PARALLELISM) $(OPTIONS_EXTRA_TOOLS) 

SKIP_FOR_PAR=-DSKIP_OTHER_CHUNKED_SEQ -DSKIP_OTHER_SEQUENTIAL -DSKIP_OTHER_FRONTIERS -DPASL_PCONTAINER_CHUNK_CAPACITY=1024 $(MY_SKIPS)

COMPILE_OPTIONS_FOR_elision2=$(SKIP_FOR_PAR) $(OPTIONS_O2) $(OPTIONS_ALLOCATORS) -DSEQUENTIAL_ELISION
COMPILE_OPTIONS_FOR_dbg=$(SKIP_FOR_PAR) $(OPTIONS_DEBUG) -DSTATS $(FASTER) -DUSE_UCONTEXT -DDISABLE_INTERRUPTS -DDEBUG_OPTIM_STRATEGY 
COMPILE_OPTIONS_FOR_dbgfp=$(COMPILE_OPTIONS_FOR_dbg) -DCONTROL_BY_FORCE_PARALLEL
COMPILE_OPTIONS_FOR_dbgfs=$(COMPILE_OPTIONS_FOR_dbg) -DCONTROL_BY_FORCE_SEQUENTIAL
COMPILE_OPTIONS_FOR_opt2=$(SKIP_FOR_PAR) $(OPTIONS_O2) $(OPTIONS_ALLOCATORS) $(OPTIONS_HWLOC_ALL)
COMPILE_OPTIONS_FOR_sta=$(SKIP_FOR_PAR) $(OPTIONS_O2) $(OPTIONS_ALLOCATORS) -DSTATS $(OPTIONS_HWLOC_ALL)
COMPILE_OPTIONS_FOR_log=$(SKIP_FOR_PAR) $(OPTIONS_O2) $(OPTIONS_ALLOCATORS) -DSTATS -DLOGGING
COMPILE_OPTIONS_FOR_cilk=$(SKIP_FOR_PAR) $(OPTIONS_cilk) $(OPTIONS_O2) $(OPTIONS_ALLOCATORS)
COMPILE_OPTIONS_FOR_mct2=$(SKIP_FOR_PAR) $(OPTIONS_O2) $(OPTIONS_MALLOC_COUNT)

COMPILE_OPTIONS_FOR_all_opt2=$(OPTIONS_O2) $(OPTIONS_ALLOCATORS)

malloc_count:
	make -C ../../tools/malloc_count


####################################################################
# Folders

INCLUDES=. ../include/ ./include/ ../bench/include/ ../example/include/ ../bench/sequential/ $(SEQUTIL_PATH) $(PARUTIL_PATH) $(SCHED_PATH) $(CHUNKEDSEQ_PATH) $(QUICKCHECK_PATH) $(MATRIX_MARKET_PATH) $(MALLOC_COUNT_PATH)

FOLDERS=$(INCLUDES)

####################################################################
# Main rules for the makefile

include $(TOOLS_BUILD_FOLDER)/Makefile_modes


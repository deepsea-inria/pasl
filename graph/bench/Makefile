
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

# for debugging faster, place #ifdef SKIP_FAST in search.hppn, and add to settings.sh the following line:
# MY_SKIPS=-DSKIP_FAST
# also possible options: -DSKIP_32_BITS -DSKIP_64_BITS 

# Include here

# Options are then configured by the auxiliary file below

include $(TOOLS_BUILD_FOLDER)/Makefile_options


####################################################################
# Modes

# What are the compilation mode supported, i.e. the "modes"
# (If extending the list, need to add cases for the definition
# of COMPILE_OPTIONS_FOR further below, and also for "clean".

MODES=dbg opt2 elision2 sta cilk log all_opt2 idle opt2_seq_init cilk_seq_init
OTHER_MODES=pref opt3 elision3 

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
COMPILE_OPTIONS_FOR_elision3=$(SKIP_FOR_PAR) $(OPTIONS_O3) $(OPTIONS_ALLOCATORS) -DSEQUENTIAL_ELISION 
COMPILE_OPTIONS_FOR_dbg=$(SKIP_FOR_PAR) $(OPTIONS_DEBUG) -DSTATS $(FASTER) -DUSE_UCONTEXT -DDISABLE_INTERRUPTS -DDEBUG_OPTIM_STRATEGY 
COMPILE_OPTIONS_FOR_opt2=$(SKIP_FOR_PAR) $(OPTIONS_O2) $(OPTIONS_ALLOCATORS) $(OPTIONS_HWLOC_ALL)
COMPILE_OPTIONS_FOR_opt3=$(SKIP_FOR_PAR) $(OPTIONS_O3) $(OPTIONS_ALLOCATORS)
COMPILE_OPTIONS_FOR_sta=$(SKIP_FOR_PAR) $(OPTIONS_O2) $(OPTIONS_ALLOCATORS) -DSTATS $(OPTIONS_HWLOC_ALL)
COMPILE_OPTIONS_FOR_idle=$(SKIP_FOR_PAR) $(OPTIONS_O2) $(OPTIONS_ALLOCATORS) -DSTATS_IDLE
COMPILE_OPTIONS_FOR_log=$(SKIP_FOR_PAR) $(OPTIONS_O2) $(OPTIONS_ALLOCATORS) -DSTATS -DLOGGING
COMPILE_OPTIONS_FOR_pref=$(SKIP_FOR_PAR) $(OPTIONS_O2) $(OPTIONS_ALLOCATORS) -DSTATS -DUSE_PREFETCHING
COMPILE_OPTIONS_FOR_cilk=$(SKIP_FOR_PAR) $(OPTIONS_cilk) $(OPTIONS_O2) $(OPTIONS_ALLOCATORS)

COMPILE_OPTIONS_FOR_opt2_seq_init=$(SKIP_FOR_PAR) $(OPTIONS_O2) $(OPTIONS_ALLOCATORS) -DFILL_ARRAY_PAR_SEQ
COMPILE_OPTIONS_FOR_cilk_seq_init=$(SKIP_FOR_PAR) $(OPTIONS_cilk)  $(OPTIONS_O2) -DFILL_ARRAY_PAR_SEQ

COMPILE_OPTIONS_FOR_all_opt2=$(OPTIONS_O2) $(OPTIONS_ALLOCATORS)


####################################################################
# Folders

INCLUDES=. ../include/ $(SEQUTIL_PATH) $(PARUTIL_PATH) $(SCHED_PATH) $(CHUNKEDSEQ_PATH) $(QUICKCHECK_PATH) $(MATRIX_MARKET_PATH) $(PBBS_PATH) $(MALLOC_COUNT_PATH)

FOLDERS=$(INCLUDES)


####################################################################
# Targets

all: progs

# DEPRECATED (see below for better way): progs: search.opt2 search.opt2 search.elision2 search.dbg graphfile.opt2 graphfile.opt3 graphfile.elision2 graphfile.dbg

progs: $(call all_modes_for,search graphfile)

temp: search.dbg

# debug dependencies:  make graphfile.elision2.show_depend


####################################################################
# Clean

clean: clean_build clean_modes


####################################################################
# Main rules for the makefile

include $(TOOLS_BUILD_FOLDER)/Makefile_modes


####################################################################
#

# TODO FOR LATER

# $(USE_CORD)
ifeq (0,1)
	CORD=/home/mrainey/Installs/gc-7.0
	C_FLAGS += -DHAVE_CORD
	LD_FLAGS += -lcord -lgccpp -lgc
	INCLUDES += -I$(CORD)/include
	LIBS += -L/home/mrainey/Installs/boehm-gc/lib
endif


####################################################################
#  study

PBENCH_OCAMLCOMP=ocamlc

ifeq ($(PBENCH_PATH),)
   PBENCH_PATH=~/pbench
endif

include $(PBENCH_PATH)/Makefile_common

graph: graph.pbench
	ln -sf $< $@ 

overview: graph
	./graph overview -size large -proc 40

overview_plot: graph
	./graph overview -size large -proc 40 -only plot

overview_plot_old: graph
	./graph overview_old -size large -proc 40 -only plot

ligra_comp: graph
	./graph ligra -size small -proc 40 -skip make

clean: pbench_clean
	

####################################################################
#  study --DEPRECATED

old_study:
	make -C $(PBENCH_PATH) pbench
	$(PBENCH_PATH)/pbench -open study.pbh "pbfs()"






# ./graph overview -size large -proc 40 -skip make --virtual_run

##valgrind option: --demangle=no

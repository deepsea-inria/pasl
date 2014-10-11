
####################################################################
# Configuration

# Paths to auxiliary Makefile definitions

TOOLS_BUILD_FOLDER=../../tools/build


####################################################################
# Default options

USE_ALLOCATOR=
USE_FATAL_ERRORS=1

PROGRAMS=bench.cpp do_fifo.cpp


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

KEYS=exe dbg mct
KINDS=full fifolifo chunksize filter splitmerge map
MODES=$(KEYS) $(foreach key,$(KEYS),$(addprefix $(key)_,$(KINDS))) 


#ifeq ($(BOOTSEQ),1)
#	KEYS=exe dbg
#	KINDS=bootseq
#endif

# Compilation options for each mode

COMPILE_OPTIONS_COMMON=$(OPTIONS_COMPILATION) $(OPTIONS_ARCH_DEPENDENT) $(OPTIONS_PARALLELISM) $(OPTIONS_EXTRA_TOOLS) 

#COMPILE_OPTIONS_COMMON=$(OPTIONS_ALL) $(OPTIONS_SETTINGS_SH)
OPTIONS_dbg=$(OPTIONS_DEBUG)
OPTIONS_exe=$(OPTIONS_O2) $(OPTIONS_ALLOCATORS)
OPTIONS_mct=$(OPTIONS_O2) $(OPTIONS_MALLOC_COUNT)

SKIP_STRUCT=-DSKIP_DEQUE -DSKIP_CHUNKEDSEQ -DSKIP_CHUNKEDSEQ_OPT -DSKIP_CHUNKEDSEQ_RINGBUFFER -DSKIP_MAP
SKIP_ALL=$(SKIP_STRUCT) -DSKIP_ITEMSIZE -DSKIP_CHUNKSIZE

# helper function to substract $1 flags from the list above
exclude_flags=$(filter-out $1,$(SKIP_ALL))


PARAMS_full=
PARAMS_fifolifo=$(call exclude_flags,-DSKIP_DEQUE -DSKIP_CHUNKEDSEQ)
PARAMS_chunksize=$(call exclude_flags,-DSKIP_DEQUE -DSKIP_CHUNKEDSEQ -DSKIP_CHUNKSIZE)
PARAMS_filter=$(call exclude_flags,-DSKIP_DEQUE -DSKIP_CHUNKEDSEQ) -DHAVE_ROPE
PARAMS_splitmerge=$(call exclude_flags,-DSKIP_DEQUE -DSKIP_CHUNKEDSEQ) -DHAVE_ROPE
PARAMS_map=$(call exclude_flags,-DSKIP_DEQUE -DSKIP_CHUNKEDSEQ -DSKIP_MAP)


# generate all modes:
$(foreach key,$(KEYS),$(eval COMPILE_OPTIONS_FOR_$(key)=$(OPTIONS_$(key))))
$(foreach key,$(KEYS),$(foreach kind,$(KINDS),$(eval COMPILE_OPTIONS_FOR_$(key)_$(kind)=$(OPTIONS_$(key)) $(PARAMS_$(kind)))))


####################################################################
# Folders

INCLUDES=. ../include/ ../examples/ $(SEQUTIL_PATH) $(MALLOC_COUNT_PATH)

FOLDERS=$(INCLUDES)


####################################################################
# Targets

all: progs

progs: bench.exe_full

fifo: fifolifo

# bench.dbg_fifolifo
# chunk: bench.exe_chunksize
#	cp $< bench.exe

bench: bench.exe_filter bench.exe_fifolifo bench.exe_chunksize bench.exe_splitmerge bench.exe_map


do_fifo : do_fifo.exe_full
	cp do_fifo.exe_full do_fifo.exe

# g++ -std=gnu++11 -O2 -DNDEBUG -D_GNU_SOURCE -fpermissive -Wno-format -Wfatal-errors -m64 -DTARGET_X86_64 -DTARGET_LINUX -I . -I ../include/ -I ../../tools/build//../../sequtil -I ../../tools/build//../../tools/malloc_count -I _build/exe_full _build/exe_full/cmdline.o _build/exe_full/atomic.o _build/exe_full/microtime.o -o do_fifo.exe_full do_fifo.cpp

####################################################################
# Automatic Targets

# For example, "make chunk" builds "bench.exe_chunk" and copy it to "bench.exe"

define target-for-kind
$1: bench.exe_$1
	cp bench.exe_$1 bench.exe
endef

$(foreach kind,$(KINDS),$(eval $(call target-for-kind,$(kind))))


####################################################################
# Clean

clean: clean_build clean_modes


####################################################################
# Main rules for the makefile

include $(TOOLS_BUILD_FOLDER)/Makefile_modes


####################################################################





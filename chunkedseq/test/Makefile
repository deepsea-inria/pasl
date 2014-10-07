
####################################################################
# Configuration

# Paths to auxiliary Makefile definitions

TOOLS_BUILD_FOLDER=../../tools/build


####################################################################
# Makefile options

# Create a file called "settings.sh" in this folder if you want to
# configure particular options. See section below for options.

-include settings.sh

# Options are then configured by the auxiliary file below

include $(TOOLS_BUILD_FOLDER)/Makefile_options


####################################################################
# Modes

# What are the compilation mode supported, i.e. the "modes"
# (If extending the list, need to add cases for the definition
# of COMPILE_OPTIONS_FOR further below, and also for "clean".

MODES=exe

# Compilation options for each mode

COMPILE_OPTIONS_COMMON=$(OPTIONS_ALL) $(OTHER_CFLAGS)
COMPILE_OPTIONS_FOR_exe=$(OPTIONS_DEBUG)

# Folders where to find all the header files and main sources

INCLUDES=. ../include/ ../examples/ $(SEQUTIL_PATH) $(MALLOC_COUNT_PATH) $(QUICKCHECK_PATH) 

# Folders where to find all the source files

FOLDERS=$(INCLUDES)


####################################################################
# Targets

all: progs

progs: quickcheck_chunkedseq.exe

tests: progs
	valgrind ./quickcheck_chunkedseq.exe 

####################################################################
# Aliases


# valgrind ./test_chunkedseq.exe -only push_pop_lifo

# valgrind ./test_bootchunkedseq.exe -only push_pop_lifo


# make check_chunkedbag && valgrind ./test_chunkedbag.exe -only concat
# push_pop_lifo




####################################################################
# Clean

clean: clean_build clean_modes


####################################################################
# Main rules for the makefile

include $(TOOLS_BUILD_FOLDER)/Makefile_modes

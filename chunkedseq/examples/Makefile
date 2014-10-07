
####################################################################
# Configuration

# Paths to auxiliary Makefile definitions

TOOLS_BUILD_FOLDER=../../tools/build


####################################################################
# Default options

USE_ALLOCATOR=
USE_FATAL_ERRORS=1

PROGRAMS=chunkedseq_1.cpp chunkedseq_2.cpp chunkedseq_3.cpp chunkedseq_4.cpp \
	chunkedseq_5.cpp chunkedseq_6.cpp chunkedseq_7.cpp \
	iterator_1.cpp map_1.cpp segment_1.cpp weighted_split.cpp


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

MODES=exe

COMPILE_OPTIONS_COMMON=$(OPTIONS_ALL) $(OPTIONS_SETTINGS_SH)
OPTIONS_exe=$(OPTIONS_O2)

####################################################################
# Folders

INCLUDES=. ../include/ $(SEQUTIL_PATH) $(MALLOC_COUNT_PATH)

FOLDERS=$(INCLUDES)


####################################################################
# Targets

all: progs

progs: 

####################################################################
# Clean

clean: clean_build clean_modes


####################################################################
# Main rules for the makefile

include $(TOOLS_BUILD_FOLDER)/Makefile_modes


####################################################################






####################################################################
# Configuration

# Paths to auxiliary Makefile definitions

TOOLS_BUILD_FOLDER=..


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

MODES=exe dbg

# Compilation options for each mode

COMPILE_OPTIONS_COMMON=$(OPTIONS_ALL)
COMPILE_OPTIONS_FOR_exe=-O3 -DNDEBUG
COMPILE_OPTIONS_FOR_dbg=-O0 -g -DDEBUG

# Folders where to find all the header files and main sources

INCLUDES=. inc

# Folders where to find all the source files

FOLDERS=$(INCLUDES) src


####################################################################
# Targets

all: $(addprefix main., $(MODES))


####################################################################
# Clean

clean: clean_build clean_modes


####################################################################
# Main rules for the makefile

include $(TOOLS_BUILD_FOLDER)/Makefile_modes






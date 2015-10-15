
####################################################################
# Configuration

# Paths to auxiliary Makefile definitions

TOOLS_BUILD_FOLDER=../build

####################################################################
# Makefile options

# Create a file called "settings.sh" in this folder if you want to 
# configure particular options. See section below for options.

-include settings.sh

# Include here

# Options are then configured by the auxiliary file below

include $(TOOLS_BUILD_FOLDER)/Makefile_options


OBJS = bump_malloc.o

COMPILE_OPTIONS_COMMON=$(OPTIONS_COMPILATION) $(OPTIONS_ARCH_DEPENDENT)  -pthread

all: $(OBJS)

%.o: %.c
	$(COMPILER) $(COMPILE_OPTIONS_COMMON) -c -o $@ $<

clean:
	rm -f *.o

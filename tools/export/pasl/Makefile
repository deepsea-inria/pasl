
########################################################"
# Folders

PASL_ROOT_PATH=../../..
BUILD_PREFIX=_build
TEST_PREFIX=_test

########################################################"
# Files to export and produce

SOURCE_FOLDERS=sequtil parutil sched example chunkedseq graph benchtools tools/build tools/jemalloc tools/tcmalloc tools/quickcheck tools/pbbs tools/malloc_count tools/matrix_market tools/hwloc
EXTRA_FILES=README
EXCLUDE_PATHS=tools/build/demo

ARCHIVE_BASE=pasl-20140625
ARCHIVE_NAME=$(ARCHIVE_BASE).tar.gz
BUILD_PATH=$(BUILD_PREFIX)/$(ARCHIVE_BASE)



########################################################"
# Targets

all: archive
# all: archive test

archive: $(ARCHIVE_NAME)

force:

testing:
	echo go
	FOO=`svn ls -R $(PASL_ROOT_PATH)/tools/build`
	echo go: $(FOO)

publish: archive
	scp $(ARCHIVE_NAME) yquem.inria.fr:/home/nas/gallium/rainey/public_html


########################################################"
# Archive creation files target

$(ARCHIVE_NAME): force
	rm -Rf $(BUILD_PREFIX)
	mkdir -p $(BUILD_PATH)
	$(foreach folder,$(SOURCE_FOLDERS),mkdir -p $(dir $(BUILD_PATH)/$(folder)); svn export $(PASL_ROOT_PATH)/$(folder) $(BUILD_PATH)/$(folder); )
	$(foreach file,$(EXTRA_FILES),cp $(file) $(BUILD_PATH)/$(file); )
	cd $(BUILD_PREFIX); tar -czf ../$(ARCHIVE_NAME) $(ARCHIVE_BASE)


########################################################"
# Testing target

# todo: test execution of the program

test: force
	rm -Rf $(TEST_PREFIX)
	mkdir -p $(TEST_PREFIX)
	cp $(ARCHIVE_NAME) $(TEST_PREFIX)
	cd $(TEST_PREFIX); tar -xzf $(ARCHIVE_NAME)
	make -j -C $(TEST_PREFIX)/$(ARCHIVE_BASE)/example/fib.opt


########################################################"
# Cleaning

clean:
	rm -Rf $(BUILD_PREFIX) $(TEST_PREFIX) $(ARCHIVE_NAME)



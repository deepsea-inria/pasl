
########################################################"
# Folders

PASL_ROOT_PATH=../../../
PBENCH_ROOT_PATH=$(PASL_ROOT_PATH)../pbench/

########################################################"
# Files to export and produce


CHUNKEDSEQ_SOURCE_FOLDERS=sequtil tools/quickcheck tools/build chunkedseq
CHUNKEDSEQ_BUILD_PREFIX=_build
CHUNKEDSEQ_ARCHIVE_BASE=chunkedseq
CHUNKEDSEQ_ARCHIVE_NAME=$(CHUNKEDSEQ_ARCHIVE_BASE).tar.gz
CHUNKEDSEQ_BUILD_PATH=$(CHUNKEDSEQ_BUILD_PREFIX)/$(CHUNKEDSEQ_ARCHIVE_BASE)
CHUNKEDSEQ_TEST_PREFIX=_test

CHUNKEDSEQ_BENCH_SOURCE_FOLDERS=$(CHUNKEDSEQ_SOURCE_FOLDERS) parutil sched graph tools/pbbs tools/matrix_market tools/malloc_count
CHUNKEDSEQ_BENCH_BUILD_PREFIX=$(CHUNKEDSEQ_BUILD_PREFIX)-bench
CHUNKEDSEQ_BENCH_TEST_PREFIX=$(CHUNKEDSEQ_TEST_PREFIX)-bench
CHUNKEDSEQ_BENCH_ARCHIVE_BASE=chunkedseq_bench
CHUNKEDSEQ_BENCH_ARCHIVE_NAME=$(CHUNKEDSEQ_BENCH_ARCHIVE_BASE).tar.gz
CHUNKEDSEQ_BENCH_BUILD_PATH=$(CHUNKEDSEQ_BENCH_BUILD_PREFIX)/$(CHUNKEDSEQ_BENCH_ARCHIVE_BASE)

EXTRA_FILES=README
EXCLUDE_PATHS=tools/build/demo

########################################################"
# Targets

all: archive

archive: $(CHUNKEDSEQ_ARCHIVE_NAME) $(CHUNKEDSEQ_BENCH_ARCHIVE_NAME)

force:

testing:
	echo go
	FOO=`svn ls -R $(PASL_ROOT_PATH)/tools/build`
	echo go: $(FOO)


########################################################"
# Archive creation files target

$(CHUNKEDSEQ_ARCHIVE_NAME): force
	rm -Rf $(CHUNKEDSEQ_BUILD_PREFIX)
	mkdir -p $(CHUNKEDSEQ_BUILD_PATH)
	$(foreach folder,$(CHUNKEDSEQ_SOURCE_FOLDERS),mkdir -p $(dir $(CHUNKEDSEQ_BUILD_PATH)/$(folder)); svn export $(PASL_ROOT_PATH)/$(folder) $(CHUNKEDSEQ_BUILD_PATH)/$(folder); )
	$(foreach file,$(EXTRA_FILES),cp $(file) $(CHUNKEDSEQ_BUILD_PATH)/$(file); )
	cd $(CHUNKEDSEQ_BUILD_PREFIX); tar -czf ../$(CHUNKEDSEQ_ARCHIVE_NAME) $(CHUNKEDSEQ_ARCHIVE_BASE)

$(CHUNKEDSEQ_BENCH_ARCHIVE_NAME): force
	rm -Rf $(CHUNKEDSEQ_BENCH_BUILD_PREFIX)
	mkdir -p $(CHUNKEDSEQ_BENCH_BUILD_PATH)
	$(foreach folder,$(CHUNKEDSEQ_BENCH_SOURCE_FOLDERS),mkdir -p $(dir $(CHUNKEDSEQ_BENCH_BUILD_PATH)/$(folder)); svn export $(PASL_ROOT_PATH)/$(folder) $(CHUNKEDSEQ_BENCH_BUILD_PATH)/$(folder); )
	svn export $(PBENCH_ROOT_PATH)/trunk $(CHUNKEDSEQ_BENCH_BUILD_PATH)/pbench
	$(foreach file,$(EXTRA_FILES),cp $(file) $(CHUNKEDSEQ_BENCH_BUILD_PATH)/$(file); )
	cd $(CHUNKEDSEQ_BENCH_BUILD_PREFIX); tar -czf ../$(CHUNKEDSEQ_BENCH_ARCHIVE_NAME) $(CHUNKEDSEQ_BENCH_ARCHIVE_BASE)



########################################################"
# Testing target

test: force
#todo

########################################################"
# Cleaning

clean:
	rm -Rf $(CHUNKEDSEQ_BUILD_PREFIX) $(CHUNKEDSEQ_TEST_PREFIX) $(CHUNKEDSEQ_ARCHIVE_NAME)
	rm -Rf $(CHUNKEDSEQ_BENCH_BUILD_PREFIX) $(CHUNKEDSEQ_BENCH_TEST_PREFIX) $(CHUNKEDSEQ_BENCH_ARCHIVE_NAME)



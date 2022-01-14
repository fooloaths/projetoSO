# Makefile, v1
# Sistemas Operativos, DEI/IST/ULisboa 2021-22
#
# This makefile should be run from the *root* of the project

CC ?= gcc
LD ?= gcc

# space separated list of directories with header files
INCLUDE_DIRS := fs .
# this creates a space separated list of -I<dir> where <dir> is each of the values in INCLUDE_DIRS
INCLUDES = $(addprefix -I, $(INCLUDE_DIRS))

SOURCES  := $(wildcard */*.c)
HEADERS  := $(wildcard */*.h)
OBJECTS  := $(SOURCES:.c=.o)
TARGET_EXECS := tests/truncate tests/test1 tests/copy_to_external_simple tests/copy_to_external_errors tests/write_10_blocks_spill tests/write_10_blocks_simple tests/write_more_than_10_blocks_simple bateria_mt/mt_test_10_files bateria_mt/no_mt_10_files bateria_mt/mt_test_10_times_same_file bateria_mt/no_mt_10_times bateria_mt/mt_test_100_reads_same_file bateria_mt/mt_test_copy_to_external bateria_mt/mt_test_copy_to_external_same_tfs_file bateria_mt/mt_test_20_reads_different_files tests/goncalo_test #bateria_mt/mt_test_delete_file

# VPATH is a variable used by Makefile which finds *sources* and makes them available throughout the codebase
# vpath %.h <DIR> tells make to look for header files in <DIR>
vpath # clears VPATH
vpath %.h $(INCLUDE_DIRS)

LDLIBS = -lpthread

CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L
CFLAGS += $(INCLUDES)

# Warnings
CFLAGS += -fdiagnostics-color=always -Wall -Werror -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused
# Warning suppressions
CFLAGS += -Wno-sign-compare

# optional debug symbols: run make DEBUG=no to deactivate them
ifneq ($(strip $(DEBUG)), no)
  CFLAGS += -g
endif

# optional O3 optimization symbols: run make OPTIM=no to deactivate them
ifeq ($(strip $(OPTIM)), no)
  CFLAGS += -O0
else
  CFLAGS += -O3
endif

# A phony target is one that is not really the name of a file
# https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean depend fmt

all: $(TARGET_EXECS)


# The following target can be used to invoke clang-format on all the source and header
# files. clang-format is a tool to format the source code based on the style specified 
# in the file '.clang-format'.
# More info available here: https://clang.llvm.org/docs/ClangFormat.html

# The $^ keyword is used in Makefile to refer to the right part of the ":" in the 
# enclosing rule. See https://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

fmt: $(SOURCES) $(HEADERS)
	clang-format -i $^

# Note the lack of a rule.
# make uses a set of default rules, one of which compiles C binaries
# the CC, LD, CFLAGS and LDFLAGS are used in this rule
tests/truncate: tests/truncate.o fs/operations.o fs/state.o
tests/test1: tests/test1.o fs/operations.o fs/state.o
tests/copy_to_external_errors: tests/copy_to_external_errors.o fs/operations.o fs/state.o
tests/copy_to_external_simple: tests/copy_to_external_simple.o fs/operations.o fs/state.o
tests/write_10_blocks_spill: tests/write_10_blocks_spill.o fs/operations.o fs/state.o
tests/write_10_blocks_simple: tests/write_10_blocks_simple.o fs/operations.o fs/state.o
tests/write_more_than_10_blocks_simple: tests/write_more_than_10_blocks_simple.o fs/operations.o fs/state.o
bateria_mt/mt_test_10_files: bateria_mt/mt_test_10_files.o fs/operations.o fs/state.o
bateria_mt/no_mt_10_files: bateria_mt/no_mt_10_files.o fs/operations.o fs/state.o
bateria_mt/mt_test_10_times_same_file: bateria_mt/mt_test_10_times_same_file.o fs/operations.o fs/state.o
bateria_mt/no_mt_10_times: bateria_mt/no_mt_10_times.o fs/operations.o fs/state.o
bateria_mt/mt_test_100_reads_same_file: bateria_mt/mt_test_100_reads_same_file.o fs/operations.o fs/state.o
bateria_mt/mt_test_copy_to_external: bateria_mt/mt_test_copy_to_external.o fs/operations.o fs/state.o
bateria_mt/mt_test_copy_to_external_same_tfs_file: bateria_mt/mt_test_copy_to_external_same_tfs_file.o fs/operations.o fs/state.o
bateria_mt/mt_test_20_reads_different_files: bateria_mt/mt_test_20_reads_different_files.o fs/operations.o fs/state.o
#bateria_mt/mt_test_delete_file: bateria_mt/mt_test_delete_file.o fs/operations.o fs/state.o
tests/goncalo_test: tests/goncalo_test.o fs/operations.o fs/state.o

clean:
	rm -f $(OBJECTS) $(TARGET_EXECS)

run:
	echo "Running tests." 
	cd tests && echo "Copy to external errors." && ./copy_to_external_errors 
	cd tests && echo "Copy to external simple." && ./copy_to_external_simple 
	cd tests && echo "Test 1." && ./test1 && echo "Write 10 blocks simple." &&./write_10_blocks_simple 
	cd tests && echo "Write 10 blocks spill." && ./write_10_blocks_spill 
	cd tests && echo "Write more than 10 blocks simple." && ./write_more_than_10_blocks_simple 

run_all:
	echo "Running tests." 
	cd tests && echo "Copy to external errors." && ./copy_to_external_errors 
	cd tests && echo "Copy to external simple." && ./copy_to_external_simple 
	cd tests && echo "Test 1." && ./test1 && echo "Write 10 blocks simple." &&./write_10_blocks_simple 
	cd tests && echo "Write 10 blocks spill." && ./write_10_blocks_spill 
	cd bateria_mt && echo "Running MT Test" && ./mt_test_10_files
	cd bateria_mt && echo "Running No MT Test 1000 Different FIles" && ./no_mt_10_files
	cd bateria_mt && echo "Running MT Test 10 times same file" && ./mt_test_10_times_same_file
	cd tests && echo "Write more than 10 blocks simple." && ./write_more_than_10_blocks_simple 
	cd tests && echo "Goncalo Test" && ./goncalo_test
	cd tests && echo "Truncate" && ./truncate
	
run_mt:
	echo "Running tests." 
	cd bateria_mt && echo "Running No MT Test - 20 Different Files" && ./no_mt_10_files
	cd bateria_mt && echo "Running No MT Test - Write 50 Times Same File" && ./no_mt_10_times
	cd bateria_mt && echo "Running MT Test - 20 Different Files" && ./mt_test_10_files
	cd bateria_mt && echo "Running MT Test - Wrote 50 Times Same file" && ./mt_test_10_times_same_file
	cd bateria_mt && echo "Running MT Test - 100 Reads Same File" && ./mt_test_100_reads_same_file
	cd bateria_mt && echo "Running MT Test - Copy to External (20 Dif File Writes Then Reads)" && ./mt_test_copy_to_external
	cd bateria_mt && echo "Running MT Test - Copy to External Same TFS File" && ./mt_test_copy_to_external_same_tfs_file
	cd bateria_mt && echo "Running MT Test - 20 Reads Different Files" && ./mt_test_20_reads_different_files


# This generates a dependency file, with some default dependencies gathered from the include tree
# The dependencies are gathered in the file autodep. You can find an example illustrating this GCC feature, without Makefile, at this URL: https://renenyffenegger.ch/notes/development/languages/C-C-plus-plus/GCC/options/MM
# Run `make depend` whenever you add new includes in your files
depend : $(SOURCES)
	$(CC) $(INCLUDES) -MM $^ > autodep

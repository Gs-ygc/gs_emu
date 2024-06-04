


PROGRAM  = $(GUEST_ISA)-minirv32ima-so
SRCS  = $(shell find src/ -name "*.c")


# Define the include directories
INCLUDE_DIRS := -I include -I $(NEMU_HOME)/include

# Define the compiler and compiler flags
CC := gcc
CFLAGS := -fPIC -shared -O2

# Define the output directory
BINDIR := build

# Define the output file
OUTPUT := $(BINDIR)/$(PROGRAM)

# Define the dependencies for the output file
$(OUTPUT):$(SRCS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $^ -o $@

all: $(OUTPUT)
.DEFAULT_GOAL = all

# Define a phony target for cleaning
.PHONY: clean
clean:
	rm -rf $(BINDIR)


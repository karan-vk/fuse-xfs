# fuse-xfs CLI tools Makefile
# Modernized for macFUSE and Apple Silicon support

TOPLEVEL=../..

include ../Make.inc

# Include paths
INCLUDES = $(XFS_INCLUDES) $(XFSUTILS_INCLUDES)

# Compiler flags
CFLAGS = $(COMMON_CFLAGS) $(INCLUDES)

# Linker flags
LDFLAGS = $(COMMON_LDFLAGS) -lreadline

# Targets
xfs-cli: $(BINS)/xfs-cli

xfs-rcopy: $(BINS)/xfs-rcopy

# Object files
$(OBJECTS)/cli.o: cli.c $(SRC)/xfsutil/xfsutil.h
	$(CC) $(CFLAGS) -o $(OBJECTS)/cli.o -c cli.c

$(OBJECTS)/rcopy.o: rcopy.c $(SRC)/xfsutil/xfsutil.h
	$(CC) $(CFLAGS) -o $(OBJECTS)/rcopy.o -c rcopy.c

# Link binaries
$(BINS)/xfs-cli: $(OBJECTS)/cli.o $(OBJECTS)/xfsutil.o $(LIBS)/libxfs.a
	$(CC) $(LDFLAGS) -o $(BINS)/xfs-cli $(OBJECTS)/cli.o $(OBJECTS)/xfsutil.o $(LIBS)/libxfs.a

$(BINS)/xfs-rcopy: $(OBJECTS)/rcopy.o $(OBJECTS)/xfsutil.o $(LIBS)/libxfs.a
	$(CC) $(COMMON_LDFLAGS) -o $(BINS)/xfs-rcopy $(OBJECTS)/rcopy.o $(OBJECTS)/xfsutil.o $(LIBS)/libxfs.a

clean:
	rm -f $(OBJECTS)/cli.o $(OBJECTS)/rcopy.o

.PHONY: xfs-cli xfs-rcopy clean
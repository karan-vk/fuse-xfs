# fuse-xfs FUSE module Makefile
# Modernized for macFUSE and Apple Silicon support

TOPLEVEL=../..

include ../Make.inc

# Include paths for compilation
INCLUDES = $(XFS_INCLUDES) $(XFSUTILS_INCLUDES) $(FUSE_INCLUDES) -I.

# Compiler flags - combine FUSE flags with common flags
CFLAGS = $(COMMON_CFLAGS) $(FUSE_CFLAGS) $(INCLUDES)

# Linker flags - combine FUSE flags with common flags
LDFLAGS = $(COMMON_LDFLAGS) $(FUSE_LDFLAGS)

# Main target
fuse-xfs: $(BINS)/fuse-xfs

# Object files
$(OBJECTS)/main_fuse.o: main.c fuse_xfs.h $(SRC)/xfsutil/xfsutil.h
	$(CC) $(CFLAGS) -o $(OBJECTS)/main_fuse.o -c main.c

$(OBJECTS)/fuse_xfs.o: fuse_xfs.c fuse_xfs.h $(SRC)/xfsutil/xfsutil.h
	$(CC) $(CFLAGS) -o $(OBJECTS)/fuse_xfs.o -c fuse_xfs.c

# Link the final binary
$(BINS)/fuse-xfs: $(OBJECTS)/main_fuse.o $(OBJECTS)/fuse_xfs.o $(OBJECTS)/xfsutil.o $(LIBS)/libxfs.a
	$(CC) $(LDFLAGS) -o $(BINS)/fuse-xfs \
		$(OBJECTS)/main_fuse.o \
		$(OBJECTS)/fuse_xfs.o \
		$(OBJECTS)/xfsutil.o \
		$(LIBS)/libxfs.a

clean:
	rm -f $(OBJECTS)/main_fuse.o $(OBJECTS)/fuse_xfs.o

.PHONY: fuse-xfs clean
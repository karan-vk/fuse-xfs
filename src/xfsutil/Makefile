# fuse-xfs XFS utilities Makefile
# Modernized for macFUSE and Apple Silicon support

TOPLEVEL=../..

include ../Make.inc

# Include paths
INCLUDES = $(XFS_INCLUDES) -I.

# Compiler flags
CFLAGS = $(COMMON_CFLAGS) $(INCLUDES)

# Object files
$(OBJECTS)/xfsutil.o: xfsutil.c xfsutil.h
	$(CC) $(CFLAGS) -o $(OBJECTS)/xfsutil.o -c xfsutil.c

clean:
	rm -f $(OBJECTS)/xfsutil.o

.PHONY: clean
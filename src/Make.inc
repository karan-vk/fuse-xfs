# fuse-xfs build configuration
# Modernized for macFUSE and Apple Silicon (ARM64) support

VERSION=0.2.1

BUILD=$(TOPLEVEL)/build
SRC=$(TOPLEVEL)/src
BINS=$(BUILD)/bin
LIBS=$(BUILD)/lib
OBJECTS=$(BUILD)/obj

# Compiler configuration
CC=gcc

# Architecture detection
ARCH := $(shell uname -m)
MACOS_VERSION := $(shell sw_vers -productVersion 2>/dev/null | cut -d. -f1)

# Set architecture-specific flags
ifeq ($(ARCH),arm64)
    # Apple Silicon (M1/M2/M3)
    ARCH_FLAGS = -arch arm64
    MACOS_MIN_VERSION = 11.0
else ifeq ($(ARCH),x86_64)
    # Intel Mac
    ARCH_FLAGS = -arch x86_64
    MACOS_MIN_VERSION = 10.15
else
    # Fallback for other architectures
    ARCH_FLAGS =
    MACOS_MIN_VERSION = 10.15
endif

# Universal binary support (uncomment to build for both architectures)
# ARCH_FLAGS = -arch arm64 -arch x86_64
# MACOS_MIN_VERSION = 11.0

# macOS deployment target
MACOS_FLAGS = -mmacosx-version-min=$(MACOS_MIN_VERSION)

# FUSE configuration - macFUSE (successor to osxfuse)
# Try pkg-config first, fall back to standard macFUSE paths
FUSE_PKG_CONFIG := $(shell pkg-config --exists fuse 2>/dev/null && echo yes || echo no)

ifeq ($(FUSE_PKG_CONFIG),yes)
    # Use pkg-config for FUSE configuration (preferred)
    FUSE_CFLAGS_PKG := $(shell pkg-config --cflags fuse 2>/dev/null)
    FUSE_LDFLAGS_PKG := $(shell pkg-config --libs fuse 2>/dev/null)
    FUSE_CFLAGS = $(FUSE_CFLAGS_PKG) -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26
    FUSE_LDFLAGS = $(FUSE_LDFLAGS_PKG)
    FUSE_INCLUDES = $(FUSE_CFLAGS_PKG)
else
    # Fallback to standard macFUSE installation paths
    # macFUSE 4.x installs to /usr/local/include/fuse (not osxfuse)
    # Also check for macFUSE in /opt/local (MacPorts) and Homebrew locations
    
    # Check for macFUSE in standard location
    ifneq ($(wildcard /usr/local/include/fuse/fuse.h),)
        FUSE_INCLUDE_PATH = /usr/local/include/fuse
        FUSE_LIB_PATH = /usr/local/lib
    # Check for macFUSE in legacy osxfuse location (for compatibility)
    else ifneq ($(wildcard /usr/local/include/osxfuse/fuse/fuse.h),)
        FUSE_INCLUDE_PATH = /usr/local/include/osxfuse/fuse
        FUSE_LIB_PATH = /usr/local/lib
    # Check for Homebrew on Apple Silicon
    else ifneq ($(wildcard /opt/homebrew/include/fuse/fuse.h),)
        FUSE_INCLUDE_PATH = /opt/homebrew/include/fuse
        FUSE_LIB_PATH = /opt/homebrew/lib
    # Check for MacPorts
    else ifneq ($(wildcard /opt/local/include/fuse/fuse.h),)
        FUSE_INCLUDE_PATH = /opt/local/include/fuse
        FUSE_LIB_PATH = /opt/local/lib
    else
        # Default to standard macFUSE location
        FUSE_INCLUDE_PATH = /usr/local/include/fuse
        FUSE_LIB_PATH = /usr/local/lib
    endif
    
    FUSE_CFLAGS = -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26
    FUSE_LDFLAGS = -L$(FUSE_LIB_PATH) -lfuse
    FUSE_INCLUDES = -I$(FUSE_INCLUDE_PATH)
endif

# XFS and utility includes
XFSUTILS_INCLUDES = -I$(SRC)/xfsutil
XFS_INCLUDES = -I$(SRC)/xfsprogs/include

# Common compiler flags
COMMON_CFLAGS = $(ARCH_FLAGS) $(MACOS_FLAGS) -Wall
COMMON_LDFLAGS = $(ARCH_FLAGS) $(MACOS_FLAGS)

# Programs to build
PROGRAMS = xfs-cli xfs-rcopy fuse-xfs mkfs.xfs
PROGRAMS := $(addprefix $(BINS)/, $(PROGRAMS))

# DMG output
DMG = $(BUILD)/fuse-xfs-$(VERSION).dmg

# Debug build support
ifdef DEBUG
    COMMON_CFLAGS += -g -DDEBUG
else
    COMMON_CFLAGS += -O2
endif
# fuse-xfs Main Makefile
# Modernized for macFUSE and Apple Silicon support

TOPLEVEL=..

include Make.inc

# Default target - build all programs (skip DMG for basic build)
all: $(PROGRAMS)

# Full build including DMG
full: $(PROGRAMS) $(DMG)

$(LIBS):
	mkdir -p $(LIBS)

$(BINS):
	mkdir -p $(BINS)

$(OBJECTS):
	mkdir -p $(OBJECTS)

$(LIBS)/libxfs.a: $(LIBS) $(BINS)
	$(MAKE) -C xfsprogs
	for LIB in libdisk libxcmd libxlog libxfs; do\
		echo $$LIB;\
		ln -sf $(PWD)/xfsprogs/$$LIB/.libs/$$LIB.0.dylib $(LIBS)/;\
		ln -sf $(PWD)/xfsprogs/$$LIB/.libs/$$LIB.a $(LIBS)/;\
		ln -sf $(PWD)/xfsprogs/$$LIB/.libs/$$LIB.la $(LIBS)/;\
		ln -sf $(PWD)/xfsprogs/$$LIB/.libs/$$LIB.dylib $(LIBS)/;\
	done

$(BINS)/mkfs.xfs: $(LIBS)/libxfs.a $(BINS)
	cp $(PWD)/xfsprogs/mkfs/mkfs.xfs $(BINS)/

$(OBJECTS)/xfsutil.o: $(OBJECTS)
	$(MAKE) -C xfsutil

$(BINS)/xfs-cli: $(LIBS)/libxfs.a $(OBJECTS)/xfsutil.o $(BINS)
	$(MAKE) -C cli xfs-cli

$(BINS)/xfs-rcopy: $(LIBS)/libxfs.a $(OBJECTS)/xfsutil.o $(BINS)
	$(MAKE) -C cli xfs-rcopy

$(BINS)/fuse-xfs: $(LIBS)/libxfs.a $(OBJECTS)/xfsutil.o $(BINS)
	$(MAKE) -C fuse

$(BUILD)/fuse-xfs-$(VERSION).dmg: $(PROGRAMS)
	$(MAKE) -C macosx

clean:
	rm -rf $(BINS)/*
	rm -rf $(LIBS)/*
	rm -rf $(OBJECTS)/*
	-$(MAKE) -C cli clean
	-$(MAKE) -C fuse clean
	-$(MAKE) -C xfsutil clean
	-$(MAKE) -C xfsprogs clean
	-$(MAKE) -C macosx clean

# Show build configuration
config:
	@echo "=== fuse-xfs Build Configuration ==="
	@echo "Architecture: $(ARCH)"
	@echo "Arch Flags: $(ARCH_FLAGS)"
	@echo "macOS Min Version: $(MACOS_MIN_VERSION)"
	@echo "FUSE pkg-config: $(FUSE_PKG_CONFIG)"
	@echo "FUSE CFLAGS: $(FUSE_CFLAGS)"
	@echo "FUSE LDFLAGS: $(FUSE_LDFLAGS)"
	@echo "FUSE INCLUDES: $(FUSE_INCLUDES)"
	@echo "==================================="

# Build macOS installer package
pkg: $(PROGRAMS)
	@echo "Building macOS installer package..."
	@$(TOPLEVEL)/scripts/build-pkg.sh

# Install to /usr/local (requires sudo)
install: $(PROGRAMS)
	@echo "Installing fuse-xfs..."
	install -d /usr/local/bin
	install -m 755 $(BINS)/fuse-xfs /usr/local/bin/
	install -m 755 $(BINS)/xfs-cli /usr/local/bin/
	install -m 755 $(BINS)/xfs-rcopy /usr/local/bin/
	@if [ -f $(BINS)/mkfs.xfs ]; then \
		install -m 755 $(BINS)/mkfs.xfs /usr/local/bin/; \
	fi
	@if [ -d $(TOPLEVEL)/man ]; then \
		install -d /usr/local/share/man/man1; \
		install -d /usr/local/share/man/man8; \
		install -m 644 $(TOPLEVEL)/man/*.1 /usr/local/share/man/man1/ 2>/dev/null || true; \
		install -m 644 $(TOPLEVEL)/man/*.8 /usr/local/share/man/man8/ 2>/dev/null || true; \
	fi
	@echo "Installation complete!"

# Uninstall from /usr/local (requires sudo)
uninstall:
	@echo "Uninstalling fuse-xfs..."
	rm -f /usr/local/bin/fuse-xfs
	rm -f /usr/local/bin/xfs-cli
	rm -f /usr/local/bin/xfs-rcopy
	rm -f /usr/local/bin/mkfs.xfs
	rm -f /usr/local/share/man/man1/fuse-xfs.1
	rm -f /usr/local/share/man/man8/mkfs.xfs.8
	@echo "Uninstallation complete!"

.PHONY: all full clean config pkg install uninstall

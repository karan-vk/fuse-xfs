# fuse-xfs for macOS

> **Modernized fork** - This project is a modernized fork of the original [fuse-xfs](https://code.google.com/archive/p/fuse-xfs/) project (formerly hosted on Google Code). Updated for Apple Silicon, modern macOS, and XFS V5 filesystem support.

**Original Authors:** Various contributors (see original project)
**Modernization by:** [@karan-vk](https://github.com/karan-vk)

**Full read-write XFS filesystem support for macOS via FUSE.**

## Features

- **Full Read-Write Support** - Create, modify, and delete files and directories on XFS filesystems
- **Apple Silicon (ARM64) and Intel (x86_64) support** - Native builds for M1/M2/M3 Macs and Intel Macs
- **Modern XFS V5 superblock format** with CRC checksums
- **FTYPE directory entries** - Proper file type information in directory listings
- **macFUSE compatibility** - Works with macFUSE 4.x
- **Support for filesystems created by modern Linux kernels** (3.10+)
- **Transaction-Safe Operations** - All write operations are transaction-protected

## Requirements

- **macOS 11.0 (Big Sur) or later** for Apple Silicon
- **macOS 10.15 (Catalina) or later** for Intel Macs
- **[macFUSE](https://macfuse.github.io/) 4.0 or later**
- **Xcode Command Line Tools**

### Installing macFUSE

Download and install macFUSE from: https://macfuse.github.io/

Or via Homebrew (if available):
```bash
brew install --cask macfuse
```

**Note:** After installing macFUSE, you may need to allow the kernel extension in System Preferences > Security & Privacy.

### Installing Xcode Command Line Tools

```bash
xcode-select --install
```

## Installation

### Via Homebrew (Recommended)

The easiest way to install fuse-xfs is via Homebrew:

```bash
# Add the tap
brew tap karan-vk/fuse-xfs

# Install fuse-xfs
brew install fuse-xfs
```

**Note:** You must install macFUSE separately:
```bash
brew install --cask macfuse
```

### Via macOS Installer Package

Download the latest `.pkg` file from the [Releases](https://github.com/karan-vk/fuse-xfs/releases) page and run the installer.

Or build the package yourself:
```bash
cd src
make
make pkg
```

The package will be created at `build/pkg/fuse-xfs-0.2.1.pkg`.

To install via command line:
```bash
sudo installer -pkg build/pkg/fuse-xfs-0.2.1.pkg -target /
```

### Via Make Install

After building, you can install directly to `/usr/local`:

```bash
cd src
make
sudo make install
```

To uninstall:
```bash
cd src
sudo make uninstall
```

### From Source (Development)

For development or if you prefer not to install system-wide:

```bash
# Install macFUSE first
brew install --cask macfuse

# Build from source
cd src
make clean
make

# Binaries are in build/bin/
./build/bin/fuse-xfs --help
```

## Building

### Quick Build

```bash
cd src
make
```

### Build with Debug Symbols

```bash
cd src
make DEBUG=1
```

### Clean and Rebuild

```bash
cd src
make clean
make
```

### View Build Configuration

```bash
cd src
make config
```

This will show the detected architecture, FUSE paths, and compiler flags.

### Build Output

After a successful build, binaries are located in `build/bin/`:

| Binary | Description |
|--------|-------------|
| `fuse-xfs` | FUSE filesystem driver for mounting XFS |
| `xfs-cli` | Command-line interface for XFS operations |
| `xfs-rcopy` | Recursive copy utility for XFS filesystems |
| `mkfs.xfs` | XFS filesystem creation tool |

## Usage

### Mounting an XFS Filesystem

```bash
# Mount an XFS disk image (read-only by default)
./build/bin/fuse-xfs /path/to/xfs.img /mnt/xfs

# Mount an XFS disk image with read-write support
./build/bin/fuse-xfs -rw /path/to/xfs.img /mnt/xfs

# Mount an XFS device (requires appropriate permissions)
sudo ./build/bin/fuse-xfs /dev/disk2s1 /mnt/xfs

# Mount an XFS device with read-write support
sudo ./build/bin/fuse-xfs -rw /dev/disk2s1 /mnt/xfs

# Mount with debug output
./build/bin/fuse-xfs -d /path/to/xfs.img /mnt/xfs

# Mount with debug output and read-write
./build/bin/fuse-xfs -rw -d /path/to/xfs.img /mnt/xfs
```

### Unmounting

```bash
# Standard unmount
umount /mnt/xfs

# Force unmount if busy
diskutil unmount force /mnt/xfs
```

### Using xfs-cli

The command-line interface allows browsing XFS filesystems without mounting:

```bash
./build/bin/xfs-cli /path/to/xfs.img
```

Available commands:
- `ls [path]` - List directory contents
- `cd <path>` - Change directory
- `cat <file>` - Display file contents
- `pwd` - Print working directory
- `exit` - Exit the CLI

### Using xfs-rcopy

Copy files from an XFS filesystem:

```bash
./build/bin/xfs-rcopy /path/to/xfs.img /source/path /destination/path
```

## Supported XFS Features

### Fully Supported

| Feature | Description |
|---------|-------------|
| V4 Superblock | Legacy XFS format |
| V5 Superblock | Modern XFS format with metadata CRC |
| FTYPE | File type in directory entries |
| Dir2 Format | Version 2 directory format |
| Shortform Directories | Small directories stored in inode |
| Block Directories | Single-block directories |
| Leaf Directories | Multi-block directories with leaf structure |
| Regular Files | Full read/write support |
| Symbolic Links | Full read/write support |
| Hard Links | Full read/write support |
| Directories | Full create/remove/rename support |

### Write Operations Support

| Operation | Status | Description |
|-----------|--------|-------------|
| `create` | ✅ Supported | Create new files |
| `write` | ✅ Supported | Write data to files |
| `truncate` | ✅ Supported | Change file size |
| `unlink` | ✅ Supported | Remove files |
| `mkdir` | ✅ Supported | Create directories |
| `rmdir` | ✅ Supported | Remove empty directories |
| `rename` | ✅ Supported | Rename files and directories |
| `chmod` | ✅ Supported | Change file permissions |
| `chown` | ✅ Supported | Change file ownership |
| `utimens` | ✅ Supported | Update timestamps |
| `mknod` | ✅ Supported | Create device nodes, FIFOs |
| `symlink` | ✅ Supported | Create symbolic links |
| `link` | ✅ Supported | Create hard links |
| `fsync` | ✅ Supported | Synchronize file data |

### XFS Versions

| Version | Support Status |
|---------|---------------|
| V1-V3 | Basic support |
| V4 | Full support |
| V5 (CRC) | Full support |

### Partially Supported

| Feature | Status |
|---------|--------|
| Large Files (>2GB) | Supported |
| Sparse Files | Supported |
| Extended Attributes | Read-only |

### Not Supported

| Feature | Reason |
|---------|--------|
| Real-time Devices | Not implemented |
| External Logs | Not implemented |
| Reflinks | Not implemented |
| Quotas | Not implemented |
| ACLs | Not implemented |
| Extended Attributes (write) | Not implemented |

## Limitations

1. **No External Log Support** - XFS filesystems with external log devices cannot be mounted.

2. **No Real-time Section** - XFS filesystems with real-time sections cannot be mounted.

3. **macFUSE Required** - The fuse-xfs binary requires macFUSE to be installed.

4. **Root Privileges** - Mounting physical devices typically requires root privileges.

5. **Extended Attributes** - Write operations for extended attributes are not supported.

6. **Quotas** - XFS quota functionality is not implemented.

7. **ACLs** - Access Control Lists are not supported.

For detailed information about write support, see [WRITE_SUPPORT.md](WRITE_SUPPORT.md).

## Troubleshooting

### "fuse.h not found" during build

macFUSE is not installed or not detected. Install macFUSE and ensure it's properly configured:

```bash
# Check if macFUSE is installed
ls -la /usr/local/include/fuse/

# If using Homebrew on Apple Silicon, check:
ls -la /opt/homebrew/include/fuse/
```

### "Operation not permitted" when mounting

1. Ensure macFUSE kernel extension is allowed in System Preferences
2. On macOS 11+, you may need to enable "Allow user management of kernel extensions" in Recovery Mode
3. Try running with `sudo`

### "Filesystem has an external log"

This XFS filesystem was created with an external log device, which is not supported. The filesystem must have an internal log.

### "Filesystem has a real-time section"

This XFS filesystem was created with a real-time section, which is not supported.

### Mount hangs or crashes

1. Try mounting with debug output: `fuse-xfs -d /path/to/image /mount/point`
2. Check system logs: `log show --predicate 'subsystem == "com.apple.filesystems"' --last 5m`
3. Ensure the XFS image is not corrupted

### Permission denied reading files

The FUSE filesystem preserves original Unix permissions. If you can't read files:

1. Check file permissions with `ls -la`
2. Mount as root if needed
3. The original file owner/group may not exist on macOS

## Project Structure

```
fuse-xfs/
├── README.md           # This file
├── MODERNIZATION_PLAN.md  # Development roadmap
├── COPYING             # License (GPL)
├── build/              # Build output
│   ├── bin/           # Compiled binaries
│   ├── lib/           # Libraries
│   └── obj/           # Object files
├── man/               # Manual pages
└── src/
    ├── Make.inc       # Build configuration
    ├── Makefile       # Main makefile
    ├── cli/           # Command-line tools
    ├── fuse/          # FUSE integration
    │   ├── fuse_xfs.c # FUSE operations
    │   └── main.c     # Entry point
    ├── xfsprogs/      # XFS utilities library
    └── xfsutil/       # XFS helper functions
```

## Development

### Adding Support for New Features

The main files to modify for XFS feature support:

- `src/xfsprogs/include/xfs_sb.h` - Superblock version detection
- `src/xfsprogs/include/xfs_dir2_sf.h` - Directory structures
- `src/xfsutil/xfsutil.c` - Directory reading, file I/O
- `src/fuse/fuse_xfs.c` - FUSE operations

### Building for Testing

```bash
cd src
make DEBUG=1
```

### Creating Test Filesystems

On a Linux system:

```bash
# Create a V4 (legacy) filesystem
dd if=/dev/zero of=test_v4.img bs=1M count=100
mkfs.xfs -m crc=0 test_v4.img

# Create a V5 (modern) filesystem
dd if=/dev/zero of=test_v5.img bs=1M count=100
mkfs.xfs test_v5.img

# Create a V5 filesystem with specific features
mkfs.xfs -m crc=1,finobt=1 test_v5_finobt.img
```

## Version History

- **0.2.1** - Current version with Apple Silicon and V5 XFS support
- **0.2.0** - macFUSE migration, ARM64 support
- **0.1.x** - Original osxfuse implementation

## License

This project is licensed under the GNU General Public License (GPL). See the `COPYING` file for details.

## Credits & History

This project is based on the original **fuse-xfs** project:
- **Original Project:** [fuse-xfs on Google Code](https://code.google.com/archive/p/fuse-xfs/) (archived)
- **Original xfsprogs:** Based on xfsprogs 3.1.4 from SGI/Linux XFS team

### Modernization (2024)
Modernized by [@karan-vk](https://github.com/karan-vk):
- Apple Silicon (ARM64) support
- macFUSE compatibility (replacing deprecated osxfuse)
- XFS V5 superblock format with CRC checksums
- Modern XFS feature flags (FTYPE, sparse inodes, etc.)
- Homebrew formula and macOS package installer

### Original Credits
- Original author: Alexandre Hardy (2011)
- XFS utilities based on xfsprogs from SGI/Linux
- FUSE support via [macFUSE](https://macfuse.github.io/)

## Documentation

- [WRITE_SUPPORT.md](WRITE_SUPPORT.md) - Detailed write operation documentation
- [API.md](API.md) - XFS utility API reference
- [WRITE_OPERATIONS_DESIGN.md](WRITE_OPERATIONS_DESIGN.md) - Design documentation
- [CHANGELOG.md](CHANGELOG.md) - Version history and changes

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on how to contribute to this project.

## See Also

- [macFUSE](https://macfuse.github.io/) - Filesystem in Userspace for macOS
- [XFS Documentation](https://xfs.org/) - Official XFS documentation
- [xfsprogs](https://git.kernel.org/pub/scm/fs/xfs/xfsprogs-dev.git/) - XFS utilities for Linux
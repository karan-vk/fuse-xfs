#!/bin/bash
# fuse-xfs macOS Package Builder
# Creates a .pkg installer for distribution

set -e

# Configuration
VERSION="1.0.0"
PKG_NAME="fuse-xfs"
PKG_IDENTIFIER="io.github.karan-vk.fuse-xfs"

# Directory setup
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SRC_DIR="$PROJECT_ROOT/src"
BUILD_DIR="$PROJECT_ROOT/build"
PKG_BUILD_DIR="$BUILD_DIR/pkg"
STAGE_DIR="$PKG_BUILD_DIR/stage"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if binaries exist
check_binaries() {
    local binaries=("fuse-xfs" "xfs-cli" "xfs-rcopy")
    local missing=0
    
    for binary in "${binaries[@]}"; do
        if [ ! -f "$BUILD_DIR/bin/$binary" ]; then
            log_error "Binary not found: $BUILD_DIR/bin/$binary"
            missing=1
        fi
    done
    
    if [ $missing -eq 1 ]; then
        log_error "Please build the project first with 'make' in the src directory"
        exit 1
    fi
}

# Clean up previous build
clean_pkg_dir() {
    log_info "Cleaning previous package build..."
    rm -rf "$PKG_BUILD_DIR"
}

# Create staging directory structure
create_staging() {
    log_info "Creating staging directory structure..."
    
    mkdir -p "$STAGE_DIR/usr/local/bin"
    mkdir -p "$STAGE_DIR/usr/local/share/man/man1"
    mkdir -p "$STAGE_DIR/usr/local/share/man/man8"
}

# Copy binaries to staging
copy_binaries() {
    log_info "Copying binaries..."
    
    cp "$BUILD_DIR/bin/fuse-xfs" "$STAGE_DIR/usr/local/bin/"
    cp "$BUILD_DIR/bin/xfs-cli" "$STAGE_DIR/usr/local/bin/"
    cp "$BUILD_DIR/bin/xfs-rcopy" "$STAGE_DIR/usr/local/bin/"
    
    # Also copy mkfs.xfs if it exists
    if [ -f "$BUILD_DIR/bin/mkfs.xfs" ]; then
        cp "$BUILD_DIR/bin/mkfs.xfs" "$STAGE_DIR/usr/local/bin/"
        log_info "Included mkfs.xfs"
    fi
    
    # Set proper permissions
    chmod 755 "$STAGE_DIR/usr/local/bin/"*
}

# Copy man pages to staging
copy_man_pages() {
    log_info "Copying man pages..."
    
    if [ -d "$PROJECT_ROOT/man" ]; then
        # Copy section 1 man pages
        if ls "$PROJECT_ROOT/man"/*.1 1>/dev/null 2>&1; then
            cp "$PROJECT_ROOT/man"/*.1 "$STAGE_DIR/usr/local/share/man/man1/"
            log_info "Copied man1 pages"
        fi
        
        # Copy section 8 man pages
        if ls "$PROJECT_ROOT/man"/*.8 1>/dev/null 2>&1; then
            cp "$PROJECT_ROOT/man"/*.8 "$STAGE_DIR/usr/local/share/man/man8/"
            log_info "Copied man8 pages"
        fi
    else
        log_warn "No man directory found, skipping man pages"
    fi
}

# Build component package
build_component_pkg() {
    log_info "Building component package..."
    
    pkgbuild \
        --root "$STAGE_DIR" \
        --identifier "$PKG_IDENTIFIER" \
        --version "$VERSION" \
        --install-location "/" \
        "$PKG_BUILD_DIR/$PKG_NAME-$VERSION-component.pkg"
}

# Create distribution XML for productbuild
create_distribution_xml() {
    log_info "Creating distribution.xml..."
    
    cat > "$PKG_BUILD_DIR/distribution.xml" << EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>fuse-xfs $VERSION</title>
    <organization>$PKG_IDENTIFIER</organization>
    <domains enable_localSystem="true"/>
    <options customize="never" require-scripts="true" rootVolumeOnly="true"/>
    
    <welcome file="welcome.html" mime-type="text/html"/>
    <license file="license.txt" mime-type="text/plain"/>
    <readme file="readme.html" mime-type="text/html"/>
    
    <choices-outline>
        <line choice="default">
            <line choice="$PKG_IDENTIFIER"/>
        </line>
    </choices-outline>
    
    <choice id="default"/>
    <choice id="$PKG_IDENTIFIER" visible="false">
        <pkg-ref id="$PKG_IDENTIFIER"/>
    </choice>
    
    <pkg-ref id="$PKG_IDENTIFIER" version="$VERSION" onConclusion="none">$PKG_NAME-$VERSION-component.pkg</pkg-ref>
</installer-gui-script>
EOF
}

# Create installer resources
create_resources() {
    log_info "Creating installer resources..."
    
    mkdir -p "$PKG_BUILD_DIR/resources"
    
    # Welcome HTML
    cat > "$PKG_BUILD_DIR/resources/welcome.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Welcome</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; padding: 20px; }
        h1 { color: #333; }
        .requirement { background: #f5f5f5; padding: 10px; border-radius: 5px; margin: 10px 0; }
        .warning { color: #c00; }
    </style>
</head>
<body>
    <h1>fuse-xfs for macOS</h1>
    <p>This package installs fuse-xfs, providing read-only XFS filesystem support for macOS.</p>
    
    <div class="requirement">
        <strong>Requirement:</strong> macFUSE must be installed before using fuse-xfs.
        <br><br>
        If you haven't installed macFUSE yet, please download it from:
        <br>
        <a href="https://macfuse.github.io/">https://macfuse.github.io/</a>
    </div>
    
    <h2>What will be installed:</h2>
    <ul>
        <li><code>/usr/local/bin/fuse-xfs</code> - FUSE filesystem driver</li>
        <li><code>/usr/local/bin/xfs-cli</code> - Command-line interface</li>
        <li><code>/usr/local/bin/xfs-rcopy</code> - Recursive copy utility</li>
        <li>Man pages in <code>/usr/local/share/man/</code></li>
    </ul>
    
    <p class="warning">Note: After installing macFUSE, you may need to allow the system extension in System Preferences → Security & Privacy.</p>
</body>
</html>
EOF
    
    # Readme HTML
    cat > "$PKG_BUILD_DIR/resources/readme.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Read Me</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; padding: 20px; }
        code { background: #f5f5f5; padding: 2px 6px; border-radius: 3px; }
        pre { background: #f5f5f5; padding: 10px; border-radius: 5px; overflow-x: auto; }
    </style>
</head>
<body>
    <h1>fuse-xfs Usage</h1>
    
    <h2>Mounting an XFS Filesystem</h2>
    <pre>
# Mount an XFS disk image
fuse-xfs /path/to/xfs.img /mount/point

# Mount an XFS device (requires sudo)
sudo fuse-xfs /dev/disk2s1 /mount/point

# Mount with debug output
fuse-xfs -d /path/to/xfs.img /mount/point
    </pre>
    
    <h2>Unmounting</h2>
    <pre>
umount /mount/point
# or
diskutil unmount /mount/point
    </pre>
    
    <h2>Using xfs-cli</h2>
    <pre>
xfs-cli /path/to/xfs.img

# Available commands: ls, cd, cat, pwd, exit
    </pre>
    
    <h2>More Information</h2>
    <p>See <code>man fuse-xfs</code> for detailed documentation.</p>
    <p>Project homepage: <a href="https://github.com/karan-vk/fuse-xfs">https://github.com/karan-vk/fuse-xfs</a></p>
</body>
</html>
EOF
    
    # License text (copy from COPYING if it exists)
    if [ -f "$PROJECT_ROOT/COPYING" ]; then
        cp "$PROJECT_ROOT/COPYING" "$PKG_BUILD_DIR/resources/license.txt"
    else
        cat > "$PKG_BUILD_DIR/resources/license.txt" << 'EOF'
GNU GENERAL PUBLIC LICENSE
Version 2, June 1991

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
EOF
    fi
}

# Build final product package
build_product_pkg() {
    log_info "Building final product package..."
    
    productbuild \
        --distribution "$PKG_BUILD_DIR/distribution.xml" \
        --resources "$PKG_BUILD_DIR/resources" \
        --package-path "$PKG_BUILD_DIR" \
        --identifier "$PKG_IDENTIFIER" \
        --version "$VERSION" \
        "$PKG_BUILD_DIR/$PKG_NAME-$VERSION.pkg"
}

# Build a simple package without distribution (fallback)
build_simple_pkg() {
    log_info "Building simple product package..."
    
    productbuild \
        --package "$PKG_BUILD_DIR/$PKG_NAME-$VERSION-component.pkg" \
        --identifier "$PKG_IDENTIFIER" \
        --version "$VERSION" \
        "$PKG_BUILD_DIR/$PKG_NAME-$VERSION.pkg"
}

# Print summary
print_summary() {
    echo ""
    echo "=========================================="
    echo -e "${GREEN}Package build complete!${NC}"
    echo "=========================================="
    echo ""
    echo "Package location:"
    echo "  $PKG_BUILD_DIR/$PKG_NAME-$VERSION.pkg"
    echo ""
    echo "Package contents:"
    echo "  - /usr/local/bin/fuse-xfs"
    echo "  - /usr/local/bin/xfs-cli"
    echo "  - /usr/local/bin/xfs-rcopy"
    if [ -f "$BUILD_DIR/bin/mkfs.xfs" ]; then
        echo "  - /usr/local/bin/mkfs.xfs"
    fi
    echo "  - Man pages"
    echo ""
    echo "To install:"
    echo "  sudo installer -pkg $PKG_BUILD_DIR/$PKG_NAME-$VERSION.pkg -target /"
    echo ""
    echo "Or double-click the .pkg file to use the graphical installer."
    echo ""
}

# Main function
main() {
    echo ""
    echo "=========================================="
    echo "fuse-xfs Package Builder"
    echo "Version: $VERSION"
    echo "=========================================="
    echo ""
    
    # Check prerequisites
    check_binaries
    
    # Build package
    clean_pkg_dir
    create_staging
    copy_binaries
    copy_man_pages
    build_component_pkg
    create_resources
    create_distribution_xml
    
    # Try to build with distribution, fall back to simple if it fails
    if ! build_product_pkg 2>/dev/null; then
        log_warn "Full distribution build failed, using simple package"
        build_simple_pkg
    fi
    
    # Clean up component package
    rm -f "$PKG_BUILD_DIR/$PKG_NAME-$VERSION-component.pkg"
    
    print_summary
}

# Run main function
main "$@"
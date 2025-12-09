# Homebrew Formula for fuse-xfs
# Read-only XFS filesystem support for macOS via FUSE

class FuseXfs < Formula
  desc "Read-only XFS filesystem support for macOS via FUSE"
  homepage "https://github.com/karan-vk/fuse-xfs"
  url "https://github.com/karan-vk/fuse-xfs/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "PLACEHOLDER_SHA256_REPLACE_WITH_ACTUAL_HASH"
  license "GPL-2.0"
  head "https://github.com/karan-vk/fuse-xfs.git", branch: "main"

  # Build requirements
  depends_on :macos
  depends_on :xcode => :build

  # Runtime dependency - macFUSE (installed as cask)
  # Note: Users need to install macfuse separately:
  #   brew install --cask macfuse
  
  # Optional: Uncomment if macfuse is available as a formula dependency
  # depends_on "macfuse" => :build

  def install
    # Build the project
    cd "src" do
      system "make", "clean"
      system "make"
      
      # Install binaries
      bin.install "#{buildpath}/build/bin/fuse-xfs"
      bin.install "#{buildpath}/build/bin/xfs-cli"
      bin.install "#{buildpath}/build/bin/xfs-rcopy"
      
      # Install mkfs.xfs if built
      if File.exist?("#{buildpath}/build/bin/mkfs.xfs")
        bin.install "#{buildpath}/build/bin/mkfs.xfs"
      end
    end
    
    # Install man pages if present
    if Dir.exist?("man")
      man1.install Dir["man/*.1"]
      man8.install Dir["man/*.8"]
    end
    
    # Install documentation
    doc.install "README.md" if File.exist?("README.md")
    doc.install "COPYING" if File.exist?("COPYING")
  end

  def caveats
    <<~EOS
      fuse-xfs requires macFUSE to be installed.

      To install macFUSE:
        brew install --cask macfuse

      After installing macFUSE, you may need to:
        1. Allow the system extension in:
           System Preferences → Security & Privacy → General
        2. Restart your computer

      Usage:
        # Mount an XFS filesystem
        fuse-xfs /path/to/xfs.img /mount/point

        # Mount a device (requires sudo)
        sudo fuse-xfs /dev/disk2s1 /mount/point

        # Use the CLI tool
        xfs-cli /path/to/xfs.img

      For more information:
        man fuse-xfs
    EOS
  end

  test do
    # Verify binaries are installed and can show help
    assert_match "usage", shell_output("#{bin}/xfs-cli --help 2>&1", 1)
    
    # Check fuse-xfs binary exists
    assert_predicate bin/"fuse-xfs", :exist?
    assert_predicate bin/"xfs-rcopy", :exist?
  end
end
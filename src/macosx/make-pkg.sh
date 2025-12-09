#
# This script is heavly based on ntfs-3g mac osx package,
# thanks to Erik Larsson, and Paul Marks for their efforts.
#

#!/bin/sh

set -e

SUDO="sudo"
SED_E="${SUDO} sed -e"
MV="${SUDO} mv"
CP_R="${SUDO} cp -R"
RM_RF="${SUDO} rm -rf"
MKDIR_P="${SUDO} mkdir -p"
LN_SF="${SUDO} ln -sf"
INSTALL_C="${SUDO} install -c"
CHOWN_R="${SUDO} chown -R"
PKGBUILD="${SUDO} pkgbuild"

FUSEXFS_NAME="fuse-xfs"

FUSEXFS_VERSION="$1"
SOURCE_FOLDER="$2"
BUILD_FOLDER="$3"
MKPKG_FOLDER="$4"

TMP_FOLDER="$BUILD_FOLDER/tmp"
DISTRIBUTION_FOLDER="${TMP_FOLDER}/Distribution_folder/"

if [ x"$FUSEXFS_VERSION" = x"" ]
then
  echo "Usage: make-pkg.sh <version> <srcdir> <builddir> <mkpkgdir>"
  exit 1
fi

if [ x"$SOURCE_FOLDER" = x"" ]
then
  echo "Usage: make-pkg.sh <version> <srcdir> <builddir> <mkpkgdir>"
  exit 1
fi

if [ x"$BUILD_FOLDER" = x"" ]
then
  echo "Usage: make-pkg.sh <version> <srcdir> <builddir> <mkpkgdir>"
  exit 1
fi

if [ x"$MKPKG_FOLDER" = x"" ]
then
  echo "Usage: make-pkg.sh <version> <srcdir> <builddir> <mkpkgdir>"
  exit 1
fi

${RM_RF} ${TMP_FOLDER} *.pkg *.dmg
${MKDIR_P} ${TMP_FOLDER}
${MKDIR_P} ${DISTRIBUTION_FOLDER}
${MKDIR_P} ${DISTRIBUTION_FOLDER}/usr
${MKDIR_P} ${DISTRIBUTION_FOLDER}/usr/local
${MKDIR_P} ${DISTRIBUTION_FOLDER}/usr/local/bin
${MKDIR_P} ${DISTRIBUTION_FOLDER}/usr/local/sbin
${MKDIR_P} ${DISTRIBUTION_FOLDER}/usr/local/lib
${MKDIR_P} ${DISTRIBUTION_FOLDER}/usr/local/lib/pkgconfig
${MKDIR_P} ${DISTRIBUTION_FOLDER}/usr/local/share/man/man1/
${MKDIR_P} ${DISTRIBUTION_FOLDER}/usr/local/share/man/man8/
${MKDIR_P} ${DISTRIBUTION_FOLDER}/Library/PreferencePanes
${MKDIR_P} ${DISTRIBUTION_FOLDER}/Library/Filesystems/fuse-xfs.fs
${MKDIR_P} ${DISTRIBUTION_FOLDER}/Library/Filesystems/fuse-xfs.fs/Support
${MKDIR_P} ${DISTRIBUTION_FOLDER}/Library/Filesystems/fuse-xfs.fs/Contents
${MKDIR_P} ${DISTRIBUTION_FOLDER}/Library/Filesystems/fuse-xfs.fs/Contents/Resources
${MKDIR_P} ${DISTRIBUTION_FOLDER}/Library/Filesystems/fuse-xfs.fs/Contents/Resources/English.lproj
${INSTALL_C} -m 755 ${BUILD_FOLDER}/bin/mkfs.xfs ${DISTRIBUTION_FOLDER}/usr/local/bin/mkfs.xfs
${INSTALL_C} -m 755 ${BUILD_FOLDER}/bin/fuse-xfs ${DISTRIBUTION_FOLDER}/usr/local/bin/fuse-xfs
${INSTALL_C} -m 755 ${BUILD_FOLDER}/bin/xfs-cli ${DISTRIBUTION_FOLDER}/usr/local/bin/xfs-cli
${INSTALL_C} -m 755 ${BUILD_FOLDER}/bin/xfs-rcopy ${DISTRIBUTION_FOLDER}/usr/local/bin/xfs-rcopy
#${INSTALL_C} -m 755 ${BUILD_FOLDER}/fuse-xfs/fuse-xfs.wait ${DISTRIBUTION_FOLDER}/usr/local/bin/fuse-xfs.wait
#${INSTALL_C} -m 755 ${BUILD_FOLDER}/fuse-xfs/fuse-xfs.probe ${DISTRIBUTION_FOLDER}/usr/local/bin/fuse-xfs.probe
#${INSTALL_C} -m 755 ${BUILD_FOLDER}/fuse-xfs/fuse-xfs.install ${DISTRIBUTION_FOLDER}/usr/local/bin/fuse-xfs.install
#${INSTALL_C} -m 755 ${BUILD_FOLDER}/fuse-xfs/fuse-xfs.uninstall ${DISTRIBUTION_FOLDER}/usr/local/bin/fuse-xfs.uninstall
${INSTALL_C} -m 644 ${SOURCE_FOLDER}/man/fuse-xfs.1 ${DISTRIBUTION_FOLDER}/usr/local/share/man/man1/fuse-xfs.1
${INSTALL_C} -m 644 ${SOURCE_FOLDER}/man/mkfs.xfs.8 ${DISTRIBUTION_FOLDER}/usr/local/share/man/man8/mkfs.xfs.8
${INSTALL_C} -m 755 ${MKPKG_FOLDER}/fuse-xfs.fs/fuse-xfs.util ${DISTRIBUTION_FOLDER}/Library/Filesystems/fuse-xfs.fs/fuse-xfs.util
${INSTALL_C} -m 755 ${MKPKG_FOLDER}/fuse-xfs.fs/mount_fuse-xfs ${DISTRIBUTION_FOLDER}/Library/Filesystems/fuse-xfs.fs/mount_fuse-xfs
${SED_E} "s/FUSEXFS_VERSION_LITERAL/$FUSEXFS_VERSION/g" < ${MKPKG_FOLDER}/fuse-xfs.fs/Contents/Info.plist.in > ${BUILD_FOLDER}/FSInfo.plist
${INSTALL_C} -m 644 ${BUILD_FOLDER}/FSInfo.plist ${DISTRIBUTION_FOLDER}/Library/Filesystems/fuse-xfs.fs/Contents/Info.plist
${INSTALL_C} -m 644 ${MKPKG_FOLDER}/fuse-xfs.fs/Contents/PkgInfo ${DISTRIBUTION_FOLDER}/Library/Filesystems/fuse-xfs.fs/Contents/PkgInfo
${INSTALL_C} -m 644 ${MKPKG_FOLDER}/fuse-xfs.fs/Contents/Resources/English.lproj/InfoPlist.strings ${DISTRIBUTION_FOLDER}/Library/Filesystems/fuse-xfs.fs/Contents/Resources/English.lproj/InfoPlist.strings
${LN_SF} /Library/Filesystems/fuse-xfs.fs/mount_fuse-xfs ${DISTRIBUTION_FOLDER}/usr/local/sbin/mount_fuse-xfs
${LN_SF} /Library/Filesystems/fuse-xfs.fs/mount_fuse-xfs ${DISTRIBUTION_FOLDER}/Library/Filesystems/fuse-xfs.fs/Contents/Resources
${SED_E} "s/FUSEXFS_VERSION_LITERAL/$FUSEXFS_VERSION/g" < ${MKPKG_FOLDER}/Info.plist.in > ${BUILD_FOLDER}/Info.plist
${INSTALL_C} -m 644 ${BUILD_FOLDER}/Info.plist ${TMP_FOLDER}/Info.plist
${CHOWN_R} root:wheel ${TMP_FOLDER}
${SUDO} find ${DISTRIBUTION_FOLDER} -name ".hg" -type d | xargs ${SUDO} rm
${PKGBUILD} \
			  --root ${DISTRIBUTION_FOLDER} \
			  "${BUILD_FOLDER}/${FUSEXFS_NAME}.pkg" \
			  --identifier Fuse-XFS\
			  --version 0.2.1 \
			  --install-location / \
			  --scripts ${MKPKG_FOLDER}/Install_resources/
${CHOWN_R} root:admin "${BUILD_FOLDER}/${FUSEXFS_NAME}.pkg"

sudo hdiutil create -layout NONE -megabytes 1 -fs HFS+ -volname "${FUSEXFS_NAME}-${FUSEXFS_VERSION}" "${TMP_FOLDER}/${FUSEXFS_NAME}-${FUSEXFS_VERSION}.dmg"
sudo hdiutil attach -private -nobrowse "${TMP_FOLDER}/${FUSEXFS_NAME}-${FUSEXFS_VERSION}.dmg"

VOLUME_PATH="/Volumes/${FUSEXFS_NAME}-${FUSEXFS_VERSION}"
sudo cp -pRX "${BUILD_FOLDER}/${FUSEXFS_NAME}.pkg" "$VOLUME_PATH"

# Set the custom icon.
sudo cp -pRX "${MKPKG_FOLDER}/Install_resources/VolumeIcon.icns" "$VOLUME_PATH"/.VolumeIcon.icns
sudo mkdir "$VOLUME_PATH"/.background
sudo cp -pRX "${MKPKG_FOLDER}/Install_resources/background.png" "$VOLUME_PATH"/.background/background.png
sudo SetFile -a C "$VOLUME_PATH"

# Copy over the license file.
sudo cp "${MKPKG_FOLDER}/Install_resources/README.rtf" "$VOLUME_PATH"/README.rtf
sudo cp "${MKPKG_FOLDER}/Install_resources/Authors.rtf" "$VOLUME_PATH"/Authors.rtf
sudo cp "${MKPKG_FOLDER}/Install_resources/ChangeLog.rtf" "$VOLUME_PATH"/ChangeLog.rtf
sudo cp "${MKPKG_FOLDER}/Install_resources/License.rtf" "$VOLUME_PATH"/License.rtf

# Perform some setup
sleep 3
osascript add_background.osa "${FUSEXFS_NAME}-${FUSEXFS_VERSION}"

# Detach the volume.
hdiutil detach "$VOLUME_PATH"

# Convert to a read-only compressed dmg.
hdiutil convert -imagekey zlib-level=9 -format UDZO "${TMP_FOLDER}/${FUSEXFS_NAME}-${FUSEXFS_VERSION}.dmg" -o "${BUILD_FOLDER}/${FUSEXFS_NAME}-${FUSEXFS_VERSION}.dmg"

${RM_RF} ${TMP_FOLDER} "${BUILD_FOLDER}/${FUSEXFS_NAME}.pkg" ${BUILD_FOLDER}/Info.plist ${BUILD_FOLDER}/FSInfo.plist

echo "SUCCESS: All Done."
exit 0

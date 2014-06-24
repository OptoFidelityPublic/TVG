#!/bin/bash

if [ x$1 == x ]
then echo "
usage: $0 ~/cerbero/dist/windows_x86_64

This script creates a distribution package for OptoFidelity
Test Video Generator. The argument should be a path to the
cerbero dist directory for the platform.
"
exit 1
fi

# Abort on errors
set -e

# Change to the oftvg root directory
BUILDDIR=$(readlink -f "$1")
SCRIPTPATH=$( cd "$(dirname "$0")" ; pwd )
cd $SCRIPTPATH/..

# Create a directory for the distribution
VERSION=$(git describe --always)
PKGDIR=tvg-$VERSION-windows-x64
rm -rf $PKGDIR
mkdir $PKGDIR

echo
echo "Creating package in" $PKGDIR
echo

# This command will copy a file to our distribution directory.
# Usage: pick <source_file> <dest_dir>
function pick {
    mkdir -p $PKGDIR/$2
    cp $BUILDDIR/$1 $PKGDIR/$2
}

# Copy GStreamer .exes and necessary .dlls
BINFILES="
gst-*-1.0.exe
"
for f in $BINFILES $(cat distribution/dlls_to_include.txt)
    do pick bin/$f gstreamer/bin
done

# Copy GStreamer plugins
PLUGINS=$(cat distribution/plugins_to_include.txt)
for f in $PLUGINS
    do pick lib/gstreamer-1.0/$f gstreamer/lib/gstreamer-1.0
done

# Strip debug symbols from files. This reduces the
# size of the distributed package.
STRIP=~/cerbero/mingw/w64/bin/x86_64-w64-mingw32-strip
rm -rf $PKGDIR-debug
mkdir -p $PKGDIR-debug
for f in $PKGDIR/gstreamer/bin/*.exe \
         $PKGDIR/gstreamer/bin/*.dll \
         $PKGDIR/gstreamer/lib/gstreamer-1.0/*.dll
    do cp $f $PKGDIR-debug; $STRIP $f
done   

# Copy env scripts that are necessary for Run_TVG.bat
mkdir $PKGDIR/bin
cp distribution/gst-env.bat $PKGDIR/gstreamer/env.bat

# Copy TVG scripts
cp examples/*.mp4 examples/*.tvg examples/*.bmp $PKGDIR
cp doc/tvg_manual.pdf $PKGDIR
cp scripts/Run_TVG.bat $PKGDIR
mkdir $PKGDIR/debug

# Check for missing DLLs
MISSING="$(distribution/list_needed_dlls.sh $PKGDIR/gstreamer/bin/* $PKGDIR/gstreamer/lib/gstreamer-1.0/*)"
if [ "x$MISSING" != x ]
    then echo "These DLLs are possibly missing from the result package:"
	echo $MISSING
fi

zip -qr $PKGDIR.zip $PKGDIR
zip -qr $PKGDIR-debug.zip $PKGDIR-debug



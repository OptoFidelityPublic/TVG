#!/bin/bash

if [ x$1 == x ]
then echo "
usage: $0 ~/dev/cerbero/build/dist/linux_x86_64

This script creates a distribution package for OptoFidelity
Test Video Generator. The argument should be a path to the
cerbero dist directory for the platform.
"
exit 1
fi

# Abort on errors
#set -e

# Change to the oftvg root directory
BUILDDIR=$(readlink -f "$1")
SCRIPTPATH=$( cd "$(dirname "$0")" ; pwd )
cd $SCRIPTPATH/..

# Create a directory for the distribution
VERSION=$(git describe --always)
PKGDIR=tvg-$VERSION-linux-x64
rm -rf $PKGDIR
mkdir $PKGDIR

echo
echo "Creating package in" $PKGDIR
echo

# This command will copy a file to our distribution directory.
# Usage: pick <source_file> <dest_dir>
function pick {
    mkdir -p $PKGDIR/$2
    cp -P $BUILDDIR/$1 $PKGDIR/$2
}

# Copy GStreamer executables
BINFILES="
gst-*-1.0
tvg_analyzer
"
for f in $BINFILES
    do pick bin/$f gstreamer/bin
done

# GStreamer external plugin scanner
pick libexec/gstreamer-1.0/gst-plugin-scanner gstreamer/bin

# Copy GStreamer library dependencies
for f in $(cat distribution/sos_to_include.txt)
    do pick "lib/$f" gstreamer/lib
done

# Copy GStreamer plugins
PLUGINS=$(cat distribution/plugins_to_include.txt)
PLATFORM_SPECIFIC="libgstvideo4linux2 libgstximagesink
                   libgstximagesrc libgstxvimagesink"
for f in $PLUGINS $PLATFORM_SPECIFIC
    do pick lib/gstreamer-1.0/$f.so gstreamer/lib/gstreamer-1.0
done

# Strip debug symbols from files. This reduces the
# size of the distributed package.
rm -rf $PKGDIR-debug
mkdir -p $PKGDIR-debug
for f in $PKGDIR/gstreamer/bin/* \
         $PKGDIR/gstreamer/lib/* \
         $PKGDIR/gstreamer/lib/gstreamer-1.0/*
    do if [ -f $f ]
       then cp $f $PKGDIR-debug
            strip $f
       fi
done   

# Copy env scripts that are necessary for Run_TVG.sh
mkdir $PKGDIR/bin
cp distribution/gst-env.sh $PKGDIR/gstreamer/env.sh

# Copy TVG scripts
cp examples/*.mp4 examples/*.tvg examples/*.bmp $PKGDIR
cp doc/tvg_manual.pdf $PKGDIR
cp scripts/Run_TVG.sh $PKGDIR
cp scripts/Analyzer.sh $PKGDIR
mkdir $PKGDIR/debug

# Check for missing SOs
COMMAND="distribution/list_needed_sos.sh $PKGDIR/gstreamer/bin/* $PKGDIR/gstreamer/lib/*.so* $PKGDIR/gstreamer/lib/gstreamer-1.0/*"
MISSING="$($COMMAND)"

if [ "x$MISSING" != x ]
    then echo "These SOs are possibly missing from the result package:"
        echo $MISSING
fi

tar -czf $PKGDIR.tar.gz $PKGDIR
tar -czf $PKGDIR-debug.tar.gz $PKGDIR-debug



#!/bin/bash

if [ x$1 == x ]
then echo "
usage: $0 [file] ...

Lists all SOs that are required by the referenced files.
"
exit 1
fi

# Process each file through objdump
LIBS=""
for f in $*
    do LIBS="$(echo "$LIBS"; objdump -p $f | grep 'NEEDED' | grep -o 'lib.*')"
done

# Skip the standard system libraries in output, and also the
# files from the argument list
SYSLIBS="libbz2.so.1.0 libc.so.6 libstdc++.so.6 libm.so.6 libpthread.so.0 libpulse.so.0
         libgcc_s.so.1 libz.so.1 librt.so.1 libX11.so.6 libXdamage.so.1 libXext.so.6
         libXfixes.so.3 libXv.so.1 libdl.so.2 libresolv.so.2 libXrender.so.1"
HAVELIBS=$(for f in $*; do basename $f; done)
for f in $SYSLIBS $HAVELIBS
do LIBS="$(echo "$LIBS" | grep -v $f)"
done

# Print the results
echo "$LIBS" | sort | uniq
exit 0

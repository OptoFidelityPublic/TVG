#!/bin/bash

if [ x$1 == x ]
then echo "
usage: $0 [file.exe] ...

Lists all DLLs that are required by the referenced files.
"
exit 1
fi

# Path to the mingw objdump
OBJDUMP=~/dev/cerbero/build/mingw/w64/bin/x86_64-w64-mingw32-objdump

# Process each file through objdump
DLLS=""
for f in $*
    do DLLS="$(echo "$DLLS"; $OBJDUMP -x $f | grep 'DLL Name' | cut -d : -f 2)"
done

# Skip the standard windows DLLs in output, and also the
# listed dll files.
WINDLLS="ADVAPI32.dll KERNEL32.dll msvcrt.dll ole32.dll
    SHELL32.dll USER32.dll WS2_32.dll DNSAPI.dll
    IPHLPAPI.DLL SHLWAPI.dll d3d9.dll DSOUND.dll GDI32.dll
    WINMM.dll MSIMG32.dll USP10.dll"
HAVEDLLS=$(for f in $*; do basename $f; done)
for f in $WINDLLS $HAVEDLLS
do DLLS="$(echo "$DLLS" | grep -v $f)"
done

# Print the results
echo "$DLLS" | sort | uniq


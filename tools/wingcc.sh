#!/bin/bash
# Runs a Windows gcc of PlatformIO from WSL ninja (tools/build.sh):
#   wingcc.sh [esp32] gcc|g++|ar|size|nm <args...>
#   BK7231N (default): arm-none-eabi GCC 14.2.1, toolchain-gccarmnoneeabi 1.140201.0
#   esp32:             xtensa-esp32-elf GCC 14.2.0, toolchain-xtensa-esp-elf 14.2.0+20260121 (the one of ESP-IDF 5.5.5)
# - /mnt/c/... arguments are rewritten to C:/... (Windows exe can't see WSL paths)
# - ar gets its arguments through a response file (Windows command line limit is 32 KiB)
# - the -MF depfile is rewritten back to /mnt/c/... so ninja can stat the headers
PKG='/mnt/c/Users/igoru/.platformio/packages'
if [ "$1" = "esp32" ]; then
    shift
    TC="$PKG/toolchain-xtensa-esp-elf/bin"
    PREFIX=xtensa-esp32-elf-
    export GCC_EXEC_PREFIX="C:/Users/igoru/.platformio/packages/toolchain-xtensa-esp-elf/lib/gcc/"
else
    TC="$PKG/toolchain-gccarmnoneeabi/bin"
    PREFIX=arm-none-eabi-
    export GCC_EXEC_PREFIX="C:/Users/igoru/.platformio/packages/toolchain-gccarmnoneeabi/lib/gcc/"
fi
export WSLENV="GCC_EXEC_PREFIX${WSLENV:+:$WSLENV}"   # argv[0] from WSL is useless to gcc.exe for finding its prefix
tool=$1
shift
args=()
mf=""
prev=""
for a in "$@"; do
    a="${a//\/mnt\/c\//C:/}"
    args+=("$a")
    [ "$prev" = "-MF" ] && mf="$a"
    prev="$a"
done
if [ "$tool" = "ar" ]; then
    # ar <flags> <archive> <objects...>
    rsp="${args[1]}.wingcc.rsp"
    printf '%s\n' "${args[@]:2}" > "$rsp"
    "$TC/${PREFIX}ar.exe" "${args[0]}" "${args[1]}" "@$rsp"
    rc=$?
    rm -f "$rsp"
    exit $rc
fi
"$TC/${PREFIX}$tool.exe" "${args[@]}"
rc=$?
if [ $rc -eq 0 ] && [ -n "$mf" ] && [ -f "$mf" ]; then
    sed -i -e 's#[Cc]:[/\\]#/mnt/c/#g' -e 's#\r$##' "$mf"
fi
exit $rc

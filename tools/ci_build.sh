#!/bin/bash
# Native Linux build of libMatterApp.a for every chip and the release packages (GitHub Actions, or Linux / WSL by
# hand): the same connectedhomeip, plug.zap data model, application and platform layer; per chip the compiler and SDK
#
#   tools/ci_build.sh <deps_dir> <out_dir>
#
# <deps_dir> gets everything the build needs, at the versions pinned below (skipped when already there, so it can be
# cached): connectedhomeip with the submodules the library uses, a Python venv for the code generators, zap-cli;
# BK7231N: FreeRTOS-Kernel V11.1.0 with its ARM968 port, lwIP 2.2.0 of the BDK port, the BDK 3.0.33 (headers only, no
# LibreTiny) and the Linux build of the arm-none-eabi gcc 14.2 (PlatformIO package, PLATFORMIO_CORE_DIR=<deps_dir>/pio);
# ESP32: stock ESP-IDF (its tools: the xtensa-esp-elf gcc, cmake, ninja, its Python env), the library's ESP-IDF project
# gn/esp32 configured with it (sdkconfig.defaults of the library); mbedTLS 3.6 (BK7231N: headers of Mbed TLS).
# <out_dir> gets the ninja builds (build-<chip>/) and a package per chip:
#   matter-espurna-<chip>.zip         lib/libMatterApp.a (debug info stripped), the headers of the library's API with
#                                     their flags (include/chip), bk7231n also include/lwipopts.h (tools/package.py)
#   matter-espurna-<chip>.zip.sha256  "<sha256>  matter-espurna-<chip>.zip"
#
# Needs git, curl, unzip, xz, python3 with venv and gn on PATH (Ubuntu 24.04: apt install generate-ninja). The Windows
# gcc build of the BK7231N one from WSL is tools/build.sh; the same GN root, this one with bk_toolchain = "native".
set -euo pipefail

M=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$1" "$2"
DEPS=$(cd "$1" && pwd)
OUT=$(cd "$2" && pwd)
JOBS=${JOBS:-8}

# connectedhomeip b87051a95f = Matter 1.5, the commit of esp_matter 1.5.1 (the ESP32 build of ESPurna)
CHIP_URL=https://github.com/project-chip/connectedhomeip
CHIP_COMMIT=b87051a95fc110c0cfbac278f232df55c89866de
CHIP_SUBMODULES="third_party/nlassert/repo third_party/nlio/repo third_party/mbedtls/repo third_party/jsoncpp/repo
                 third_party/pigweed/repo"
ZAP_VERSION=v2025.10.23-nightly          # scripts/setup/zap.version of that commit
# BK7231N: GCC 14.2.1 (Arm GNU Toolchain 14.2.Rel1), the one ESPurna's Beken build takes instead of LibreTiny's 10.3
# (ESPurna libraries.ini beken_toolchain, U25): one compiler for the firmware and this library
GCC_PACKAGE=platformio/toolchain-gccarmnoneeabi@1.140201.0
GCC_VERSION=14.2.1
# ESP32: stock ESP-IDF v5.5.5 (FreeRTOS-Kernel-SMP V11.1.0, esp-lwip 2.2.0, mbedTLS 3.6), its own gcc 14.2
IDF_URL=https://github.com/espressif/esp-idf
IDF_TAG=v5.5.5
IDF_COMMIT=ff1bac0aeecdd2b797b9c3a558c6bd03629bc013
ESP32_GCC_VERSION=14.2.0
# BK7231N: the headers of mbedTLS 3.6.6, the one tree of every chip (Espressif's 3.6.6 that ESP-IDF v5.5.5 takes, the
# framework-mbedtls package of ESPurna); the library carries no mbedTLS: the firmware links its own 3.6, checked by
# matter_requirements.h of the package
MBEDTLS_URL=https://github.com/igorufox/mbedtls
MBEDTLS_COMMIT=b3b5f67d252dcb83bafab82ddc3b280c030fc6d5   # 3.6.6-espurna.1
# BK7231N: the FreeRTOS kernel package with the ARM968 port and FreeRTOSConfig.h of the BK7231N (the one kernel source
# of the firmware and the library), lwIP 2.2.0 of the BDK port (port/arch: cc.h, sys_arch.h)
FREERTOS_URL=https://github.com/igorufox/FreeRTOS-Kernel
FREERTOS_COMMIT=c101968b46bb42e1845e26cc14bc2cebd9c64513   # 11.1.0-espurna.2: V11.1.0 + portable/GCC/ARM968 (configUSE_MINI_LIST_ITEM 0)
# BDK 3.0.33 of the BK7231N (headers of its API only)
BDK_URL=https://github.com/libretiny-eu/framework-beken-bdk
BDK_COMMIT=841343e64d7584a3a650222ab0b6ea967755352a    # 3.0.33
LWIP_URL=https://github.com/libretiny-eu/lwip
LWIP_COMMIT=0d1cedf257bf58ac7c12fb7b79ec79daf2a006bd   # 2.2.0-bdk

CHIP=$DEPS/connectedhomeip
PIO=$DEPS/pio
FREERTOS=$DEPS/freertos-kernel
LWIP=$DEPS/lwip
BDK=$DEPS/bdk
VENV=$DEPS/venv
ZAP=$DEPS/zap-$ZAP_VERSION
IDF=$DEPS/esp-idf
IDF_TOOLS=$DEPS/espressif
MBEDTLS=$DEPS/mbedtls

# shallow checkout of one commit; .ci-commit marks a finished one
checkout() {   # <dir> <url> <commit>
    if [ "$(cat "$1/.ci-commit" 2>/dev/null)" = "$3" ]; then
        return
    fi
    echo "== $2 @ $3"
    rm -rf "$1"
    git init -q "$1"
    git -C "$1" remote add origin "$2"
    git -C "$1" fetch -q --depth 1 origin "$3"
    git -C "$1" -c advice.detachedHead=false checkout -q FETCH_HEAD
    echo "$3" > "$1/.ci-commit"
}

checkout "$CHIP" "$CHIP_URL" "$CHIP_COMMIT"
if [ ! -f "$CHIP/.ci-submodules" ]; then
    # shellcheck disable=SC2086
    git -C "$CHIP" submodule update -q --init --depth 1 -- $CHIP_SUBMODULES
    touch "$CHIP/.ci-submodules"
fi

checkout "$FREERTOS" "$FREERTOS_URL" "$FREERTOS_COMMIT"
checkout "$LWIP" "$LWIP_URL" "$LWIP_COMMIT"
checkout "$BDK" "$BDK_URL" "$BDK_COMMIT"

if [ ! -f "$VENV/.ci-done" ]; then
    echo "== Python venv"
    rm -rf "$VENV"
    python3 -m venv "$VENV"
    "$VENV/bin/pip" install -q -r "$CHIP/scripts/setup/requirements.build.txt" ninja platformio
    touch "$VENV/.ci-done"
fi

GCC_BIN=$PIO/packages/toolchain-gccarmnoneeabi/bin
# a cache of an older pin (restore-keys) has another gcc there: replaced, not kept
if [ -x "$GCC_BIN/arm-none-eabi-gcc" ] && ! "$GCC_BIN/arm-none-eabi-gcc" --version | head -1 | grep -q " $GCC_VERSION "; then
    rm -rf "$PIO/packages/toolchain-gccarmnoneeabi"
fi
if [ ! -x "$GCC_BIN/arm-none-eabi-gcc" ]; then
    echo "== $GCC_PACKAGE"
    PLATFORMIO_CORE_DIR=$PIO "$VENV/bin/pio" pkg install -g -t "$GCC_PACKAGE"
fi
"$GCC_BIN/arm-none-eabi-gcc" --version | head -1 | grep -q " $GCC_VERSION " || {
    echo "arm-none-eabi-gcc is not $GCC_VERSION:"; "$GCC_BIN/arm-none-eabi-gcc" --version | head -1; exit 1; }

if [ ! -x "$ZAP/zap-cli" ]; then
    echo "== zap-cli $ZAP_VERSION"
    rm -rf "$ZAP" "$ZAP.zip"
    curl -fsSL -o "$ZAP.zip" "https://github.com/project-chip/zap/releases/download/$ZAP_VERSION/zap-linux-x64.zip"
    unzip -q "$ZAP.zip" -d "$ZAP"
    rm "$ZAP.zip"
    chmod +x "$ZAP/zap-cli"
fi

# a tar.xz unpacked into <dir> once; .ci-url marks a finished one
fetch_tar() {   # <dir> <url>
    if [ "$(cat "$1/.ci-url" 2>/dev/null)" = "$2" ]; then
        return
    fi
    echo "== $2"
    rm -rf "$1" "$1.tar.xz"
    mkdir -p "$1"
    curl -fsSL -o "$1.tar.xz" "$2"
    tar -xJf "$1.tar.xz" -C "$1"
    rm "$1.tar.xz"
    echo "$2" > "$1/.ci-url"
}

checkout "$MBEDTLS" "$MBEDTLS_URL" "$MBEDTLS_COMMIT"

# ESP-IDF with the submodules its configure needs (mbedtls, lwip; the Wi-Fi / PHY blobs are not linked here), its
# tools for esp32 (gcc, cmake, ninja, the Python env)
checkout "$IDF" "$IDF_URL" "$IDF_COMMIT"
if [ ! -f "$IDF/.ci-submodules" ]; then
    git -C "$IDF" submodule update -q --init --depth 1 -- components/mbedtls/mbedtls components/lwip/lwip \
        components/esp_wifi/lib components/esp_phy/lib components/esp_coex/lib components/bootloader/subproject/components/micro-ecc/micro-ecc
    touch "$IDF/.ci-submodules"
fi
if [ ! -f "$IDF_TOOLS/.ci-$IDF_COMMIT" ]; then
    echo "== ESP-IDF $IDF_TAG tools"
    IDF_TOOLS_PATH=$IDF_TOOLS "$IDF/install.sh" esp32
    touch "$IDF_TOOLS/.ci-$IDF_COMMIT"
fi
(
    export IDF_TOOLS_PATH=$IDF_TOOLS
    # the Kconfig of connectedhomeip's ESP32 component (gn/esp32/components/chip_config)
    export CHIP_ROOT=$CHIP
    # shellcheck disable=SC1091
    . "$IDF/export.sh" > /dev/null
    echo "== ESP-IDF project gn/esp32 (the library's sdkconfig.defaults)"
    rm -rf "$OUT/esp32-idf"
    idf.py -C "$M/gn/esp32" -B "$OUT/esp32-idf" -DSDKCONFIG="$OUT/esp32-idf/sdkconfig" \
        -DSDKCONFIG_DEFAULTS="$M/gn/esp32/sdkconfig.defaults" set-target esp32
    idf.py -C "$M/gn/esp32" -B "$OUT/esp32-idf" -DSDKCONFIG="$OUT/esp32-idf/sdkconfig" \
        -DSDKCONFIG_DEFAULTS="$M/gn/esp32/sdkconfig.defaults" reconfigure
    echo "$(dirname "$(command -v xtensa-esp32-elf-gcc)")" > "$OUT/esp32-idf/gcc-bin"
)
ESP32_BIN=$(cat "$OUT/esp32-idf/gcc-bin")
"$ESP32_BIN/xtensa-esp32-elf-gcc" --version | head -1 | grep -q " $ESP32_GCC_VERSION" || {
    echo "xtensa-esp32-elf-gcc is not $ESP32_GCC_VERSION:"; "$ESP32_BIN/xtensa-esp32-elf-gcc" --version | head -1; exit 1; }

command -v gn > /dev/null || { echo "gn is not on PATH (Ubuntu: apt install generate-ninja)"; exit 1; }

# the GN root finds connectedhomeip and the PlatformIO packages through these links (tools/build.sh makes NTFS
# junctions instead, for the Windows gcc)
mkdir -p "$M/gn/third_party"
for link in "connectedhomeip:$CHIP" "pio:$PIO" "freertos-kernel:$FREERTOS" "lwip:$LWIP" "bdk:$BDK" \
            "esp32-toolchain:$(dirname "$ESP32_BIN")" "esp-idf:$IDF" \
            "esp32-config:$OUT/esp32-idf/config" "mbedtls:$MBEDTLS"; do
    name=${link%%:*}
    target=${link#*:}
    rm -rf "${M:?}/gn/third_party/$name"
    ln -s "$target" "$M/gn/third_party/$name"
done
# the data model target has to live under connectedhomeip/examples (visibility of src/app:required-privileges)
mkdir -p "$CHIP/examples/espurna-plug"
cp "$M/gn/chip-examples/espurna-plug/BUILD.gn" "$CHIP/examples/espurna-plug/BUILD.gn"
# the flags ESP-IDF gives a component of gn/esp32, for the ESP32 build
python3 "$M/tools/gen_esp32_flags.py" "$IDF" "$OUT/esp32-idf" "$M/gn/chips/esp32_flags.gni"

# shellcheck disable=SC1091
. "$VENV/bin/activate"
export PATH=$ZAP:$PATH
export ZAP_INSTALL_PATH=$ZAP
export PYTHONPATH=$CHIP/scripts/py_matter_idl:$CHIP/scripts/py_matter_yamltests

echo "== build: connectedhomeip $(git -C "$CHIP" log -1 --format='%h %ad' --date=short)," \
     "$("$GCC_BIN/arm-none-eabi-gcc" --version | head -1), $("$ESP32_BIN/xtensa-esp32-elf-gcc" --version | head -1)," \
     "gn $(gn --version), ninja $(ninja --version)"
cd "$M/gn"

# one chip: <chip> <gn args> <strip> <nm>
build() {
    echo "== build $1"
    # no include check (`--check`, any value of it turns it on): the platform layer includes headers of targets it does
    # not depend on (OTAImageProcessorImpl.h: ota-requestor)
    gn gen "$OUT/build-$1" --args="$2"
    ninja -j "$JOBS" -C "$OUT/build-$1" default

    echo "== package $1"
    export NM="$4"
    "$3" --strip-debug -o "$OUT/libMatterApp-$1.stripped.a" "$OUT/build-$1/lib/libMatterApp.a"
    python3 "$M/tools/package.py" "$1" "$M" "$CHIP" "$OUT/build-$1" "$OUT/libMatterApp-$1.stripped.a" "$OUT/pkg-$1" \
        "$OUT/matter-espurna-$1.zip"
    (cd "$OUT" && sha256sum "matter-espurna-$1.zip" > "matter-espurna-$1.zip.sha256")
    ls -l "$OUT/build-$1/lib/libMatterApp.a" "$OUT/pkg-$1/lib/libMatterApp.a" "$OUT/matter-espurna-$1.zip"
    cat "$OUT/matter-espurna-$1.zip.sha256"
}

build bk7231n 'import("//chips/bk7231n.gni") bk_toolchain="native"' "$GCC_BIN/arm-none-eabi-strip" "$GCC_BIN/arm-none-eabi-nm"
build esp32 'import("//chips/esp32.gni") esp32_toolchain="native"' "$ESP32_BIN/xtensa-esp32-elf-strip" "$ESP32_BIN/xtensa-esp32-elf-nm"

#!/bin/bash
# Build libMatterApp.a of one chip with the Windows gcc of PlatformIO (both chips natively on Linux: tools/ci_build.sh).
# Run in WSL:  [MATTER_CHIP=esp32] bash /mnt/c/working/ginhub/libs/matter-espurna/tools/build.sh [out_dir]
# MATTER_CHIP: bk7231n (default) or esp32 (stock ESP-IDF 5.5.5 of PlatformIO, configured by tools/esp32_config.ps1)
# connectedhomeip b87051a95f (Matter 1.5, the commit of esp_matter 1.5.1) at ../libs/connectedhomeip, zap-cli
# v2025.10.23-nightly in tools/zap-2025.10.23. (The last build on v1.4.2.0 is commit cefb35a of this repository.)
set -e
M=/mnt/c/working/ginhub/libs/matter-espurna
MATTER_CHIP=${MATTER_CHIP:-bk7231n}
case "$MATTER_CHIP" in
    bk7231n) OUT=${1:-$M/out/plug}; GN_ARGS='import("//chips/bk7231n.gni")'; NM_CMD="$M/tools/wingcc.sh nm" ;;
    esp32)   OUT=${1:-$M/out/esp32}; GN_ARGS='import("//chips/esp32.gni")'; NM_CMD="$M/tools/wingcc.sh esp32 nm" ;;
    *) echo "MATTER_CHIP: bk7231n or esp32"; exit 1 ;;
esac
CHIP_DIR=/mnt/c/working/ginhub/libs/connectedhomeip
ZAP_DIR=$M/tools/zap-2025.10.23

PIO_DIR=/mnt/c/Users/igoru/.platformio   # the PlatformIO home: the gcc and the SDK packages the library is compiled against

# gn/third_party/{connectedhomeip,pio,freertos-kernel,lwip,mbedtls,bdk; esp32: esp-idf,esp32-config}: NTFS junctions
# made on the Windows side (the Windows gcc can not follow a WSL symlink; tools/ci_build.sh makes plain symlinks instead)
junction() {   # <name under gn/third_party> <target under /mnt/c>
    local link=$M/gn/third_party/$1
    if [ "$(readlink "$link" 2>/dev/null)" != "$2" ]; then
        local link_win="C:\\working\\ginhub\\libs\\matter-espurna\\gn\\third_party\\$1"
        local target_win="C:/${2#/mnt/c/}"
        target_win="${target_win//\//\\}"
        mkdir -p "$M/gn/third_party"
        /mnt/c/Windows/System32/cmd.exe /c "rmdir $link_win" >/dev/null 2>&1 || rm -f "$link"
        /mnt/c/Windows/System32/cmd.exe /c "mklink /J $link_win $target_win" >/dev/null
    fi
    [ "$(readlink "$link")" = "$2" ] || { echo "gn/third_party/$1 does not point at $2"; exit 1; }
}
junction connectedhomeip "$CHIP_DIR"
junction pio "$PIO_DIR"
# FreeRTOS-Kernel V11.1.0 + the ARM968 port, lwIP 2.2.0 of the BDK port, Mbed TLS 3.6.6 (headers): the same packages
# as tools/ci_build.sh pins
junction freertos-kernel "$PIO_DIR/packages/framework-freertos-kernel"
junction lwip "$PIO_DIR/packages/library-lwip"
junction mbedtls "$PIO_DIR/packages/framework-mbedtls"
junction bdk "$PIO_DIR/packages/framework-beken-bdk"
if [ "$MATTER_CHIP" = esp32 ]; then
    # the ESP-IDF project of the library configured on the Windows side, gn/chips/esp32_flags.gni from it
    /mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass \
        -File 'C:/working/ginhub/libs/matter-espurna/tools/esp32_config.ps1'
    junction esp-idf "$PIO_DIR/packages/framework-espidf"
    junction esp32-config "$M/out/esp32-idf/config"
fi

# the data model target has to live under connectedhomeip/examples (visibility of src/app:required-privileges)
mkdir -p "$CHIP_DIR/examples/espurna-plug"
cp "$M/gn/chip-examples/espurna-plug/BUILD.gn" "$CHIP_DIR/examples/espurna-plug/BUILD.gn"
python3 "$M/tools/chip_patch.py" "$CHIP_DIR"

echo "connectedhomeip: $(git -C "$CHIP_DIR" log -1 --format='%h %ad' --date=short), zap: $ZAP_DIR"
. $M/.venv-wsl/bin/activate
export PATH=$M/tools/gnroot/usr/bin:$ZAP_DIR:$PATH
export ZAP_INSTALL_PATH=$ZAP_DIR
export PYTHONPATH=$M/gn/third_party/connectedhomeip/scripts/py_matter_idl:$M/gn/third_party/connectedhomeip/scripts/py_matter_yamltests
cd $M/gn
# no include check (any value of `--check` turns it on): the ESP32 platform includes headers of targets it does not
# depend on, as in tools/ci_build.sh
gn gen "$OUT" --args="$GN_ARGS"
ninja -j "${JOBS:-8}" -k 0 -C "$OUT" ${NINJA_TARGETS:-default}

# the package ESPurna takes, unzipped: ESPURNA_MATTER_LIB_DIR=<out>/pkg (lib/, include/ with the headers of the API)
NM="$NM_CMD" python3 "$M/tools/package.py" "$MATTER_CHIP" "$M" "$M/gn/third_party/connectedhomeip" "$OUT" "$OUT/lib/libMatterApp.a" "$OUT/pkg"

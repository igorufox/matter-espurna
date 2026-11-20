# The ESP-IDF configuration of the ESP32 library for a local build (tools/build.sh with MATTER_CHIP=esp32 runs it; CI
# does the same with idf.py in tools/ci_build.sh): the library's ESP-IDF project gn/esp32 configured with stock ESP-IDF
# 5.5.5 of PlatformIO (framework-espidf, its Python env, cmake, ninja, the xtensa gcc) into out/esp32-idf, then
# gn/chips/esp32_flags.gni from its compile_commands.json.
#
#   powershell -ExecutionPolicy Bypass -File tools\esp32_config.ps1

$ErrorActionPreference = "Stop"
$M = Split-Path -Parent $PSScriptRoot
$P = Join-Path $env:USERPROFILE ".platformio"
$env:IDF_PATH = Join-Path $P "packages\framework-espidf"
$env:IDF_PYTHON_ENV_PATH = Join-Path $P "penv\.espidf-5.5.5"
$env:PATH = "$P\packages\toolchain-xtensa-esp-elf\bin;$P\packages\tool-ninja;$P\packages\tool-cmake\bin;$env:PATH"
$py = Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"
$out = Join-Path $M "out\esp32-idf"
# the Kconfig of connectedhomeip's ESP32 component (gn/esp32/components/chip_config)
$env:CHIP_ROOT = (Join-Path $M "gn/third_party/connectedhomeip") -replace "\\", "/"

if (Test-Path $out) { Remove-Item -Recurse -Force $out }
# PYTHON_DEPS_CHECKED: the Python env of PlatformIO is not an install.ps1 one (no constraints file), as its own builds
& cmake -S (Join-Path $M "gn\esp32") -B $out -G Ninja "-DPYTHON=$py" -DPYTHON_DEPS_CHECKED=1 -DIDF_TARGET=esp32 `
    "-DSDKCONFIG=$out\sdkconfig" "-DSDKCONFIG_DEFAULTS=$M\gn\esp32\sdkconfig.defaults" -DCMAKE_EXPORT_COMPILE_COMMANDS=1
if ($LASTEXITCODE -ne 0) { throw "ESP-IDF configure of gn/esp32 failed" }

& $py (Join-Path $M "tools\gen_esp32_flags.py") $env:IDF_PATH $out (Join-Path $M "gn\chips\esp32_flags.gni")
if ($LASTEXITCODE -ne 0) { throw "gen_esp32_flags.py failed" }

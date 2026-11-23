# matter-espurna

Matter (connectedhomeip) on two chips, the Beken BK7231N and the ESP32 (ESP-IDF 5.5), as a static library per chip,
ESPurna's and anyone's who links it (no Arduino, no LibreTiny, no SDK and no mbedTLS of its own inside):
`libMatterApp.a` with On/Off plugs, one per relay (`gn/app/plug.zap`), and
a small C API that ESPurna calls (`espurna_matter.h` of the ESPurna fork). The same connectedhomeip, data model and
application on both chips; only the platform of connectedhomeip under it is the chip's. (Until 2026-11-12 the
repository was `matter-bk7231n`, the library of the BK7231N alone; the ESP32 build of ESPurna had the Matter library of
Arduino-ESP32, esp_matter, with another data model.)

- `gn/` - own GN root, one for both chips (`.gn`; the chip's arguments come with `gn gen`: `chips/bk7231n.gni`,
  `chips/esp32.gni`), `toolchain/` (arm-none-eabi for the BK7231N, xtensa-esp32-elf for the ESP32), the application
  (`app/`: data model, `matter_app.cpp` with the C API, `AppServer`, `ProjectConfig.h`) and ONE platform layer for every
  chip, `external_platform/Port/` (derived from the Beken platform of connectedhomeip for the Armino SDK): Wi-Fi,
  key-value store, events, OTA, log and heap size come from the firmware through `matter_port.h`; FreeRTOS and lwIP of
  the SDK under it. The build arguments every chip shares are `chips/matter.gni`; per chip only the compiler and SDK:
  - BK7231N: FreeRTOS-Kernel V11.1.0 with its ARM968 port, lwIP 2.2.0 of the BDK port with the library's
    `lwipopts/lwipopts.h`, the headers of Mbed TLS 3.6.6 with the library's config (`mbedtls/`), BDK 3.0.33 headers
    with the library's `bdk/sys_config.h` (`chips/bk7231n_sdk.gni`); the `__atomic_*` of the ARMv5 in
    `external_platform/Port/atomic_armv5.c`
  - ESP32: the flags, include folders and `sdkconfig.h` stock ESP-IDF 5.5.5 gives a component of the library's
    ESP-IDF project `esp32/` (its own `sdkconfig.defaults`: FreeRTOS 11 SMP, 1 ms tick, IPv6, HKDF;
    `chips/esp32_flags.gni`, made by `tools/gen_esp32_flags.py`), mbedTLS, lwIP and FreeRTOS of ESP-IDF (`mbedtls_idf/`)
  The identity of the board (factory data or an own development DAC), the pairing codes, the controllers and the pairing
  window are ESPurna's (`matter_identity.cpp`, `matter_app.cpp` of the fork): the library calls
  `matter_identity_install()` before the server init
- what the library needs from the firmware: `gn/app/matter_requirements.h` (FreeRTOS 11, lwIP IPv4 + IPv6 + UDP with
  multicast, mbedTLS 3.6 with the modules Matter calls) and `matter_abi.h`, generated from the build
  (`gn/app/matter_abi_probe.c`, `tools/abi.py`): the sizes of the FreeRTOS, lwIP and mbedTLS types the library shares
  with the firmware. The firmware compiles `<app/matter_requirements.h>` with its own SDK flags: a firmware that lacks
  something or lays a shared type out otherwise stops at build time. The firmware links its own FreeRTOS, lwIP and
  mbedTLS 3.6 (ESPurna: the parts only Matter uses go into its matter module)
- `tools/` - `build.sh` (WSL: gn + ninja with the Windows arm-none-eabi gcc through `wingcc.sh`, the package
  unzipped in `<out>/pkg` for `ESPURNA_MATTER_LIB_DIR`), `package.py` (the package of both builds), flag generators,
  size reports

connectedhomeip is not part of this repository: commit `b87051a95f` (Matter 1.5, the same commit as esp_matter 1.5.1
of the ESP32 build of ESPurna) at `C:\working\ginhub\libs\connectedhomeip` (the owner's folder of libraries; upstream
as it is, not patched) with the submodules nlassert, nlio, mbedtls,
jsoncpp and pigweed, linked as `gn/third_party/connectedhomeip` (an NTFS junction: the Windows gcc can not follow a
WSL symlink; `build.sh` makes it). The data model target (`gn/chip-examples/espurna-plug/BUILD.gn`) is copied into
`<connectedhomeip>/examples/espurna-plug` by `build.sh` / `ci_build.sh`: since 1.5 a `chip_data_model` may use
`src/app:required-privileges` only from there. Not committed either: `out/` (build output), `.venv-wsl/` (Python for
the code generators, + `scripts/setup/requirements.build.txt` of connectedhomeip), `tools/gnroot` (gn),
`tools/zap-2025.10.23` (zap-cli v2025.10.23-nightly, the version `scripts/setup/zap.version` asks for).

`gn/app/plug.matter` is generated from `plug.zap` (after a connectedhomeip update: `scripts/tools/zap/convert.py` and
`scripts/tools/zap/generate.py plug.zap -t src/app/zap-templates/matter-idl-server.json -o gn/app` of connectedhomeip).

Build (WSL):

    bash /mnt/c/working/ginhub/libs/matter-espurna/tools/build.sh /mnt/c/working/ginhub/libs/matter-espurna/out/plug-tuned-15

The gcc and the SDK packages (`framework-freertos-kernel`, `library-lwip`, `framework-mbedtls` of
`C:\Users\igoru\.platformio`) go through junctions in `gn/third_party` that `build.sh` makes.

Configs: `out/plug-tuned-15` (Quectel factory data of the board), `out/plug-test` (SDK test attestation, VID 0xFFF1 /
PID 0x8001, `ProjectConfigTest.h`). A change of the project config or of `lwipopts.h` rebuilds most of the
library (10-15 min). The last build on connectedhomeip v1.4.2.0 is commit `cefb35a`.

Fixes that made the speaker hub keep the device (2026-11-05):
- `lwipopts.h`: `PBUF_POOL_SIZE` 6 (was 3) - Matter's packet buffers come from this pool; at AddNOC it ran empty,
  the AddNOC reply and the operational mDNS announcement were dropped
- `DiagnosticDataProviderImpl::GetNetworkInterfaces()` - IsOperational, MAC, IPv4 and IPv6 addresses (the hub reads
  them right after CommissioningComplete and removed its fabric when the interface looked down)

## Releases

GitHub Actions (`.github/workflows/release.yml`) build the library after every push to `main` that changes `gn/`,
`tools/` or the workflow (and on "Run workflow"): `tools/ci_build.sh` on Ubuntu 24.04, natively with
the Linux builds of the same compilers (BK7231N: the PlatformIO package `toolchain-gccarmnoneeabi@1.140201.0`,
GCC 14.2.1, as ESPurna's Beken build; ESP32: the gcc 14.2 ESP-IDF installs) and the same pinned connectedhomeip,
FreeRTOS kernel package (igorufox/FreeRTOS-Kernel 11.1.0-espurna.2), lwIP, mbedTLS 3.6.6 (igorufox/mbedtls
3.6.6-espurna.1) and ESP-IDF v5.5.5 (all pins are at the top of `ci_build.sh`). Each build is published as a release `libmatter-<yyyymmdd>-<sha7>` with, per chip (`bk7231n`, `esp32`):

- `matter-espurna-<chip>.zip` - `lib/libMatterApp.a` (debug info stripped) and the headers of the library's API:
  `include/chip/` (connectedhomeip `src` + `src/include`, the generated `zzz_generated/app-common`, nlassert, nlio, the generated build config, the project
  config with `app/matter_requirements.h` and `app/matter_abi.h`; bk7231n also its platform headers) with
  `include/chip/flags.txt` (its include folders and defines). ESPurna's code on the Matter API is compiled against them (`tools/package.py`)
- `matter-espurna-<chip>.zip.sha256`

The newest 5 of these releases are kept, older ones are deleted with their tags. ESPurna pins one in
`platformio_beken.ini` and `platformio_esp32.ini` (`custom_matter_lib`, checked against its SHA-256,
`scripts/matter_lib.py`). The same build on any
Linux, WSL included (`gn` on PATH):

    bash tools/ci_build.sh <deps_dir> <out_dir>

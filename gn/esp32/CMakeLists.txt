# The ESP-IDF configuration of the ESP32 library: an ESP-IDF project that is only configured, never built
# (`idf.py reconfigure`, tools/ci_build.sh). Its sdkconfig.h, and the include folders, defines and flags ESP-IDF gives
# the sources of main (build/compile_commands.json), become gn/chips/esp32_flags.gni (tools/gen_esp32_flags.py): the
# library is compiled as a component of this project would be. Stock ESP-IDF of the version tools/ci_build.sh pins,
# configured by sdkconfig.defaults here (the library's own; what the firmware has to match is checked by
# matter_requirements.h of the release package).
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(matter-espurna-esp32)

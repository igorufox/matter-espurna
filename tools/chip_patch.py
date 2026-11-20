#!/usr/bin/env python3
"""The changes of the library to the connectedhomeip tree (both build scripts run it, it may run again).

- src/platform/ESP32/BUILD.gn: with chip_mdns = "platform" the ESP32 platform brings its DNS-SD on the mdns
  component of ESP-IDF (DnssdImpl.cpp, ESP32DnssdImpl.cpp). The library's own platform DNS-SD (gn/platform/
  EspurnaDnssd.cpp: ESPurna's one mDNS responder) takes its place on both chips: those sources go.
- src/platform/ESP32/NetworkCommissioningDriver.cpp: a network given by a controller (commissioning over BLE) goes
  to the firmware (matter_wifi_connect(), ESPurna's Wi-Fi settings) instead of esp_wifi_set_config().

  python3 tools/chip_patch.py <connectedhomeip>
"""

import os
import re
import sys


def esp32(chip):
    path = os.path.join(chip, "src", "platform", "ESP32", "BUILD.gn")
    with open(path, encoding="utf-8") as f:
        text = f.read()
    patched = re.sub(r'\n\s*if \(chip_mdns == "platform"\) \{\s*sources \+= \[ "DnssdImpl\.cpp" \]\s*\}', "\n", text)
    patched = re.sub(r'\n\s*if \(chip_mdns == "platform"\) \{\s*sources \+= \[\s*"ESP32DnssdImpl\.cpp",\s*'
                     r'"ESP32DnssdImpl\.h",\s*\]\s*\}', "\n", patched)
    # the exact names: OpenThread's "../OpenThread/OpenThreadDnssdImpl.cpp" stays (not built, no OpenThread here)
    if ('"DnssdImpl.cpp"' in patched) or ('"ESP32DnssdImpl.cpp"' in patched):
        raise SystemExit(f"{path}: the DNS-SD sources of the ESP32 platform are still there (a new layout?)")
    if patched != text:
        with open(path, "w", encoding="utf-8") as f:
            f.write(patched)
        print(f"chip_patch: {path}: the DNS-SD of ESP-IDF removed")


# the ESP32 platform's Wi-Fi commissioning driver configures esp_wifi itself; the firmware owns Wi-Fi (its settings,
# its driver): the network a controller gives over BLE goes to the firmware through matter_wifi_connect() instead
WIFI_HOOK = """// matter-espurna (tools/chip_patch.py): the firmware owns Wi-Fi, the network a controller gives goes to it
extern "C" int matter_wifi_connect(const char * ssid, size_t ssid_length, const char * key, size_t key_length);

CHIP_ERROR ESPWiFiDriver::ConnectWiFiNetwork(const char * ssid, uint8_t ssidLen, const char * key, uint8_t keyLen)
{
    if (ssidLen)
    {
        return (matter_wifi_connect(ssid, ssidLen, key, keyLen) == 0) ? CHIP_NO_ERROR : CHIP_ERROR_INTERNAL;
    }
"""


def esp32_wifi(chip):
    path = os.path.join(chip, "src", "platform", "ESP32", "NetworkCommissioningDriver.cpp")
    with open(path, encoding="utf-8") as f:
        text = f.read()
    if "matter_wifi_connect" in text:
        return
    head = ("CHIP_ERROR ESPWiFiDriver::ConnectWiFiNetwork(const char * ssid, uint8_t ssidLen, const char * key, "
            "uint8_t keyLen)\n{\n")
    if text.count(head) != 1:
        raise SystemExit(f"{path}: ESPWiFiDriver::ConnectWiFiNetwork not found as expected (a new layout?)")
    with open(path, "w", encoding="utf-8") as f:
        f.write(text.replace(head, WIFI_HOOK))
    print(f"chip_patch: {path}: Wi-Fi networks go to the firmware (matter_wifi_connect)")


def main():
    esp32(sys.argv[1])
    esp32_wifi(sys.argv[1])


if __name__ == "__main__":
    main()

/* sys_config.h of BDK 3.0.33 for the BK7231N library: the BDK's makefile makes it from app/config/sys_config_bk7231n.h,
 * the library includes that one itself (no LibreTiny, no file of the firmware). Of it the library only sees the
 * declarations of the BDK API matter_pal.h uses (rtos_pub.h, wlan_ui_pub.h, net.h, BkDriverFlash.h); the switches below
 * as the BK7231N boards run: no BLE, no PMF / WPA3 in the BDK's own supplicant config. */
#pragma once

#include "sys_config_bk7231n.h"

#undef CFG_SUPPORT_BLE
#define CFG_SUPPORT_BLE 0
#undef CFG_IEEE80211W
#define CFG_IEEE80211W 0
#undef CFG_WPA3
#define CFG_WPA3 0

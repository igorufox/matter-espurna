/*
 * The platform layer of the library (external_platform/Port), the same on every chip: what it needs from the firmware
 * under it. The firmware implements these functions; the layer itself only uses FreeRTOS and lwIP besides them.
 * The code of the layer is connectedhomeip v1.4.2.0 src/platform/Beken (ARMINO) with its vendor calls replaced by these.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 0 ok, anything else an error */
#define MATTER_PORT_OK 0
#define MATTER_PORT_FAIL -1
/* matter_port_kv_read(): the value is longer than the buffer - the buffer holds its first len bytes, out_len = len */
#define MATTER_PORT_BUFFER_TOO_SMALL -2

#define MATTER_PORT_LOG_ON_ERR(x)                                                                                         \
    do                                                                                                                    \
    {                                                                                                                     \
        int _e = (x);                                                                                                     \
        if (_e != MATTER_PORT_OK)                                                                                         \
            ChipLogError(DeviceLayer, "%s:%d err %d", __func__, __LINE__, _e);                                            \
    } while (0)

/* ---------- key-value store: namespace + name ---------- */
int matter_port_kv_read(const char * ns, const char * name, void * buf, size_t len, size_t * out_len);
int matter_port_kv_write(const char * ns, const char * name, const void * buf, size_t len);
int matter_port_kv_remove(const char * ns, const char * name);
bool matter_port_kv_exists(const char * ns, const char * name);
/* every key of one namespace */
int matter_port_kv_clear(const char * ns);
/* every key of the stack (factory reset) */
int matter_port_kv_erase_all(void);

/* ---------- events: the station and its IPv4 address ---------- */
typedef enum
{
    MATTER_PORT_EVENT_WIFI,
    MATTER_PORT_EVENT_NETIF,
} matter_port_event_module_t;

#define MATTER_PORT_EVENT_ID_ALL -1
enum
{
    MATTER_PORT_EVENT_WIFI_SCAN_DONE = 0,
    MATTER_PORT_EVENT_WIFI_STA_CONNECTED,
    MATTER_PORT_EVENT_WIFI_STA_DISCONNECTED,
};
enum
{
    MATTER_PORT_EVENT_NETIF_GOT_IP4 = 0,
};

/* the data of MATTER_PORT_EVENT_WIFI_STA_DISCONNECTED */
typedef struct
{
    int disconnect_reason;
    bool local_generated;
} matter_port_sta_disconnected_t;

typedef int (*matter_port_event_cb_t)(void * arg, matter_port_event_module_t module, int event_id, void * event_data);
/* a callback registered while the station is connected hears STA_CONNECTED / GOT_IP4 at once */
int matter_port_event_register(matter_port_event_module_t module, int event_id, matter_port_event_cb_t cb, void * arg);
int matter_port_event_unregister(matter_port_event_module_t module, int event_id, matter_port_event_cb_t cb);

/* ---------- Wi-Fi station: the firmware's own ---------- */
typedef enum
{
    MATTER_PORT_SECURITY_UNSPECIFIED = 0,
    MATTER_PORT_SECURITY_NONE,
    MATTER_PORT_SECURITY_WEP,
    MATTER_PORT_SECURITY_WPA,
    MATTER_PORT_SECURITY_WPA2,
    MATTER_PORT_SECURITY_WPA3,
} matter_port_security_t;

typedef struct
{
    char ssid[33];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi;
    matter_port_security_t security;
} matter_port_link_t;

/* a network a controller gives (Network Commissioning): the firmware connects to it; the result comes as events */
int matter_port_wifi_connect(const char * ssid, size_t ssid_len, const char * password, size_t password_len);
/* MATTER_PORT_OK only while connected */
int matter_port_wifi_link(matter_port_link_t * link);
void matter_port_wifi_mac(uint8_t mac[6]);
/* the netif of the station (lwIP) */
struct netif * matter_port_sta_netif(void);

typedef struct
{
    char ssid[33];
    uint8_t bssid[6];
    int8_t rssi;
    uint8_t channel;
    matter_port_security_t security;
} matter_port_scan_ap_t;

/* ssid NULL: every network; MATTER_PORT_EVENT_WIFI_SCAN_DONE when the results are there */
int matter_port_wifi_scan_start(const char * ssid);
/* the results of the last scan, freed with matter_port_wifi_scan_free() */
int matter_port_wifi_scan_result(matter_port_scan_ap_t ** aps, size_t * count);
void matter_port_wifi_scan_free(matter_port_scan_ap_t * aps);

/* ---------- system ---------- */
void matter_port_reboot(void);
void matter_port_log(const char * line);
/* the size of the heap the allocator has (Software Diagnostics) */
size_t matter_port_heap_total(void);

/* ---------- OTA: the image goes to the firmware's own update path ---------- */
/* the first bytes of the image (size 0: not known); MATTER_PORT_OK: started */
int matter_port_ota_begin(const uint8_t * data, size_t len, size_t size);
int matter_port_ota_write(const uint8_t * data, size_t len);
/* the whole image is in: checked, the next boot runs it */
int matter_port_ota_end(void);
void matter_port_ota_abort(void);

#ifdef __cplusplus
}
#endif

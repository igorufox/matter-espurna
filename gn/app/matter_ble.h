/* Matter over BLE (CHIPoBLE: commissioning of a board that is not on Wi-Fi yet): the C API between the library and the
 * firmware's BLE stack - the package's include/chip/gn/app/matter_ble.h.
 *
 * The library has connectedhomeip's BLE transport (BleLayer, BTP) and one BLEManagerImpl for every chip
 * (gn/platform/ble), but no BLE stack: the firmware's stack serves the CHIPoBLE GATT service and the advertisement
 * through the operations below, which an optional part of the firmware hands over with matter_ble_attach() (ESPurna: the
 * matter_ble module, on its ble module). Without it Matter runs on the network only, as before.
 *
 * What the stack serves (one connection at a time is enough):
 *   service      16-bit UUID 0xFFF6
 *   C1 (RX)      18EE2EF5-263D-4559-959F-4F9C429F9D11  write, write without response: the controller writes here
 *   C2 (TX)      18EE2EF5-263D-4559-959F-4F9C429F9D12  indicate (CCCD): the library sends here
 *   advertising  connectable, undirected; the data and the scan response are the library's (matter_ble_ops.advertise)
 *
 * The stack's events are reported with the matter_ble_* functions from any task; the library copies what it needs and
 * handles it in the Matter task. The operations are called from the Matter task. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct matter_ble_ops
{
    /* start (or restart) advertising with this data and scan response (whole AD structures, as on the air), the
     * interval between min_ms and max_ms; adv == NULL: stop advertising. 0 - done */
    int (*advertise)(const uint8_t * adv, size_t adv_length, const uint8_t * response, size_t response_length, uint16_t min_ms,
                     uint16_t max_ms);
    /* an indication of C2 to the connection (length <= its MTU - 3); its confirmation comes as matter_ble_indicated(). 0 - sent */
    int (*indicate)(uint16_t connection, const uint8_t * data, size_t length);
    /* close the connection; matter_ble_disconnected() follows. 0 - done */
    int (*disconnect)(uint16_t connection);
} matter_ble_ops;

/* the firmware's BLE stack is there (ops != NULL) or gone (NULL). The pointer is kept: a static table */
void matter_ble_attach(const matter_ble_ops * ops);

/* --- events of the stack, from any task --------------------------------------------------------------------------- */

void matter_ble_connected(uint16_t connection);
/* reason: the HCI reason (0x13 / 0x08 the peer, 0x16 we closed) */
void matter_ble_disconnected(uint16_t connection, uint8_t reason);
/* the ATT MTU of the connection was exchanged (23 until then) */
void matter_ble_mtu(uint16_t connection, uint16_t mtu);
/* a write of C1 */
void matter_ble_written(uint16_t connection, const uint8_t * data, size_t length);
/* the CCCD of C2: indications on / off */
void matter_ble_subscribed(uint16_t connection, bool subscribed);
/* the confirmation of an indication of C2 (ok == false: it failed or timed out) */
void matter_ble_indicated(uint16_t connection, bool ok);

#ifdef __cplusplus
}
#endif

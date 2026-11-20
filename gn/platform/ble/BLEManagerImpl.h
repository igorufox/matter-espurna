/* BLEManagerImpl of the library, one for every chip (EXTERNAL_BLEMANAGERIMPL_HEADER of chips/*.gni): connectedhomeip's
 * BLE transport on the firmware's BLE stack, through the operations of app/matter_ble.h. No stack of its own: without
 * matter_ble_attach() the CHIPoBLE service is "not supported" and Matter runs on the network only.
 *
 * Derived from connectedhomeip's src/platform/Beken/BLEManagerImpl (Apache-2.0, Project CHIP Authors). */
#pragma once

#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE

#include <ble/Ble.h>
#include <lib/support/BitFlags.h>
#include <platform/internal/BLEManager.h>

#include <app/matter_ble.h>

namespace chip {
namespace DeviceLayer {
namespace Internal {

class BLEManagerImpl final : public BLEManager, private Ble::BleLayer, private Ble::BlePlatformDelegate,
                             private Ble::BleApplicationDelegate
{
    friend BLEManager;

public:
    // the events of matter_ble.h, in the Matter task (matter_ble_* schedule them there)
    void Attach(const matter_ble_ops * ops);
    void OnConnected(uint16_t id);
    void OnDisconnected(uint16_t id, uint8_t reason);
    void OnMtu(uint16_t id, uint16_t mtu);
    void OnSubscribed(uint16_t id, bool subscribed);

private:
    // ===== BLEManager
    CHIP_ERROR _Init();
    void _Shutdown() {}
    bool _IsAdvertisingEnabled() { return mFlags.Has(Flags::kAdvertisingEnabled); }
    CHIP_ERROR _SetAdvertisingEnabled(bool val);
    bool _IsAdvertising() { return mFlags.Has(Flags::kAdvertising); }
    CHIP_ERROR _SetAdvertisingMode(BLEAdvertisingMode mode);
    CHIP_ERROR _GetDeviceName(char * buf, size_t bufSize);
    CHIP_ERROR _SetDeviceName(const char * deviceName);
    uint16_t _NumConnections();
    void _OnPlatformEvent(const ChipDeviceEvent * event);
    Ble::BleLayer * _GetBleLayer() { return this; }

    // ===== BlePlatformDelegate
    CHIP_ERROR SubscribeCharacteristic(BLE_CONNECTION_OBJECT conId, const Ble::ChipBleUUID * svcId,
                                       const Ble::ChipBleUUID * charId) override;
    CHIP_ERROR UnsubscribeCharacteristic(BLE_CONNECTION_OBJECT conId, const Ble::ChipBleUUID * svcId,
                                         const Ble::ChipBleUUID * charId) override;
    CHIP_ERROR CloseConnection(BLE_CONNECTION_OBJECT conId) override;
    uint16_t GetMTU(BLE_CONNECTION_OBJECT conId) const override;
    CHIP_ERROR SendIndication(BLE_CONNECTION_OBJECT conId, const Ble::ChipBleUUID * svcId, const Ble::ChipBleUUID * charId,
                              System::PacketBufferHandle pBuf) override;
    CHIP_ERROR SendWriteRequest(BLE_CONNECTION_OBJECT conId, const Ble::ChipBleUUID * svcId, const Ble::ChipBleUUID * charId,
                                System::PacketBufferHandle pBuf) override;

    // ===== BleApplicationDelegate
    void NotifyChipConnectionClosed(BLE_CONNECTION_OBJECT conId) override;

    friend BLEManager & BLEMgr();
    friend BLEManagerImpl & BLEMgrImpl();
    static BLEManagerImpl sInstance;

    const matter_ble_ops * mOps { nullptr };

    enum class Flags : uint16_t
    {
        kAdvertisingEnabled = 0x0001, // the application wants it (commissioning window open)
        kFastAdvertising    = 0x0002, // the first CHIP_DEVICE_CONFIG_BLE_ADVERTISING_INTERVAL_CHANGE_TIME ms
        kAdvertising        = 0x0004, // the stack advertises
        kRefreshNeeded      = 0x0008, // data or interval changed
        kDeviceNameSet      = 0x0010,
    };

    static constexpr size_t kMaxDeviceNameLength = 16;

    struct Connection
    {
        uint16_t id;
        uint16_t mtu;
        bool used;
        bool subscribed;
    };

    BitFlags<Flags> mFlags;
    CHIPoBLEServiceMode mServiceMode { ConnectivityManager::kCHIPoBLEServiceMode_NotSupported };
    Connection mConnections[BLE_LAYER_NUM_BLE_ENDPOINTS] {};
    char mDeviceName[kMaxDeviceNameLength + 1] {};

    Connection * Find(uint16_t id, bool allocate = false);
    void Release(uint16_t id);

    void DriveBLEState();
    static void DriveBLEState(intptr_t);
    CHIP_ERROR StartAdvertising();
    CHIP_ERROR StopAdvertising();
    static void HandleFastAdvertisingTimeout(System::Layer *, void *);
};

inline BLEManager & BLEMgr()
{
    return BLEManagerImpl::sInstance;
}

inline BLEManagerImpl & BLEMgrImpl()
{
    return BLEManagerImpl::sInstance;
}

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip

#endif // CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE

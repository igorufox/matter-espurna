/* BLEManagerImpl of the library on the firmware's BLE stack (app/matter_ble.h), one for every chip.
 * Derived from connectedhomeip's src/platform/Beken/BLEManagerImpl.cpp (Apache-2.0, Project CHIP Authors): the same
 * state machine, the chip's stack calls replaced by the operations the firmware attaches. */

#include <platform/internal/CHIPDeviceLayerInternal.h>

#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE

#include <ble/Ble.h>
#include <ble/CHIPBleServiceData.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/ConfigurationManager.h>
#include <platform/internal/BLEManager.h>

#include <atomic>
#include <cstdio>
#include <cstring>

using namespace ::chip;
using namespace ::chip::Ble;
using namespace ::chip::DeviceLayer;
using namespace ::chip::DeviceLayer::Internal;
using namespace ::chip::System;

namespace {

// the stack attached before the Matter task runs (the module starts first) is kept here, Init() takes it
std::atomic<const matter_ble_ops *> gOps { nullptr };
std::atomic<bool> gStarted { false };

constexpr uint16_t kDefaultMtu = 23;

// an event of the stack for the Matter task: the connection and one value in the argument of ScheduleWork
intptr_t Pack(uint16_t id, uint16_t value)
{
    return static_cast<intptr_t>((static_cast<uint32_t>(id) << 16) | value);
}

uint16_t Id(intptr_t arg)
{
    return static_cast<uint16_t>(static_cast<uint32_t>(arg) >> 16);
}

uint16_t Value(intptr_t arg)
{
    return static_cast<uint16_t>(static_cast<uint32_t>(arg) & 0xFFFF);
}

void Schedule(AsyncWorkFunct work, intptr_t arg)
{
    if (gStarted.load())
    {
        PlatformMgr().ScheduleWork(work, arg);
    }
}

// the advertising intervals of the CHIP config are in units of 0.625 ms
constexpr uint16_t Ms(uint32_t units)
{
    return static_cast<uint16_t>((units * 5) / 8);
}

} // namespace

BLEManagerImpl BLEManagerImpl::sInstance;

CHIP_ERROR BLEManagerImpl::_Init()
{
    ReturnErrorOnFailure(BleLayer::Init(this, this, &DeviceLayer::SystemLayer()));

    memset(mConnections, 0, sizeof(mConnections));
    mFlags.ClearAll().Set(Flags::kAdvertisingEnabled, CHIP_DEVICE_CONFIG_CHIPOBLE_ENABLE_ADVERTISING_AUTOSTART);
    mFlags.Set(Flags::kFastAdvertising);
    mOps         = gOps.load();
    mServiceMode = mOps ? ConnectivityManager::kCHIPoBLEServiceMode_Enabled : ConnectivityManager::kCHIPoBLEServiceMode_NotSupported;
    gStarted.store(true);

    PlatformMgr().ScheduleWork(DriveBLEState, 0);
    return CHIP_NO_ERROR;
}

void BLEManagerImpl::Attach(const matter_ble_ops * ops)
{
    if (mOps && !ops)
    {
        // the stack is gone: its connections with it
        for (auto & connection : mConnections)
        {
            if (connection.used)
            {
                HandleConnectionError(connection.id, BLE_ERROR_APP_CLOSED_CONNECTION);
                connection = Connection {};
            }
        }
        mFlags.Clear(Flags::kAdvertising);
    }

    mOps         = ops;
    mServiceMode = ops ? ConnectivityManager::kCHIPoBLEServiceMode_Enabled : ConnectivityManager::kCHIPoBLEServiceMode_NotSupported;
    mFlags.Set(Flags::kRefreshNeeded);
    ChipLogProgress(DeviceLayer, "CHIPoBLE %s", ops ? "available" : "not available");
    DriveBLEState();
}

// ===== the application's side

CHIP_ERROR BLEManagerImpl::_SetAdvertisingEnabled(bool val)
{
    // kept without a stack too: the commissioning window opens on the network as well, it must not fail here
    if (mFlags.Has(Flags::kAdvertisingEnabled) != val)
    {
        mFlags.Set(Flags::kAdvertisingEnabled, val);
        if (val)
        {
            mFlags.Set(Flags::kFastAdvertising);
        }
        PlatformMgr().ScheduleWork(DriveBLEState, 0);
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR BLEManagerImpl::_SetAdvertisingMode(BLEAdvertisingMode mode)
{
    switch (mode)
    {
    case BLEAdvertisingMode::kFastAdvertising:
        mFlags.Set(Flags::kFastAdvertising);
        break;
    case BLEAdvertisingMode::kSlowAdvertising:
        mFlags.Clear(Flags::kFastAdvertising);
        break;
    default:
        return CHIP_ERROR_INVALID_ARGUMENT;
    }
    mFlags.Set(Flags::kRefreshNeeded);
    PlatformMgr().ScheduleWork(DriveBLEState, 0);
    return CHIP_NO_ERROR;
}

CHIP_ERROR BLEManagerImpl::_GetDeviceName(char * buf, size_t bufSize)
{
    VerifyOrReturnError(strlen(mDeviceName) < bufSize, CHIP_ERROR_BUFFER_TOO_SMALL);
    strcpy(buf, mDeviceName);
    return CHIP_NO_ERROR;
}

CHIP_ERROR BLEManagerImpl::_SetDeviceName(const char * deviceName)
{
    if (deviceName && deviceName[0])
    {
        VerifyOrReturnError(strlen(deviceName) <= kMaxDeviceNameLength, CHIP_ERROR_INVALID_ARGUMENT);
        strcpy(mDeviceName, deviceName);
        mFlags.Set(Flags::kDeviceNameSet);
    }
    else
    {
        mDeviceName[0] = 0;
        mFlags.Clear(Flags::kDeviceNameSet);
    }
    mFlags.Set(Flags::kRefreshNeeded);
    return CHIP_NO_ERROR;
}

uint16_t BLEManagerImpl::_NumConnections()
{
    uint16_t out = 0;
    for (const auto & connection : mConnections)
    {
        out += connection.used ? 1 : 0;
    }
    return out;
}

void BLEManagerImpl::_OnPlatformEvent(const ChipDeviceEvent * event)
{
    switch (event->Type)
    {
    case DeviceEventType::kCHIPoBLESubscribe:
        OnSubscribed(event->CHIPoBLESubscribe.ConId, true);
        HandleSubscribeReceived(event->CHIPoBLESubscribe.ConId, &CHIP_BLE_SVC_ID, &CHIP_BLE_CHAR_2_UUID);
        {
            ChipDeviceEvent established;
            established.Type = DeviceEventType::kCHIPoBLEConnectionEstablished;
            PlatformMgr().PostEventOrDie(&established);
        }
        break;

    case DeviceEventType::kCHIPoBLEUnsubscribe:
        OnSubscribed(event->CHIPoBLEUnsubscribe.ConId, false);
        HandleUnsubscribeReceived(event->CHIPoBLEUnsubscribe.ConId, &CHIP_BLE_SVC_ID, &CHIP_BLE_CHAR_2_UUID);
        break;

    case DeviceEventType::kCHIPoBLEWriteReceived:
        HandleWriteReceived(event->CHIPoBLEWriteReceived.ConId, &CHIP_BLE_SVC_ID, &CHIP_BLE_CHAR_1_UUID,
                            PacketBufferHandle::Adopt(event->CHIPoBLEWriteReceived.Data));
        break;

    case DeviceEventType::kCHIPoBLEIndicateConfirm:
        HandleIndicationConfirmation(event->CHIPoBLEIndicateConfirm.ConId, &CHIP_BLE_SVC_ID, &CHIP_BLE_CHAR_2_UUID);
        break;

    case DeviceEventType::kCHIPoBLEConnectionError:
        HandleConnectionError(event->CHIPoBLEConnectionError.ConId, event->CHIPoBLEConnectionError.Reason);
        break;

    case DeviceEventType::kServiceProvisioningChange:
    case DeviceEventType::kWiFiConnectivityChange:
        mFlags.Set(Flags::kRefreshNeeded);
        DriveBLEState();
        break;

    default:
        break;
    }
}

// ===== the stack's events, in the Matter task

void BLEManagerImpl::OnConnected(uint16_t id)
{
    Connection * connection = Find(id, true);
    if (connection == nullptr)
    {
        ChipLogError(DeviceLayer, "CHIPoBLE: no room for connection %u", id);
        if (mOps)
        {
            mOps->disconnect(id);
        }
        return;
    }
    ChipLogProgress(DeviceLayer, "CHIPoBLE: connection %u", id);
    // advertising stops while a controller is connected (one connection)
    DriveBLEState();
}

void BLEManagerImpl::OnDisconnected(uint16_t id, uint8_t reason)
{
    if (Find(id) != nullptr)
    {
        Release(id);
        CHIP_ERROR error;
        switch (reason)
        {
        case 0x13:
        case 0x08:
            error = BLE_ERROR_REMOTE_DEVICE_DISCONNECTED;
            break;
        case 0x16:
            error = BLE_ERROR_APP_CLOSED_CONNECTION;
            break;
        default:
            error = BLE_ERROR_CHIPOBLE_PROTOCOL_ABORT;
            break;
        }
        HandleConnectionError(id, error);
    }
    ChipLogProgress(DeviceLayer, "CHIPoBLE: connection %u closed (0x%02x)", id, reason);
    mFlags.Set(Flags::kRefreshNeeded);
    DriveBLEState();
}

void BLEManagerImpl::OnMtu(uint16_t id, uint16_t mtu)
{
    Connection * connection = Find(id);
    if (connection != nullptr)
    {
        connection->mtu = mtu;
    }
}

void BLEManagerImpl::OnSubscribed(uint16_t id, bool subscribed)
{
    Connection * connection = Find(id, subscribed);
    if (connection != nullptr)
    {
        connection->subscribed = subscribed;
    }
}

BLEManagerImpl::Connection * BLEManagerImpl::Find(uint16_t id, bool allocate)
{
    Connection * free = nullptr;
    for (auto & connection : mConnections)
    {
        if (connection.used && (connection.id == id))
        {
            return &connection;
        }
        if (!connection.used && (free == nullptr))
        {
            free = &connection;
        }
    }
    if (allocate && (free != nullptr))
    {
        *free = Connection { id, kDefaultMtu, true, false };
        return free;
    }
    return nullptr;
}

void BLEManagerImpl::Release(uint16_t id)
{
    Connection * connection = Find(id);
    if (connection != nullptr)
    {
        *connection = Connection {};
    }
}

// ===== BlePlatformDelegate

CHIP_ERROR BLEManagerImpl::SubscribeCharacteristic(BLE_CONNECTION_OBJECT, const ChipBleUUID *, const ChipBleUUID *)
{
    return CHIP_ERROR_NOT_IMPLEMENTED; // central role only
}

CHIP_ERROR BLEManagerImpl::UnsubscribeCharacteristic(BLE_CONNECTION_OBJECT, const ChipBleUUID *, const ChipBleUUID *)
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR BLEManagerImpl::SendWriteRequest(BLE_CONNECTION_OBJECT, const ChipBleUUID *, const ChipBleUUID *, PacketBufferHandle)
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR BLEManagerImpl::CloseConnection(BLE_CONNECTION_OBJECT conId)
{
    VerifyOrReturnError(mOps != nullptr, CHIP_ERROR_INCORRECT_STATE);
    ChipLogProgress(DeviceLayer, "CHIPoBLE: closing connection %u", conId);
    return (mOps->disconnect(conId) == 0) ? CHIP_NO_ERROR : BLE_ERROR_APP_CLOSED_CONNECTION;
}

uint16_t BLEManagerImpl::GetMTU(BLE_CONNECTION_OBJECT conId) const
{
    for (const auto & connection : mConnections)
    {
        if (connection.used && (connection.id == conId))
        {
            return connection.mtu ? connection.mtu : kDefaultMtu;
        }
    }
    return 0;
}

CHIP_ERROR BLEManagerImpl::SendIndication(BLE_CONNECTION_OBJECT conId, const ChipBleUUID *, const ChipBleUUID *,
                                          PacketBufferHandle data)
{
    Connection * connection = Find(conId);
    VerifyOrReturnError(mOps != nullptr && connection != nullptr && connection->subscribed, CHIP_ERROR_INVALID_ARGUMENT);
    return (mOps->indicate(conId, data->Start(), data->DataLength()) == 0) ? CHIP_NO_ERROR : BLE_ERROR_GATT_INDICATE_FAILED;
}

void BLEManagerImpl::NotifyChipConnectionClosed(BLE_CONNECTION_OBJECT conId)
{
    // the connection was closed by CHIP (CloseConnection): the stack reports matter_ble_disconnected()
}

// ===== advertising

void BLEManagerImpl::DriveBLEState(intptr_t)
{
    sInstance.DriveBLEState();
}

void BLEManagerImpl::DriveBLEState()
{
    const bool want = (mOps != nullptr) && (mServiceMode == ConnectivityManager::kCHIPoBLEServiceMode_Enabled) &&
        mFlags.Has(Flags::kAdvertisingEnabled) && (_NumConnections() == 0);

    if (want && (!mFlags.Has(Flags::kAdvertising) || mFlags.Has(Flags::kRefreshNeeded)))
    {
        CHIP_ERROR err = StartAdvertising();
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(DeviceLayer, "CHIPoBLE advertising failed: %" CHIP_ERROR_FORMAT, err.Format());
        }
    }
    else if (!want && mFlags.Has(Flags::kAdvertising))
    {
        StopAdvertising();
    }
}

CHIP_ERROR BLEManagerImpl::StartAdvertising()
{
    // the Matter service data (Matter Core spec 5.4.2.5.6): discriminator, VID, PID of the board's identity
    ChipBLEDeviceIdentificationInfo info;
    ReturnErrorOnFailure(ConfigurationMgr().GetBLEDeviceIdentificationInfo(info));

    if (!mFlags.Has(Flags::kDeviceNameSet))
    {
        uint16_t discriminator = 0;
        ReturnErrorOnFailure(GetCommissionableDataProvider()->GetSetupDiscriminator(discriminator));
        snprintf(mDeviceName, sizeof(mDeviceName), "%s%04u", CHIP_DEVICE_CONFIG_BLE_DEVICE_NAME_PREFIX, discriminator);
    }

    uint8_t adv[3 + 4 + sizeof(info)];
    size_t index = 0;
    adv[index++] = 2; // flags: LE general discoverable, no BR/EDR
    adv[index++] = 0x01;
    adv[index++] = 0x06;
    adv[index++] = static_cast<uint8_t>(3 + sizeof(info)); // service data, 16-bit UUID 0xFFF6
    adv[index++] = 0x16;
    adv[index++] = 0xF6;
    adv[index++] = 0xFF;
    memcpy(&adv[index], &info, sizeof(info));
    index += sizeof(info);

    uint8_t response[2 + kMaxDeviceNameLength];
    const size_t name = strlen(mDeviceName);
    response[0]       = static_cast<uint8_t>(1 + name); // complete local name
    response[1]       = 0x09;
    memcpy(&response[2], mDeviceName, name);

    const bool fast    = mFlags.Has(Flags::kFastAdvertising);
    const uint16_t min = fast ? Ms(CHIP_DEVICE_CONFIG_BLE_FAST_ADVERTISING_INTERVAL_MIN) : Ms(CHIP_DEVICE_CONFIG_BLE_SLOW_ADVERTISING_INTERVAL_MIN);
    const uint16_t max = fast ? Ms(CHIP_DEVICE_CONFIG_BLE_FAST_ADVERTISING_INTERVAL_MAX) : Ms(CHIP_DEVICE_CONFIG_BLE_SLOW_ADVERTISING_INTERVAL_MAX);

    VerifyOrReturnError(mOps->advertise(adv, index, response, 2 + name, min, max) == 0, CHIP_ERROR_INTERNAL);

    const bool started = !mFlags.Has(Flags::kAdvertising);
    mFlags.Set(Flags::kAdvertising).Clear(Flags::kRefreshNeeded);
    ChipLogProgress(DeviceLayer, "CHIPoBLE advertising %s (%u..%u ms)", mDeviceName, min, max);

    if (fast)
    {
        // fast for a while, then slow (Matter Core spec 5.4.2.5.3)
        DeviceLayer::SystemLayer().StartTimer(System::Clock::Milliseconds32(CHIP_DEVICE_CONFIG_BLE_ADVERTISING_INTERVAL_CHANGE_TIME),
                                              HandleFastAdvertisingTimeout, nullptr);
    }

    if (started)
    {
        ChipDeviceEvent event;
        event.Type                             = DeviceEventType::kCHIPoBLEAdvertisingChange;
        event.CHIPoBLEAdvertisingChange.Result = kActivity_Started;
        PlatformMgr().PostEventOrDie(&event);
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR BLEManagerImpl::StopAdvertising()
{
    DeviceLayer::SystemLayer().CancelTimer(HandleFastAdvertisingTimeout, nullptr);
    if (mOps)
    {
        mOps->advertise(nullptr, 0, nullptr, 0, 0, 0);
    }
    mFlags.Clear(Flags::kAdvertising).Set(Flags::kFastAdvertising);

    ChipDeviceEvent event;
    event.Type                             = DeviceEventType::kCHIPoBLEAdvertisingChange;
    event.CHIPoBLEAdvertisingChange.Result = kActivity_Stopped;
    PlatformMgr().PostEventOrDie(&event);
    return CHIP_NO_ERROR;
}

void BLEManagerImpl::HandleFastAdvertisingTimeout(System::Layer *, void *)
{
    sInstance.mFlags.Clear(Flags::kFastAdvertising).Set(Flags::kRefreshNeeded);
    sInstance.DriveBLEState();
}

// ===== app/matter_ble.h: the firmware's stack, from any task

extern "C" {

void matter_ble_attach(const matter_ble_ops * ops)
{
    gOps.store(ops);
    Schedule([](intptr_t arg) { BLEMgrImpl().Attach(reinterpret_cast<const matter_ble_ops *>(arg)); },
             reinterpret_cast<intptr_t>(ops));
}

void matter_ble_connected(uint16_t connection)
{
    Schedule([](intptr_t arg) { BLEMgrImpl().OnConnected(Id(arg)); }, Pack(connection, 0));
}

void matter_ble_disconnected(uint16_t connection, uint8_t reason)
{
    Schedule([](intptr_t arg) { BLEMgrImpl().OnDisconnected(Id(arg), static_cast<uint8_t>(Value(arg))); },
             Pack(connection, reason));
}

void matter_ble_mtu(uint16_t connection, uint16_t mtu)
{
    Schedule([](intptr_t arg) { BLEMgrImpl().OnMtu(Id(arg), Value(arg)); }, Pack(connection, mtu));
}

void matter_ble_written(uint16_t connection, const uint8_t * data, size_t length)
{
    if (!gStarted.load())
    {
        return;
    }
    PacketBufferHandle buffer = PacketBufferHandle::NewWithData(data, length);
    if (buffer.IsNull())
    {
        ChipLogError(DeviceLayer, "CHIPoBLE: no buffer for %u bytes", static_cast<unsigned>(length));
        return;
    }
    ChipDeviceEvent event;
    event.Type                        = DeviceEventType::kCHIPoBLEWriteReceived;
    event.CHIPoBLEWriteReceived.ConId = connection;
    event.CHIPoBLEWriteReceived.Data  = std::move(buffer).UnsafeRelease();
    if (PlatformMgr().PostEvent(&event) != CHIP_NO_ERROR)
    {
        PacketBufferHandle::Adopt(event.CHIPoBLEWriteReceived.Data);
    }
}

void matter_ble_subscribed(uint16_t connection, bool subscribed)
{
    if (!gStarted.load())
    {
        return;
    }
    ChipDeviceEvent event;
    event.Type                    = subscribed ? DeviceEventType::kCHIPoBLESubscribe : DeviceEventType::kCHIPoBLEUnsubscribe;
    event.CHIPoBLESubscribe.ConId = connection;
    PlatformMgr().PostEvent(&event);
}

void matter_ble_indicated(uint16_t connection, bool ok)
{
    if (!gStarted.load())
    {
        return;
    }
    ChipDeviceEvent event;
    if (ok)
    {
        event.Type                          = DeviceEventType::kCHIPoBLEIndicateConfirm;
        event.CHIPoBLEIndicateConfirm.ConId = connection;
    }
    else
    {
        event.Type                           = DeviceEventType::kCHIPoBLEConnectionError;
        event.CHIPoBLEConnectionError.ConId  = connection;
        event.CHIPoBLEConnectionError.Reason = BLE_ERROR_GATT_INDICATE_FAILED;
    }
    PlatformMgr().PostEvent(&event);
}

} // extern "C"

#endif // CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE

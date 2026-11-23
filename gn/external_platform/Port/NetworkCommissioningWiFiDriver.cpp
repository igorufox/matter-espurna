/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <lib/support/CodeUtils.h>
#include <lib/support/SafeInt.h>
#include <external_platform/Port/NetworkCommissioningDriver.h>
#include <platform/CHIPDeviceLayer.h>

#include <limits>
#include <lib/support/CHIPMemString.h>
#include <string>

#include "matter_port.h"

#if __has_include(<freertos/FreeRTOS.h>)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#else
#include <FreeRTOS.h>
#include <semphr.h>
#endif

#include <algorithm>

using namespace ::chip;
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
namespace chip {
namespace DeviceLayer {
namespace NetworkCommissioning {

namespace {
constexpr char kWiFiSSIDKeyName[]        = "wifi-ssid";
constexpr char kWiFiCredentialsKeyName[] = "wifi-pass";
} // namespace

CHIP_ERROR PortWiFiDriver::Init(NetworkStatusChangeCallback * networkStatusChangeCallback)
{
    CHIP_ERROR err;
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::Init\r\n");

    err = GetSavedNetWorkConfig(&mSavedNetwork);

    mStagingNetwork        = mSavedNetwork;
    mpScanCallback         = nullptr;
    mpConnectCallback      = nullptr;
    mpStatusChangeCallback = networkStatusChangeCallback;
    return err;
}

CHIP_ERROR PortWiFiDriver::GetSavedNetWorkConfig(WiFiNetwork * WifiNetconf)
{
    CHIP_ERROR err;
    size_t ssidLen        = 0;
    size_t credentialsLen = 0;

    memset(WifiNetconf, 0x0, sizeof(WiFiNetwork));
    // the KVS of this port says PERSISTED_STORAGE_VALUE_NOT_FOUND: no saved network is not an error, or the
    // Network Commissioning cluster fails to init and commissioners abort on its unreadable FeatureMap
    err = PersistedStorage::KeyValueStoreMgr().Get(kWiFiCredentialsKeyName, WifiNetconf->credentials,
                                                   sizeof(WifiNetconf->credentials), &credentialsLen);
    if ((err == CHIP_ERROR_NOT_FOUND) || (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND))
    {
        memset(WifiNetconf, 0x0, sizeof(WiFiNetwork));
        return CHIP_NO_ERROR;
    }

    err = PersistedStorage::KeyValueStoreMgr().Get(kWiFiSSIDKeyName, WifiNetconf->ssid, sizeof(WifiNetconf->ssid), &ssidLen);
    if ((err == CHIP_ERROR_NOT_FOUND) || (err == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND))
    {
        memset(WifiNetconf, 0x0, sizeof(WiFiNetwork));
        return CHIP_NO_ERROR;
    }
    WifiNetconf->credentialsLen = credentialsLen;
    WifiNetconf->ssidLen        = ssidLen;
    return err;
}

void PortWiFiDriver::Shutdown()
{
    mpStatusChangeCallback = nullptr;
}

CHIP_ERROR PortWiFiDriver::CommitConfiguration()
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::CommitConfiguration\r\n");
    ReturnErrorOnFailure(PersistedStorage::KeyValueStoreMgr().Put(kWiFiSSIDKeyName, mStagingNetwork.ssid, mStagingNetwork.ssidLen));
    ReturnErrorOnFailure(PersistedStorage::KeyValueStoreMgr().Put(kWiFiCredentialsKeyName, mStagingNetwork.credentials,
                                                                  mStagingNetwork.credentialsLen));
    mSavedNetwork = mStagingNetwork;
    return CHIP_NO_ERROR;
}

CHIP_ERROR PortWiFiDriver::RevertConfiguration()
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::RevertConfiguration\r\n");
    mStagingNetwork = mSavedNetwork;
    return CHIP_NO_ERROR;
}

bool PortWiFiDriver::NetworkMatch(const WiFiNetwork & network, ByteSpan networkId)
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::NetworkMatch\r\n");
    return networkId.size() == network.ssidLen && memcmp(networkId.data(), network.ssid, network.ssidLen) == 0;
}

Status PortWiFiDriver::AddOrUpdateNetwork(ByteSpan ssid, ByteSpan credentials, MutableCharSpan & outDebugText,
                                           uint8_t & outNetworkIndex)
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::AddOrUpdateNetwork\r\n");

    outDebugText.reduce_size(0);
    outNetworkIndex = 0;
    VerifyOrReturnError(mStagingNetwork.ssidLen == 0 || NetworkMatch(mStagingNetwork, ssid), Status::kBoundsExceeded);
    VerifyOrReturnError(credentials.size() <= sizeof(mStagingNetwork.credentials), Status::kOutOfRange);
    VerifyOrReturnError(ssid.size() <= sizeof(mStagingNetwork.ssid), Status::kOutOfRange);

    memcpy(mStagingNetwork.credentials, credentials.data(), credentials.size());
    mStagingNetwork.credentialsLen = static_cast<decltype(mStagingNetwork.credentialsLen)>(credentials.size());

    memcpy(mStagingNetwork.ssid, ssid.data(), ssid.size());
    mStagingNetwork.ssidLen = static_cast<decltype(mStagingNetwork.ssidLen)>(ssid.size());

    return Status::kSuccess;
}

Status PortWiFiDriver::RemoveNetwork(ByteSpan networkId, MutableCharSpan & outDebugText, uint8_t & outNetworkIndex)
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::RemoveNetwork\r\n");

    outDebugText.reduce_size(0);
    outNetworkIndex = 0;
    VerifyOrReturnError(NetworkMatch(mStagingNetwork, networkId), Status::kNetworkIDNotFound);

    // Use empty ssid for representing invalid network
    mStagingNetwork.ssidLen = 0;
    return Status::kSuccess;
}

Status PortWiFiDriver::ReorderNetwork(ByteSpan networkId, uint8_t index, MutableCharSpan & outDebugText)
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::ReorderNetwork\r\n");
    // Only one network is supported now
    outDebugText.reduce_size(0);
    VerifyOrReturnError(index == 0, Status::kOutOfRange);
    VerifyOrReturnError(NetworkMatch(mStagingNetwork, networkId), Status::kNetworkIDNotFound);
    return Status::kSuccess;
}

CHIP_ERROR PortWiFiDriver::ConnectWiFiNetwork(const char * ssid, uint8_t ssidLen, const char * key, uint8_t keyLen)
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::ConnectWiFiNetwork....ssid:%s", StringOrNullMarker(ssid));
    ReturnErrorOnFailure(ConnectivityMgr().SetWiFiStationMode(ConnectivityManager::kWiFiStationMode_Disabled));

    MATTER_PORT_LOG_ON_ERR(matter_port_wifi_connect(ssid, ssidLen, key, keyLen));

    return ConnectivityMgr().SetWiFiStationMode(ConnectivityManager::kWiFiStationMode_Enabled);
}

void PortWiFiDriver::OnConnectWiFiNetwork()
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::OnConnectWiFiNetwork\r\n");
    if (mpConnectCallback)
    {
        // chip::DeviceLayer::PlatformMgr().LockChipStack();
        mpConnectCallback->OnResult(Status::kSuccess, CharSpan(), 0);
        // chip::DeviceLayer::PlatformMgr().UnlockChipStack();
        mpConnectCallback = nullptr;
    }
}

void PortWiFiDriver::ConnectNetwork(ByteSpan networkId, ConnectCallback * callback)
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::ConnectNetwork\r\n");
    CHIP_ERROR err          = CHIP_NO_ERROR;
    Status networkingStatus = Status::kSuccess;

    VerifyOrExit(NetworkMatch(mStagingNetwork, networkId), networkingStatus = Status::kNetworkIDNotFound);
    VerifyOrExit(mpConnectCallback == nullptr, networkingStatus = Status::kUnknownError);

    err               = ConnectWiFiNetwork(reinterpret_cast<const char *>(mStagingNetwork.ssid), mStagingNetwork.ssidLen,
                                           reinterpret_cast<const char *>(mStagingNetwork.credentials), mStagingNetwork.credentialsLen);
    mpConnectCallback = callback;
exit:
    if (err != CHIP_NO_ERROR)
    {
        networkingStatus = Status::kUnknownError;
    }
    if (networkingStatus != Status::kSuccess)
    {
        ChipLogError(NetworkProvisioning, "Failed to connect to WiFi network:%s", chip::ErrorStr(err));
        mpConnectCallback = nullptr;
        // chip::DeviceLayer::PlatformMgr().LockChipStack();
        callback->OnResult(networkingStatus, CharSpan(), 0);
        // chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    }
}

CHIP_ERROR GetConnectedNetwork(Network & network)
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::GetConnectedNetwork\r\n");
    matter_port_link_t link;
    uint8_t length = 0;
    if (MATTER_PORT_OK == matter_port_wifi_link(&link))
    {
        length = static_cast<uint8_t>(strnlen(link.ssid, DeviceLayer::Internal::kMaxWiFiSSIDLength));
        memcpy(network.networkID, link.ssid, length);
    }
    ChipLogProgress(NetworkProvisioning, "networkID:[%s][%d]\r\n", network.networkID, length);
    network.networkIDLen = length;
    return CHIP_NO_ERROR;
}

void PortWiFiDriver::OnNetworkStatusChange()
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::OnNetworkStatusChange\r\n");
    Network configuredNetwork = { 0 };

    VerifyOrReturn(mpStatusChangeCallback != nullptr);
    GetConnectedNetwork(configuredNetwork);

    if (configuredNetwork.networkIDLen)
    {
        mpStatusChangeCallback->OnNetworkingStatusChange(
            Status::kSuccess, MakeOptional(ByteSpan(configuredNetwork.networkID, configuredNetwork.networkIDLen)), NullOptional);
        return;
    }
    mpStatusChangeCallback->OnNetworkingStatusChange(
        Status::kUnknownError, MakeOptional(ByteSpan(configuredNetwork.networkID, configuredNetwork.networkIDLen)),
        MakeOptional(GetLastDisconnectReason()));
}

CHIP_ERROR PortWiFiDriver::SetLastDisconnectReason(const ChipDeviceEvent * event)
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::SetLastDisconnectReason\r\n");
    mLastDisconnectedReason = event->Platform.PortSystemEvent.Data.WiFiStaDisconnected;
    return CHIP_NO_ERROR;
}

int32_t PortWiFiDriver::GetLastDisconnectReason()
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::GetLastDisconnectReason\r\n");
    return mLastDisconnectedReason;
}

// the scan runs in the firmware, its end comes as an event (FreeRTOS semaphore: the SDKs' FreeRTOS, both chips)
static SemaphoreHandle_t matter_scan_sema = NULL;

int scan_done_handler(void * arg, matter_port_event_module_t module, int event_id, void * event_data)
{
    if (matter_scan_sema)
    {
        xSemaphoreGive(matter_scan_sema);
    }
    return MATTER_PORT_OK;
}

CHIP_ERROR PortWiFiDriver::StartScanWiFiNetworks(ByteSpan ssid)
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::StartScanWiFiNetworks");
    if (ssid.data() == NULL) // non-directed scanning
    {
        return (MATTER_PORT_OK == matter_port_wifi_scan_start(NULL)) ? CHIP_NO_ERROR : CHIP_ERROR_INTERNAL;
    }

    char name[33] = { 0 };
    memcpy(name, ssid.data(), std::min(ssid.size(), sizeof(name) - 1));
    return (MATTER_PORT_OK == matter_port_wifi_scan_start(name)) ? CHIP_NO_ERROR : CHIP_ERROR_INTERNAL;
}

void PortWiFiDriver::OnScanWiFiNetworkDone()
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::OnScanWiFiNetworkDone");
    if (!GetInstance().mpScanCallback)
    {
        return;
    }
    matter_port_scan_ap_t * aps = nullptr;
    size_t count                = 0;
    if ((MATTER_PORT_OK != matter_port_wifi_scan_result(&aps, &count)) || !count)
    {
        GetInstance().mpScanCallback->OnFinished(Status::kNetworkNotFound, CharSpan(), nullptr);
    }
    else
    {
        PortScanResponseIterator iter(count, aps);
        GetInstance().mpScanCallback->OnFinished(Status::kSuccess, CharSpan(), &iter);
    }
    GetInstance().mpScanCallback = nullptr;
    matter_port_wifi_scan_free(aps);
}

void PortWiFiDriver::ScanNetworks(ByteSpan ssid, WiFiDriver::ScanCallback * callback)
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::ScanNetworks");
    if (callback != nullptr)
    {
        mpScanCallback = callback;
        if (matter_scan_sema == NULL)
        {
            matter_scan_sema = xSemaphoreCreateBinary();
        }
        MATTER_PORT_LOG_ON_ERR(
            matter_port_event_register(MATTER_PORT_EVENT_WIFI, MATTER_PORT_EVENT_WIFI_SCAN_DONE, scan_done_handler, NULL));
        if (StartScanWiFiNetworks(ssid) != CHIP_NO_ERROR)
        {
            mpScanCallback = nullptr;
            callback->OnFinished(Status::kUnknownError, CharSpan(), nullptr);
        }
        else
        {
            xSemaphoreTake(matter_scan_sema, pdMS_TO_TICKS(4000));
        }
        MATTER_PORT_LOG_ON_ERR(matter_port_event_unregister(MATTER_PORT_EVENT_WIFI, MATTER_PORT_EVENT_WIFI_SCAN_DONE, scan_done_handler));
        vSemaphoreDelete(matter_scan_sema);
        matter_scan_sema = NULL;
        OnScanWiFiNetworkDone();
    }
}

size_t PortWiFiDriver::WiFiNetworkIterator::Count()
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::WiFiNetworkIterator::Count\r\n");
    return mDriver->mStagingNetwork.ssidLen == 0 ? 0 : 1;
}

bool PortWiFiDriver::WiFiNetworkIterator::Next(Network & item)
{
    ChipLogProgress(NetworkProvisioning, "PortWiFiDriver::WiFiNetworkIterator::Next\r\n");
    if (mExhausted || mDriver->mStagingNetwork.ssidLen == 0)
    {
        return false;
    }
    uint8_t length =
        strnlen(reinterpret_cast<const char *>(mDriver->mStagingNetwork.ssid), DeviceLayer::Internal::kMaxWiFiSSIDLength);
    memcpy(item.networkID, mDriver->mStagingNetwork.ssid, length);
    item.networkIDLen        = length;
    item.connected           = false;
    mExhausted               = true;
    Network connectedNetwork = { 0 };
    CHIP_ERROR err           = GetConnectedNetwork(connectedNetwork);
    if (err == CHIP_NO_ERROR)
    {
        if (connectedNetwork.networkIDLen == item.networkIDLen &&
            memcmp(connectedNetwork.networkID, item.networkID, item.networkIDLen) == 0)
        {
            item.connected = true;
        }
    }
    return true;
}

} // namespace NetworkCommissioning
} // namespace DeviceLayer
} // namespace chip
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI

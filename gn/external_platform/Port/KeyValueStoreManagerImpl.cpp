/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *    All rights reserved.
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

/**
 *    @file
 *          Platform-specific key value storage implementation for Beken
 */
/* this file behaves like a config.h, comes first */
#include "matter_port.h"
#include <platform/KeyValueStoreManager.h>
#include <lib/support/CodeUtils.h>

namespace chip {
namespace DeviceLayer {
namespace PersistedStorage {

KeyValueStoreManagerImpl KeyValueStoreManagerImpl::sInstance;

CHIP_ERROR KeyValueStoreManagerImpl::_Get(const char * key, void * value, size_t value_size, size_t * read_bytes_size,
                                          size_t offset_bytes)
{
    CHIP_ERROR err         = CHIP_NO_ERROR;
    int32_t ret            = -1;
    size_t dwTeReadBytes   = 0;

    if ((!value) || offset_bytes > 0)
    {
        // Offset and partial reads are not supported in nvs, for now just return NOT_IMPLEMENTED. Support can be added in the
        // future if this is needed.
        return (err = CHIP_ERROR_INVALID_ARGUMENT);
    }

    if (read_bytes_size == NULL)
    {
        ret = matter_port_kv_read(GetKVNameSpaceName(key), key, value, value_size, &dwTeReadBytes);
    }
    else
    {
        ret = matter_port_kv_read(GetKVNameSpaceName(key), key, value, value_size, read_bytes_size);
    }
    if (ret == MATTER_PORT_OK)
    {
        err = CHIP_NO_ERROR;
    }
    else if (ret == MATTER_PORT_BUFFER_TOO_SMALL)
    {
        // the KeyValueStoreManager contract: the first value_size bytes are in the buffer
        err = CHIP_ERROR_BUFFER_TOO_SMALL;
    }
    else
    {
        err = CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
    }

    return err;
}

CHIP_ERROR KeyValueStoreManagerImpl::_Put(const char * key, const void * value, size_t value_size)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    uint32_t ret   = 0;

    if (!value)
    {
        return (err = CHIP_ERROR_INVALID_ARGUMENT);
    }

    ret = matter_port_kv_write(GetKVNameSpaceName(key), key, value, value_size);

    if (ret == MATTER_PORT_OK)
    {
        err = CHIP_NO_ERROR;
    }
    else
    {
        err = CHIP_ERROR_INTERNAL;
    }

    return err;
}

CHIP_ERROR KeyValueStoreManagerImpl::_Delete(const char * key)
{
    uint32_t ret   = 0;
    CHIP_ERROR err = CHIP_NO_ERROR;

    matter_port_kv_remove(GetKVNameSpaceName(key), key);

    return CHIP_NO_ERROR;
}

const char * KeyValueStoreManagerImpl::GetKVNameSpaceName(const char * key)
{
    int idx                               = 0;
    const char * BekenkeyValueNameSpace[] = { "BEKEN0", "BEKEN1", "BEKEN2", "BEKEN3",
                                              "BEKEN4" }; // the stored key names (BK7231N boards have them)
    if (key != NULL)
    {
        int i, len = strlen(key);
        int sum = 0;
        for (i = 0; i < len; i += 2)
            sum += key[i];
        idx = sum % (sizeof(BekenkeyValueNameSpace) / sizeof(BekenkeyValueNameSpace[0]));
    }
    return BekenkeyValueNameSpace[idx];
}

} // namespace PersistedStorage
} // namespace DeviceLayer
} // namespace chip

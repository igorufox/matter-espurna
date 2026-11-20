# What connectedhomeip's ESP32 platform includes (src/platform/ESP32): their include folders reach the library;
# chip_config: the options of connectedhomeip's ESP32 component in sdkconfig.h
idf_component_register(SRCS "probe.c" "probe.cpp"
                       REQUIRES app_update bootloader_support chip_config efuse esp_event esp_hw_support esp_netif
                                esp_partition esp_system esp_timer esp_wifi freertos heap log lwip mbedtls newlib nvs_flash soc
                                spi_flash)

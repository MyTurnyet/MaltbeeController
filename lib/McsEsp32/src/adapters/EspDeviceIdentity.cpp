#ifdef ARDUINO

#include "EspDeviceIdentity.h"

#include <esp_mac.h>

#include <array>

MacAddress EspDeviceIdentity::mac() const
{
    std::array<uint8_t, 6> bytes{};
    esp_efuse_mac_get_default(bytes.data());
    return MacAddress(bytes);
}

#endif

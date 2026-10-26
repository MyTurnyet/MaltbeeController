#ifdef ARDUINO

#include "EepromConfigurationRepository.h"

#include <EEPROM.h>

namespace
{
    constexpr int EEPROM_MAGIC_ADDRESS = 0;
    constexpr int EEPROM_VERSION_ADDRESS = EEPROM_MAGIC_ADDRESS + sizeof(uint16_t);
    constexpr int EEPROM_CONFIG_ADDRESS = EEPROM_VERSION_ADDRESS + sizeof(uint8_t);

    constexpr uint16_t SCHEMA_MAGIC = 0xBEEF;
    constexpr uint8_t SCHEMA_VERSION = 1;
}

MegaConfig EepromConfigurationRepository::load()
{
    uint16_t magic = 0;
    uint8_t version = 0;
    EEPROM.get(EEPROM_MAGIC_ADDRESS, magic);
    EEPROM.get(EEPROM_VERSION_ADDRESS, version);

    MegaConfig config{};
    if (magic != SCHEMA_MAGIC || version != SCHEMA_VERSION)
    {
        return config;
    }

    EEPROM.get(EEPROM_CONFIG_ADDRESS, config);
    return config;
}

bool EepromConfigurationRepository::save(const MegaConfig& config)
{
    EEPROM.put(EEPROM_MAGIC_ADDRESS, SCHEMA_MAGIC);
    EEPROM.put(EEPROM_VERSION_ADDRESS, SCHEMA_VERSION);
    EEPROM.put(EEPROM_CONFIG_ADDRESS, config);
    return true;
}

#endif

#pragma once

#include "../ports/ConfigurationRepository.h"

// #ifdef ARDUINO-guarded hardware adapter, same precedent as TurnoutStation
// and MrrwaLocoNetSwitchDriver - uses the AVR EEPROM library directly, no
// native test possible or needed.
class EepromConfigurationRepository final : public ConfigurationRepository
{
public:
    MegaConfig load() override;
    bool save(const MegaConfig& config) override;
};

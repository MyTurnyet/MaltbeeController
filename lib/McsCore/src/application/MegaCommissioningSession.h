#pragma once

#include "../domain/MegaConfig.h"
#include "../ports/ConfigurationRepository.h"

// Minimal, hand-parsed serial commissioning for the Mega panel's 4 turnout
// addresses/names. Deliberately smaller than the ESP32 side's
// CommandLineParser/ParsedCommand/CommissioningSession layering - this
// command set (address/name/save) doesn't need a generic parser pipeline.
// Edits apply to an in-memory draft only; the repository is touched on
// "save" alone, to avoid EEPROM wear from every keystroke.
class MegaCommissioningSession
{
public:
    MegaCommissioningSession(ConfigurationRepository& repository, const MegaConfig& defaults);

    [[nodiscard]] FixedString32 handleLine(const char* line);
    [[nodiscard]] const MegaConfig& draft() const;

private:
    ConfigurationRepository& repository_;
    MegaConfig draft_;
};

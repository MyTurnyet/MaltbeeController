#include "MegaCommissioningSession.h"

#include <cstdio>
#include <cstring>

MegaCommissioningSession::MegaCommissioningSession(ConfigurationRepository& repository, const MegaConfig& defaults)
    : repository_(repository), draft_(repository_.load())
{
    if (!draft_.validate())
    {
        draft_ = defaults;
    }
}

FixedString32 MegaCommissioningSession::handleLine(const char* line)
{
    int station = 0;
    int address = 0;

    if (sscanf(line, "address %d %d", &station, &address) == 2)
    {
        if (station < 1 || station > MegaConfig::kStationCount)
        {
            return FixedString32("ERR: station must be 1-4");
        }
        if (address < MegaConfig::kMinAddress || address > MegaConfig::kMaxAddress)
        {
            return FixedString32("ERR: address out of range");
        }
        draft_.stations[station - 1].address = address;
        return FixedString32("OK");
    }

    char nameBuffer[FixedString32::CAPACITY] = {};
    if (sscanf(line, "name %d %31s", &station, nameBuffer) == 2)
    {
        if (station < 1 || station > MegaConfig::kStationCount)
        {
            return FixedString32("ERR: station must be 1-4");
        }
        draft_.stations[station - 1].name = FixedString32(nameBuffer);
        return FixedString32("OK");
    }

    if (strcmp(line, "save") == 0)
    {
        return repository_.save(draft_) ? FixedString32("OK") : FixedString32("ERR: save failed");
    }

    return FixedString32("ERR: unknown command");
}

const MegaConfig& MegaCommissioningSession::draft() const
{
    return draft_;
}

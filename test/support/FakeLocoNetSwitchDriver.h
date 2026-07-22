#pragma once

#include <vector>

#include "ports/LocoNetSwitchDriver.h"

struct LocoNetSwitchRequest
{
    int address;
    TurnoutPosition position;
    bool outputOn;
};

class FakeLocoNetSwitchDriver final : public LocoNetSwitchDriver
{
public:
    std::vector<LocoNetSwitchRequest> requests;

    void requestSwitch(const int address, const TurnoutPosition position, const bool outputOn) override
    {
        requests.push_back({address, position, outputOn});
    }
};

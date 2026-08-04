#include <Arduino.h>
#include <LocoNet.h>

#include "adapters/ArduinoClock.h"
#include "adapters/LocoNetFeedbackDecoder.h"
#include "adapters/MrrwaLocoNetFeedbackSource.h"
#include "adapters/MrrwaLocoNetSwitchDriver.h"
#include "adapters/MrrwaLocoNetTurnoutAdapter.h"
#include "adapters/PulsingLocoNetTransport.h"
#include "adapters/TurnoutStation.h"

namespace
{
    constexpr int STATION_COUNT = 4;
    constexpr TurnoutConfig STATION_CONFIGS[STATION_COUNT] = {
        {101, "Turnout 1", 22, 23, 8, 9},
        {102, "Turnout 2", 24, 25, 10, 11},
        {103, "Turnout 3", 26, 27, 12, 13},
        {104, "Turnout 4", 28, 29, 14, 15},
    };
    constexpr unsigned long LOCONET_PULSE_DURATION_MS = 250;
}

ArduinoClock clock;

MrrwaLocoNetSwitchDriver locoNetSwitchDriver;
PulsingLocoNetTransport locoNetTransport(locoNetSwitchDriver, clock, LOCONET_PULSE_DURATION_MS);
MrrwaLocoNetTurnoutAdapter turnoutCommandPort(locoNetTransport);

MrrwaLocoNetFeedbackSource locoNetFeedbackSource;
LocoNetFeedbackDecoder locoNetFeedbackDecoder;

TurnoutStation stations[STATION_COUNT] = {
    TurnoutStation(STATION_CONFIGS[0], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[1], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[2], clock, turnoutCommandPort),
    TurnoutStation(STATION_CONFIGS[3], clock, turnoutCommandPort),
};

void setup()
{
    for (auto& station : stations)
    {
        station.begin();
    }

    LocoNet.init();
}

void loop()
{
    for (auto& station : stations)
    {
        station.update();
    }

    locoNetTransport.update();

    SwitchOutputsReport report;
    if (locoNetFeedbackSource.poll(report))
    {
        const TurnoutFeedbackLookup lookup = locoNetFeedbackDecoder.decode(report);
        if (lookup.found)
        {
            for (auto& station : stations)
            {
                station.applyFeedback(lookup.feedback);
            }
        }
    }
}

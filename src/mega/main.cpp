#include <Arduino.h>
#include <LocoNet.h>

#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/LocoNetFeedbackDecoder.h"
#include "adapters/MrrwaLocoNetFeedbackSource.h"
#include "adapters/MrrwaLocoNetSwitchDriver.h"
#include "adapters/MrrwaLocoNetTurnoutAdapter.h"
#include "adapters/PulsingLocoNetTransport.h"
#include "adapters/TurnoutStation.h"
#include "application/RouteExecutionService.h"
#include "domain/Button.h"
#include "domain/Route.h"

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
    constexpr unsigned long ROUTE_COMMAND_SPACING_MS = 300;
    constexpr unsigned long ROUTE_BUTTON_DEBOUNCE_MS = 30;
}

static_assert(ROUTE_COMMAND_SPACING_MS > LOCONET_PULSE_DURATION_MS,
              "route spacing must exceed the LocoNet pulse duration, or route commands will truncate turnout pulses");

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

ArduinoDigitalInput route1ButtonInput(30, /*activeLow=*/true, /*useInternalPullup=*/true);
ArduinoDigitalInput route2ButtonInput(31, /*activeLow=*/true, /*useInternalPullup=*/true);
Button route1Button(route1ButtonInput, clock, ROUTE_BUTTON_DEBOUNCE_MS);
Button route2Button(route2ButtonInput, clock, ROUTE_BUTTON_DEBOUNCE_MS);

// Placeholder routes - replace with real route definitions for your layout.
// Populated in setup() via addTurnout() calls below.
Route route1(1, "Example Route 1");
Route route2(2, "Example Route 2");

RouteExecutionService routeExecutionService(turnoutCommandPort, clock, ROUTE_COMMAND_SPACING_MS);

void setup()
{
    route1.addTurnout(101, TurnoutPosition::Thrown);
    route1.addTurnout(102, TurnoutPosition::Closed);
    route2.addTurnout(103, TurnoutPosition::Closed);
    route2.addTurnout(104, TurnoutPosition::Closed);

    route1ButtonInput.begin();
    route2ButtonInput.begin();

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

    route1Button.update();
    route2Button.update();

    if (route1Button.wasPressed())
    {
        routeExecutionService.start(route1);
    }
    if (route2Button.wasPressed())
    {
        routeExecutionService.start(route2);
    }
    routeExecutionService.update();
}

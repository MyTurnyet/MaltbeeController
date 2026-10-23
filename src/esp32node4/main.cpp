#include <Arduino.h>

#include <array>
#include <string>

#include <nvs_flash.h>

#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ButtonSetupModeTrigger.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/EitherActiveDigitalInput.h"
#include "adapters/EspDeviceIdentity.h"
#include "adapters/EspUartPort.h"
#include "adapters/IndependentLedPairFlashIndicator.h"
#include "adapters/IndependentLedPairStation.h"
#include "adapters/NullTurnoutCommandPort.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ToggleTurnoutStation.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "application/CommissioningSession.h"
#include "domain/BootMode.h"
#include "domain/BootModeSelector.h"
#include "domain/LedPairDriver.h"
#include "domain/MacAddress.h"
#include "domain/NodeConfig.h"
#include "domain/SetupApName.h"

namespace
{
    constexpr unsigned long UART_BAUD_RATE = 115200;
    constexpr unsigned long SETUP_TRIGGER_HOLD_MS = 3000;
    constexpr unsigned long BLINK_INTERVAL_MS = 500;
    constexpr LedPairColor DEFAULT_LED_COLOR = LedPairColor::Red;
}

namespace
{
    struct NvsBootstrap
    {
        NvsBootstrap()
        {
            // ESP-IDF runs global constructors before app_main(), so Arduino's
            // initArduino() has not yet called nvs_flash_init() when the globals
            // below read NVS. Initialize it here (idempotent - initArduino()'s
            // later call returns ESP_OK) so this boot's config is the one
            // commissioning actually saved.
            esp_err_t err = nvs_flash_init();
            if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                nvs_flash_erase();
                nvs_flash_init();
            }
        }
    };
}

namespace
{
    struct TurnoutNodeConfig
    {
        int address;
        const char* name;
        int buttonGpio;
        int redGpio;
        int greenGpio;
    };

    constexpr TurnoutNodeConfig TURNOUT_CONFIGS[4] = {
        {1, "T1", 34, 32, 33},
        {2, "T2", 35, 25, 26},
        {3, "T3", 36, 27, 13},
        {4, "T4", 39, 23, 19},
    };
}

NvsBootstrap nvsBootstrap;

ArduinoClock systemClock;

const MacAddress ownMac = EspDeviceIdentity().mac();

EspUartPort uartPort(UART_BAUD_RATE);
NvsConfigStore4 configStore;
CommissioningSession4 commissioningSession(configStore);
SerialCommissioningAdapter4 serialCommissioningAdapter(uartPort, commissioningSession);
WebFormCommissioningAdapter4 webFormAdapter(commissioningSession);
CaptivePortalServer4 captivePortalServer(webFormAdapter);

NvsSetupModeRequestStore setupModeRequestStore;
NodeConfig4 runningConfig = configStore.load();
const bool wirelessSetupRequested = setupModeRequestStore.consumeRequest();
const BootMode bootMode = BootModeSelector::select(runningConfig, wirelessSetupRequested);
const bool configValid = runningConfig.validate().empty();

ArduinoDigitalInput bootButton(0, /*activeLow=*/true, /*useInternalPullup=*/true);
ArduinoDigitalInput setupButton(16, /*activeLow=*/true, /*useInternalPullup=*/true);
EitherActiveDigitalInput setupTriggerInput(bootButton, setupButton);
ButtonSetupModeTrigger setupTrigger(setupTriggerInput, systemClock, SETUP_TRIGGER_HOLD_MS);

ArduinoDigitalInput buttons[4] = {
    ArduinoDigitalInput(TURNOUT_CONFIGS[0].buttonGpio, /*activeLow=*/true, /*useInternalPullup=*/false),
    ArduinoDigitalInput(TURNOUT_CONFIGS[1].buttonGpio, /*activeLow=*/true, /*useInternalPullup=*/false),
    ArduinoDigitalInput(TURNOUT_CONFIGS[2].buttonGpio, /*activeLow=*/true, /*useInternalPullup=*/false),
    ArduinoDigitalInput(TURNOUT_CONFIGS[3].buttonGpio, /*activeLow=*/true, /*useInternalPullup=*/false),
};

IndependentLedPairStation ledStations[4] = {
    IndependentLedPairStation(IndependentLedPairConfig{.redGpioPin = TURNOUT_CONFIGS[0].redGpio,
                                                       .greenGpioPin = TURNOUT_CONFIGS[0].greenGpio},
                              systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    IndependentLedPairStation(IndependentLedPairConfig{.redGpioPin = TURNOUT_CONFIGS[1].redGpio,
                                                       .greenGpioPin = TURNOUT_CONFIGS[1].greenGpio},
                              systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    IndependentLedPairStation(IndependentLedPairConfig{.redGpioPin = TURNOUT_CONFIGS[2].redGpio,
                                                       .greenGpioPin = TURNOUT_CONFIGS[2].greenGpio},
                              systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    IndependentLedPairStation(IndependentLedPairConfig{.redGpioPin = TURNOUT_CONFIGS[3].redGpio,
                                                       .greenGpioPin = TURNOUT_CONFIGS[3].greenGpio},
                              systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
};

const std::array<IndependentLedPairDriver*, 4> bootModeLedPairs = {
    &ledStations[0].driver(),
    &ledStations[1].driver(),
    &ledStations[2].driver(),
    &ledStations[3].driver(),
};
IndependentLedPairFlashIndicator<4> bootModeIndicator(bootModeLedPairs);

NullTurnoutCommandPort turnoutCommandPort;

ToggleTurnoutStation stations[4] = {
    ToggleTurnoutStation(TURNOUT_CONFIGS[0].address, TURNOUT_CONFIGS[0].name, buttons[0],
                          ledStations[0].green(), ledStations[0].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[1].address, TURNOUT_CONFIGS[1].name, buttons[1],
                          ledStations[1].green(), ledStations[1].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[2].address, TURNOUT_CONFIGS[2].name, buttons[2],
                          ledStations[2].green(), ledStations[2].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[3].address, TURNOUT_CONFIGS[3].name, buttons[3],
                          ledStations[3].green(), ledStations[3].red(), systemClock, turnoutCommandPort),
};

void setup()
{
    uartPort.begin();

    bootButton.begin();
    setupButton.begin();

    for (auto& button : buttons)
    {
        button.begin();
    }

    for (auto& ledStation : ledStations)
    {
        ledStation.begin();
    }

    for (auto& station : stations)
    {
        station.begin();
    }

    bootModeIndicator.show(bootMode);

    if (bootMode == BootMode::WirelessSetup)
    {
        const std::string apName = SetupApName::from(ownMac);
        captivePortalServer.begin(apName);
        uartPort.write("MaltBee node in wireless setup mode. Connect to " + apName + "\n");
        return;
    }

    uartPort.write(configValid ? "MaltBee node ready (configured).\n"
                                : "MaltBee node needs commissioning. Type 'show'.\n");
}

void loop()
{
    bootModeIndicator.update();
    serialCommissioningAdapter.poll();
    if (serialCommissioningAdapter.rebootRequested())
    {
        Serial.flush();
        ESP.restart();
    }

    if (bootMode == BootMode::WirelessSetup)
    {
        captivePortalServer.poll();
        return;
    }

    setupTrigger.update();

    if (setupTrigger.requested())
    {
        const bool stored = setupModeRequestStore.requestOnNextBoot();
        if (stored)
        {
            uartPort.write("Entering wireless setup...\n");
            Serial.flush();
            ESP.restart();
        }
        else
        {
            uartPort.write("Failed to persist wireless setup request; staying in normal mode.\n");
        }
    }

    for (auto& ledStation : ledStations)
    {
        ledStation.update();
    }

    for (auto& station : stations)
    {
        station.update();
    }
}

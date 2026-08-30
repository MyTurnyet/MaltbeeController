#include <Arduino.h>

#include <string>

#include <nvs_flash.h>

#include "adapters/ArduinoClock.h"
#include "adapters/ArduinoDigitalInput.h"
#include "adapters/ArduinoDigitalOutput.h"
#include "adapters/EspUartPort.h"
#include "adapters/JmriFeedbackSource.h"
#include "adapters/JmriTurnoutCommandAdapter.h"
#include "adapters/LedPairStation.h"
#include "adapters/MatrixDigitalInput.h"
#include "adapters/MqttLink.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/ToggleTurnoutStation.h"
#include "adapters/WiFiLink.h"
#include "application/CommissioningSession.h"
#include "domain/LedPairDriver.h"
#include "domain/MatrixScanner.h"
#include "domain/NodeConfig.h"
#include "ports/TurnoutCommandPort.h"

namespace
{
    struct TurnoutPanelConfig
    {
        int address;
        const char* name;
        int matrixRow;
        int matrixColumn;
        int ledGpio;
    };

    constexpr TurnoutPanelConfig TURNOUT_CONFIGS[12] = {
        {1, "T1", 0, 0, 4},    {2, "T2", 0, 1, 13},   {3, "T3", 0, 2, 14},   {4, "T4", 0, 3, 16},
        {5, "T5", 1, 0, 17},   {6, "T6", 1, 1, 22},   {7, "T7", 1, 2, 23},   {8, "T8", 1, 3, 25},
        {9, "T9", 2, 0, 26},   {10, "T10", 2, 1, 27}, {11, "T11", 2, 2, 32}, {12, "T12", 2, 3, 33},
    };

    constexpr int MATRIX_ROW_PINS[MatrixScanner::kRowCount] = {18, 19, 21};
    constexpr int MATRIX_COLUMN_PINS[MatrixScanner::kColumnCount] = {34, 35, 36, 39};

    constexpr unsigned long BLINK_INTERVAL_MS = 500;
    constexpr LedPairColor DEFAULT_LED_COLOR = LedPairColor::Red;
    constexpr unsigned long RETRY_INTERVAL_MS = 5000;
    constexpr unsigned long UART_BAUD_RATE = 115200;
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

NvsBootstrap nvsBootstrap;

ArduinoClock systemClock;

EspUartPort uartPort(UART_BAUD_RATE);
NvsConfigStore configStore;
CommissioningSession commissioningSession(configStore);
SerialCommissioningAdapter serialCommissioningAdapter(uartPort, commissioningSession);

NodeConfig runningConfig = configStore.load();
const bool configValid = runningConfig.validate().empty();

ArduinoDigitalOutput matrixRow0(MATRIX_ROW_PINS[0], true);
ArduinoDigitalOutput matrixRow1(MATRIX_ROW_PINS[1], true);
ArduinoDigitalOutput matrixRow2(MATRIX_ROW_PINS[2], true);

ArduinoDigitalInput matrixCol0(MATRIX_COLUMN_PINS[0], true, false);
ArduinoDigitalInput matrixCol1(MATRIX_COLUMN_PINS[1], true, false);
ArduinoDigitalInput matrixCol2(MATRIX_COLUMN_PINS[2], true, false);
ArduinoDigitalInput matrixCol3(MATRIX_COLUMN_PINS[3], true, false);

MatrixScanner matrixScanner({&matrixRow0, &matrixRow1, &matrixRow2},
                            {&matrixCol0, &matrixCol1, &matrixCol2, &matrixCol3});

LedPairStation ledStations[12] = {
    LedPairStation({TURNOUT_CONFIGS[0].ledGpio}, systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[1].ledGpio}, systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[2].ledGpio}, systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[3].ledGpio}, systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[4].ledGpio}, systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[5].ledGpio}, systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[6].ledGpio}, systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[7].ledGpio}, systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[8].ledGpio}, systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[9].ledGpio}, systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[10].ledGpio}, systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
    LedPairStation({TURNOUT_CONFIGS[11].ledGpio}, systemClock, BLINK_INTERVAL_MS, DEFAULT_LED_COLOR),
};

MatrixDigitalInput matrixButtons[12] = {
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[0].matrixRow, TURNOUT_CONFIGS[0].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[1].matrixRow, TURNOUT_CONFIGS[1].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[2].matrixRow, TURNOUT_CONFIGS[2].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[3].matrixRow, TURNOUT_CONFIGS[3].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[4].matrixRow, TURNOUT_CONFIGS[4].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[5].matrixRow, TURNOUT_CONFIGS[5].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[6].matrixRow, TURNOUT_CONFIGS[6].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[7].matrixRow, TURNOUT_CONFIGS[7].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[8].matrixRow, TURNOUT_CONFIGS[8].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[9].matrixRow, TURNOUT_CONFIGS[9].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[10].matrixRow, TURNOUT_CONFIGS[10].matrixColumn),
    MatrixDigitalInput(matrixScanner, TURNOUT_CONFIGS[11].matrixRow, TURNOUT_CONFIGS[11].matrixColumn),
};

WiFiLink wifiLink(systemClock, RETRY_INTERVAL_MS);

const std::string mqttClientId = "maltbee-esp32-" + std::to_string(runningConfig.nodeId);
const std::string mqttWillTopic = "panel/" + std::to_string(runningConfig.nodeId) + "/status";
const std::string mqttWillMessage = "offline";

MqttLink mqttLink(systemClock, RETRY_INTERVAL_MS, mqttClientId, mqttWillTopic, mqttWillMessage);

JmriTurnoutCommandAdapter turnoutCommandPort(mqttLink, runningConfig.channelJmriNames);
JmriFeedbackSource feedbackSource(mqttLink, runningConfig.channelJmriNames);

ToggleTurnoutStation stations[12] = {
    ToggleTurnoutStation(TURNOUT_CONFIGS[0].address, TURNOUT_CONFIGS[0].name, matrixButtons[0],
                          ledStations[0].green(), ledStations[0].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[1].address, TURNOUT_CONFIGS[1].name, matrixButtons[1],
                          ledStations[1].green(), ledStations[1].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[2].address, TURNOUT_CONFIGS[2].name, matrixButtons[2],
                          ledStations[2].green(), ledStations[2].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[3].address, TURNOUT_CONFIGS[3].name, matrixButtons[3],
                          ledStations[3].green(), ledStations[3].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[4].address, TURNOUT_CONFIGS[4].name, matrixButtons[4],
                          ledStations[4].green(), ledStations[4].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[5].address, TURNOUT_CONFIGS[5].name, matrixButtons[5],
                          ledStations[5].green(), ledStations[5].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[6].address, TURNOUT_CONFIGS[6].name, matrixButtons[6],
                          ledStations[6].green(), ledStations[6].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[7].address, TURNOUT_CONFIGS[7].name, matrixButtons[7],
                          ledStations[7].green(), ledStations[7].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[8].address, TURNOUT_CONFIGS[8].name, matrixButtons[8],
                          ledStations[8].green(), ledStations[8].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[9].address, TURNOUT_CONFIGS[9].name, matrixButtons[9],
                          ledStations[9].green(), ledStations[9].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[10].address, TURNOUT_CONFIGS[10].name, matrixButtons[10],
                          ledStations[10].green(), ledStations[10].red(), systemClock, turnoutCommandPort),
    ToggleTurnoutStation(TURNOUT_CONFIGS[11].address, TURNOUT_CONFIGS[11].name, matrixButtons[11],
                          ledStations[11].green(), ledStations[11].red(), systemClock, turnoutCommandPort),
};

void setup()
{
    uartPort.begin();

    matrixRow0.begin();
    matrixRow1.begin();
    matrixRow2.begin();
    matrixCol0.begin();
    matrixCol1.begin();
    matrixCol2.begin();
    matrixCol3.begin();

    for (auto& ledStation : ledStations)
    {
        ledStation.begin();
    }

    for (auto& station : stations)
    {
        station.begin();
    }

    if (configValid)
    {
        wifiLink.begin(runningConfig.wifiSsid, runningConfig.wifiPassword);
        mqttLink.begin(runningConfig.brokerHost, runningConfig.brokerPort);
    }

    uartPort.write(configValid ? "MaltBee panel ready (configured).\n"
                                : "MaltBee panel needs commissioning. Type 'show'.\n");
}

void loop()
{
    matrixScanner.update();

    serialCommissioningAdapter.poll();
    if (serialCommissioningAdapter.rebootRequested())
    {
        Serial.flush();
        ESP.restart();
    }

    if (configValid)
    {
        wifiLink.poll();
        if (wifiLink.connected())
        {
            mqttLink.poll();
        }
    }

    if (configValid && mqttLink.connected())
    {
        TurnoutFeedback feedback{};
        while (feedbackSource.poll(feedback))
        {
            for (auto& station : stations)
            {
                station.applyFeedback(feedback);
            }
        }
    }
    else
    {
        for (auto& station : stations)
        {
            station.clearIndicator();
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

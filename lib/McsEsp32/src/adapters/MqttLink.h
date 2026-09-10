#pragma once

#ifdef ARDUINO

#include <WiFiClient.h>
#include <PubSubClient.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ports/Clock.h"
#include "../application/BrokerAddressResolver.h"
#include "../ports/MqttTransport.h"

class MqttLink final : public MqttTransport
{
public:
    MqttLink(Clock& clock, unsigned long retryIntervalMs, std::string clientId,
              std::string willTopic, std::string willMessage, BrokerAddressResolver& brokerAddressResolver);

    void begin(const std::string& host, int port);
    void poll();
    bool connected();

    void publish(const std::string& topic, const std::string& payload, bool retained) override;
    void subscribe(const std::string& topic, std::function<void(const std::string&)> handler) override;

private:
    void connect();
    void dispatch(const std::string& topic, const std::string& payload);

    Clock& clock_;
    unsigned long retryIntervalMs_;
    std::string clientId_;
    std::string willTopic_;
    std::string willMessage_;
    BrokerAddressResolver& brokerAddressResolver_;
    std::string fallbackHost_;
    int port_ = 0;
    WiFiClient wifiClient_;
    PubSubClient client_;
    unsigned long lastAttemptMs_ = 0;
    std::vector<std::pair<std::string, std::function<void(const std::string&)>>> handlers_;
};

#endif

#pragma once

#include <functional>
#include <string>

class MqttTransport
{
public:
    virtual ~MqttTransport() = default;

    virtual void publish(const std::string& topic, const std::string& payload, bool retained) = 0;
    virtual void subscribe(const std::string& topic, std::function<void(const std::string&)> handler) = 0;
};

#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ports/MqttTransport.h"

struct PublishedMessage
{
    std::string topic;
    std::string payload;
    bool retained;
};

class FakeMqttTransport final : public MqttTransport
{
public:
    std::vector<PublishedMessage> published;
    std::vector<std::string> subscribedTopics;

    void publish(const std::string& topic, const std::string& payload, const bool retained) override
    {
        published.push_back({topic, payload, retained});
    }

    void subscribe(const std::string& topic, std::function<void(const std::string&)> handler) override
    {
        subscribedTopics.push_back(topic);
        handlers_.emplace_back(topic, std::move(handler));
    }

    // Test helper: simulate an incoming message on `topic`, invoking
    // whichever handler(s) subscribed to it. No-op if nothing is
    // subscribed there.
    void deliver(const std::string& topic, const std::string& payload)
    {
        for (const auto& entry : handlers_)
        {
            if (entry.first == topic)
            {
                entry.second(payload);
            }
        }
    }

private:
    std::vector<std::pair<std::string, std::function<void(const std::string&)>>> handlers_;
};

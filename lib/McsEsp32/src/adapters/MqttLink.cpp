#ifdef ARDUINO

#include "MqttLink.h"

MqttLink::MqttLink(Clock& clock, const unsigned long retryIntervalMs, std::string clientId,
                    std::string willTopic, std::string willMessage)
    : clock_(clock),
      retryIntervalMs_(retryIntervalMs),
      clientId_(std::move(clientId)),
      willTopic_(std::move(willTopic)),
      willMessage_(std::move(willMessage)),
      client_(wifiClient_)
{
    client_.setCallback([this](char* topic, byte* payload, unsigned int length) {
        dispatch(topic, std::string(reinterpret_cast<char*>(payload), length));
    });
    client_.setSocketTimeout(2);
}

void MqttLink::begin(const std::string& host, const int port)
{
    client_.setServer(host.c_str(), port);
    connect();
}

void MqttLink::poll()
{
    if (client_.connected())
    {
        client_.loop();
        return;
    }

    if (clock_.nowMilliseconds() - lastAttemptMs_ >= retryIntervalMs_)
    {
        connect();
    }
}

bool MqttLink::connected()
{
    return client_.connected();
}

void MqttLink::publish(const std::string& topic, const std::string& payload, const bool retained)
{
    client_.publish(topic.c_str(), payload.c_str(), retained);
}

void MqttLink::subscribe(const std::string& topic, std::function<void(const std::string&)> handler)
{
    handlers_.emplace_back(topic, std::move(handler));
    client_.subscribe(topic.c_str());
}

void MqttLink::dispatch(const std::string& topic, const std::string& payload)
{
    for (const auto& entry : handlers_)
    {
        if (entry.first == topic)
        {
            entry.second(payload);
        }
    }
}

void MqttLink::connect()
{
    client_.connect(clientId_.c_str(), willTopic_.c_str(), 1, true, willMessage_.c_str());
    lastAttemptMs_ = clock_.nowMilliseconds();

    // PubSubClient forgets subscriptions across a dropped session - replay
    // every topic this link has ever subscribed to on every successful
    // (re)connect.
    if (client_.connected())
    {
        for (const auto& entry : handlers_)
        {
            client_.subscribe(entry.first.c_str());
        }
    }
}

#endif

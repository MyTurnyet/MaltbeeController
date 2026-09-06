#pragma once

#include <string>

class TopicScheme
{
public:
    static std::string topicFor(int address)
    {
        return "loconet/turnout/" + std::to_string(address) + "/set";
    }

    static std::string stateTopicFor(int address)
    {
        return "loconet/turnout/" + std::to_string(address) + "/state";
    }
};

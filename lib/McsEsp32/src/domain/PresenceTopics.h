#pragma once

#include <string>

class PresenceTopics
{
public:
    static std::string statusTopic(int nodeId)
    {
        return "panel/" + std::to_string(nodeId) + "/status";
    }

    static std::string macTopic(int nodeId)
    {
        return "panel/" + std::to_string(nodeId) + "/mac";
    }
};

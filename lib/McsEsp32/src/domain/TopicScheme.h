#pragma once

#include <string>

class TopicScheme
{
public:
    static std::string topicFor(const std::string& jmriName)
    {
        return "track/turnout/" + jmriName;
    }
};

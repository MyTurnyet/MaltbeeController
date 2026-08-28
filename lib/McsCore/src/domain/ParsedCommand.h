#pragma once

#include <string>

enum class CommandKind
{
    Id,
    Wifi,
    Broker,
    TurnoutName,
    Show,
    Save,
    Reboot,
    Invalid
};

struct ParsedCommand
{
    CommandKind kind = CommandKind::Invalid;
    int intArg = 0;
    std::string stringArg1;
    std::string stringArg2;
    int intArg2 = 0;
    std::string errorMessage;
};

#pragma once

#include "ParsedCommand.h"

class CommandLineParser
{
public:
    static ParsedCommand parse(const std::string& line);
};

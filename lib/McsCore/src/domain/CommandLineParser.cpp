#if !defined(__AVR__)

#include "CommandLineParser.h"

#include <sstream>
#include <vector>

namespace
{
    ParsedCommand invalid(const std::string& message)
    {
        ParsedCommand command;
        command.kind = CommandKind::Invalid;
        command.errorMessage = message;
        return command;
    }

    std::vector<std::string> tokenize(const std::string& line)
    {
        std::vector<std::string> tokens;
        std::istringstream stream(line);
        std::string token;
        while (stream >> token)
        {
            tokens.push_back(token);
        }
        return tokens;
    }

    bool parseInt(const std::string& text, int& out)
    {
        if (text.empty())
        {
            return false;
        }
        try
        {
            size_t consumed = 0;
            const int value = std::stoi(text, &consumed);
            if (consumed != text.size())
            {
                return false;
            }
            out = value;
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
}

ParsedCommand CommandLineParser::parse(const std::string& line)
{
    const std::vector<std::string> tokens = tokenize(line);

    if (tokens.empty())
    {
        return invalid("empty command");
    }

    const std::string& verb = tokens[0];

    if (verb == "id")
    {
        if (tokens.size() != 2)
        {
            return invalid("usage: id <n>");
        }
        int id = 0;
        if (!parseInt(tokens[1], id))
        {
            return invalid("id must be a number");
        }
        ParsedCommand command;
        command.kind = CommandKind::Id;
        command.intArg = id;
        return command;
    }

    if (verb == "wifi")
    {
        if (tokens.size() != 3)
        {
            return invalid("usage: wifi <ssid> <password> (no spaces in either)");
        }
        ParsedCommand command;
        command.kind = CommandKind::Wifi;
        command.stringArg1 = tokens[1];
        command.stringArg2 = tokens[2];
        return command;
    }

    if (verb == "broker")
    {
        if (tokens.size() != 3)
        {
            return invalid("usage: broker <host> <port>");
        }
        int port = 0;
        if (!parseInt(tokens[2], port))
        {
            return invalid("broker port must be a number");
        }
        ParsedCommand command;
        command.kind = CommandKind::Broker;
        command.stringArg1 = tokens[1];
        command.intArg2 = port;
        return command;
    }

    if (verb == "turnout")
    {
        if (tokens.size() != 4 || tokens[2] != "name")
        {
            return invalid("usage: turnout <n> name <jmriSystemName>");
        }
        int channel = 0;
        if (!parseInt(tokens[1], channel))
        {
            return invalid("turnout channel must be a number");
        }
        ParsedCommand command;
        command.kind = CommandKind::TurnoutName;
        command.intArg = channel;
        command.stringArg1 = tokens[3];
        return command;
    }

    if (verb == "show")
    {
        if (tokens.size() != 1)
        {
            return invalid("usage: show");
        }
        ParsedCommand command;
        command.kind = CommandKind::Show;
        return command;
    }

    if (verb == "save")
    {
        if (tokens.size() != 1)
        {
            return invalid("usage: save");
        }
        ParsedCommand command;
        command.kind = CommandKind::Save;
        return command;
    }

    if (verb == "reboot")
    {
        if (tokens.size() != 1)
        {
            return invalid("usage: reboot");
        }
        ParsedCommand command;
        command.kind = CommandKind::Reboot;
        return command;
    }

    return invalid("unknown command: " + verb);
}

#endif

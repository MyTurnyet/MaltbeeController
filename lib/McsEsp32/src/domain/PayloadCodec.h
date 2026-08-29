#pragma once

#include <optional>
#include <string>

#include "domain/Turnout.h"

class PayloadCodec
{
public:
    static std::string encode(TurnoutPosition position)
    {
        return position == TurnoutPosition::Closed ? "CLOSED" : "THROWN";
    }

    static std::optional<TurnoutPosition> decode(const std::string& payload)
    {
        if (payload == "CLOSED")
        {
            return TurnoutPosition::Closed;
        }
        if (payload == "THROWN")
        {
            return TurnoutPosition::Thrown;
        }
        return std::nullopt;
    }
};

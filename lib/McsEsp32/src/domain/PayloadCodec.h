#pragma once

#include <string>

#include "domain/Turnout.h"

struct TurnoutPositionLookup
{
    bool found;
    TurnoutPosition position;
};

class PayloadCodec
{
public:
    static std::string encode(TurnoutPosition position)
    {
        return position == TurnoutPosition::Closed ? "CLOSED" : "THROWN";
    }

    static TurnoutPositionLookup decode(const std::string& payload)
    {
        if (payload == "CLOSED")
        {
            return {true, TurnoutPosition::Closed};
        }
        if (payload == "THROWN")
        {
            return {true, TurnoutPosition::Thrown};
        }
        return {false, TurnoutPosition::Closed};
    }
};

#pragma once
#include "FixedString32.h"

enum class TurnoutPosition {
    Closed,
    Thrown
};


class Turnout {
public:
    Turnout();

    Turnout(int address, const char* name, TurnoutPosition turnout_position, bool turnout_locked,
            bool turnout_disabled);


    void throwStraight();

    void throwDiverging();

    [[nodiscard]] TurnoutPosition position() const;

    FixedString32 name() const;

    [[nodiscard]] int address() const;

    bool isDisabled() const;

    [[nodiscard]] bool isLocked() const;

    void toggle();

    bool canThrow() const;

    void lock();

    void unlock();

    void disable();

    void enable();

private:
    int layoutAddress;
    FixedString32 displayName;
    TurnoutPosition currentPosition;
    bool turnoutDisabled;
    bool turnoutLocked;
};

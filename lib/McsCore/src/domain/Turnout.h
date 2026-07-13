#pragma once
#include <string>

enum class TurnoutPosition {
    Closed,
    Thrown
};


class Turnout {
public:
    Turnout(int address, std::string name, TurnoutPosition turnout_position, bool turnout_locked,
            bool turnout_disabled);


    void throwStraight();

    void throwDiverging();

    [[nodiscard]] TurnoutPosition position() const;

    std::string name() const;

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
    std::string displayName;
    TurnoutPosition currentPosition;
    bool turnoutDisabled;
    bool turnoutLocked;
};

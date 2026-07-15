#ifndef TURNOUTCOLLECTION_H
#define TURNOUTCOLLECTION_H

#include "Turnout.h"

class TurnoutCollection {
private:
    static constexpr int MAX_TURNOUTS = 64;

    Turnout turnouts_[MAX_TURNOUTS] = {};
    int count_ = 0;

public:
    TurnoutCollection() = default;

    void add(const Turnout& turnout);
    int count() const;
    const Turnout* getByAddress(int address) const;
    Turnout* getByAddress(int address);
};

#endif // TURNOUTCOLLECTION_H

#ifndef TURNOUTCOLLECTION_H
#define TURNOUTCOLLECTION_H

#include <map>
#include "Turnout.h"

class TurnoutCollection {
private:
    std::map<int, Turnout> turnouts;

public:
    TurnoutCollection() = default;

    void add(const Turnout& turnout);
    int count() const;
    const Turnout* getByAddress(int address) const;
    Turnout* getByAddress(int address);
};

#endif // TURNOUTCOLLECTION_H

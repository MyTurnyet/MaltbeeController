#pragma once

#include <string>

#include "../domain/CommissioningSession.h"
#include "../ports/UartPort.h"

class SerialCommissioningAdapter
{
public:
    SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session);

    void poll();

    [[nodiscard]] bool rebootRequested() const;

private:
    UartPort& uart_;
    CommissioningSession& session_;
    std::string lineBuffer_;
};

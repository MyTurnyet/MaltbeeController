#pragma once

#include <cstddef>
#include <string>

#include "../application/CommissioningSession.h"
#include "../ports/UartPort.h"

class SerialCommissioningAdapter
{
public:
    static constexpr size_t kMaxLineLength = 128;

    SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session);

    void poll();

    [[nodiscard]] bool rebootRequested() const;

private:
    UartPort& uart_;
    CommissioningSession& session_;
    std::string lineBuffer_;
};

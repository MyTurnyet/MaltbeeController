#pragma once

#include "ports/Clock.h"

class FakeClock final : public Clock
{
public:
    [[nodiscard]] unsigned long nowMilliseconds() const override
    {
        return now_;
    }

    void advanceBy(const unsigned long milliseconds)
    {
        now_ += milliseconds;
    }

    void setTo(const unsigned long milliseconds)
    {
        now_ = milliseconds;
    }

private:
    unsigned long now_ = 0;
};

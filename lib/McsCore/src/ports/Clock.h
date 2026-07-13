#pragma once

class Clock
{
public:
    virtual ~Clock() = default;

    virtual unsigned long nowMilliseconds() const = 0;
};

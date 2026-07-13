#pragma once

class DigitalInput
{
public:
    virtual ~DigitalInput() = default;

    virtual bool isActive() const = 0;
};

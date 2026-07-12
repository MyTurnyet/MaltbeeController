#pragma once

class DigitalOutput
{
public:
    virtual ~DigitalOutput() = default;

    virtual void set(bool active) = 0;
    virtual bool isSet() const = 0;
};
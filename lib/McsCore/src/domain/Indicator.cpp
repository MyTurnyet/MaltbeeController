#include "Indicator.h"

Indicator::Indicator(DigitalOutput& output)
    : output_(output)
{
}

void Indicator::on()
{
    output_.set(true);
}

void Indicator::off()
{
    output_.set(false);
}

void Indicator::set(const bool active)
{
    output_.set(active);
}

bool Indicator::isOn() const
{
    return output_.isSet();
}
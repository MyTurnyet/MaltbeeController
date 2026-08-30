#include "GatedDigitalInput.h"

GatedDigitalInput::GatedDigitalInput(DigitalInput& inner) : inner_(inner)
{
}

void GatedDigitalInput::setSuppressed(const bool suppressed)
{
    suppressed_ = suppressed;
}

bool GatedDigitalInput::isActive() const
{
    return !suppressed_ && inner_.isActive();
}

#include "EitherActiveDigitalInput.h"

EitherActiveDigitalInput::EitherActiveDigitalInput(DigitalInput& first, DigitalInput& second)
    : first_(first), second_(second)
{
}

bool EitherActiveDigitalInput::isActive() const
{
    return first_.isActive() || second_.isActive();
}

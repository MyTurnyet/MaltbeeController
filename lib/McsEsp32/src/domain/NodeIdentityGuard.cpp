#include "NodeIdentityGuard.h"

NodeIdentityGuard::NodeIdentityGuard(std::string ownMac) : ownMac_(std::move(ownMac))
{
}

void NodeIdentityGuard::onMacObserved(const std::string& observedMac)
{
    if (observedMac != ownMac_)
    {
        collisionDetected_ = true;
        lastForeignMac_ = observedMac;
    }
}

bool NodeIdentityGuard::collisionDetected() const
{
    return collisionDetected_;
}

const std::string& NodeIdentityGuard::observedMac() const
{
    return lastForeignMac_;
}

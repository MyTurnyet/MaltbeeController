#include "NodeIdentityGuard.h"

NodeIdentityGuard::NodeIdentityGuard(std::string ownMac) : ownMac_(std::move(ownMac))
{
}

void NodeIdentityGuard::onMacObserved(const std::string& observedMac)
{
    if (observedMac != ownMac_)
    {
        collisionDetected_ = true;
    }
}

bool NodeIdentityGuard::collisionDetected() const
{
    return collisionDetected_;
}

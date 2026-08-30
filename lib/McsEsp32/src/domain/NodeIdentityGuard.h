#pragma once

#include <string>

class NodeIdentityGuard
{
public:
    explicit NodeIdentityGuard(std::string ownMac);

    void onMacObserved(const std::string& observedMac);
    [[nodiscard]] bool collisionDetected() const;
    [[nodiscard]] const std::string& observedMac() const;

private:
    std::string ownMac_;
    bool collisionDetected_ = false;
    std::string lastForeignMac_;
};

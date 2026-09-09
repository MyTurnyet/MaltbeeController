#pragma once

#include "../ports/BootModeIndicator.h"
#include "../domain/LedPairDriver.h"

#include <array>
#include <cstddef>

// WirelessSetup: 150ms identify-flash on every pair.
// NeedsCommissioning: 500ms unconfirmed blink on every pair (release both
// colors so LedPairDriver's existing blink path runs). Distinct from
// WirelessSetup; closes the previous "no visual" gap for this mode.
// Normal: identifying off; later station ticks own the pairs.
// update() is a no-op — LedPairStation::update() already ticks the flash.
template <std::size_t N>
class LedPairFlashIndicator : public BootModeIndicator
{
public:
    explicit LedPairFlashIndicator(const std::array<LedPairDriver*, N>& pairs) : pairs_(pairs)
    {
    }

    void show(BootMode mode) override
    {
        applyIdentifying(mode == BootMode::WirelessSetup);
        if (mode == BootMode::NeedsCommissioning)
        {
            releaseAllPairs();
        }
    }

    void update() override
    {
    }

private:
    void applyIdentifying(bool identifying)
    {
        for (LedPairDriver* pair : pairs_)
        {
            pair->setIdentifying(identifying);
        }
    }

    void releaseAllPairs()
    {
        for (LedPairDriver* pair : pairs_)
        {
            pair->setGreen(false);
            pair->setRed(false);
        }
    }

    std::array<LedPairDriver*, N> pairs_;
};

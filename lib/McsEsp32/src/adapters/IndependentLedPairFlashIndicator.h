#pragma once

#include "../ports/BootModeIndicator.h"
#include "../domain/IndependentLedPairDriver.h"

#include <array>
#include <cstddef>

// WirelessSetup: 150ms identify-flash on every pair (IndependentLedPairDriver's
// own setIdentifying() override).
// NeedsCommissioning: 500ms unconfirmed blink on every pair (release both
// colors so IndependentLedPairDriver's existing blink path runs). Distinct
// from WirelessSetup.
// Normal: identifying off; later station ticks own the pairs.
// update() is a no-op — IndependentLedPairStation::update() already ticks
// the flash/blink timing every loop().
//
// Near-exact mirror of LedPairFlashIndicator<N>
// (lib/McsEsp32/src/adapters/LedPairFlashIndicator.h), templated against
// IndependentLedPairDriver* instead of LedPairDriver* — see that class and
// CLAUDE.md's "Independent-output turnout indicator" section for why this
// project keeps independent-output logic in its own, separately-maintained
// copy rather than sharing it with the shared-GPIO driver's equivalents.
template <std::size_t N>
class IndependentLedPairFlashIndicator : public BootModeIndicator
{
public:
    explicit IndependentLedPairFlashIndicator(const std::array<IndependentLedPairDriver*, N>& pairs)
        : pairs_(pairs)
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
        for (IndependentLedPairDriver* pair : pairs_)
        {
            pair->setIdentifying(identifying);
        }
    }

    void releaseAllPairs()
    {
        for (IndependentLedPairDriver* pair : pairs_)
        {
            pair->setGreen(false);
            pair->setRed(false);
        }
    }

    std::array<IndependentLedPairDriver*, N> pairs_;
};

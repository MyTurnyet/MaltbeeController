#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/LocoNetFeedbackDecoder.h"

TEST_CASE("Closed output report decodes to found feedback with Closed position")
{
    LocoNetFeedbackDecoder decoder;

    const TurnoutFeedbackLookup lookup = decoder.decode({101, true, false});

    REQUIRE(lookup.found);
    REQUIRE(lookup.feedback.address == 101);
    REQUIRE(lookup.feedback.position == TurnoutPosition::Closed);
}

TEST_CASE("Thrown output report decodes to found feedback with Thrown position")
{
    LocoNetFeedbackDecoder decoder;

    const TurnoutFeedbackLookup lookup = decoder.decode({202, false, true});

    REQUIRE(lookup.found);
    REQUIRE(lookup.feedback.address == 202);
    REQUIRE(lookup.feedback.position == TurnoutPosition::Thrown);
}

TEST_CASE("Report with neither output asserted decodes to not found")
{
    LocoNetFeedbackDecoder decoder;

    const TurnoutFeedbackLookup lookup = decoder.decode({101, false, false});

    REQUIRE_FALSE(lookup.found);
}

TEST_CASE("Decoding multiple reports in sequence returns independent correct results")
{
    LocoNetFeedbackDecoder decoder;

    const TurnoutFeedbackLookup first = decoder.decode({101, true, false});
    const TurnoutFeedbackLookup second = decoder.decode({202, false, true});

    REQUIRE(first.found);
    REQUIRE(first.feedback.address == 101);
    REQUIRE(first.feedback.position == TurnoutPosition::Closed);
    REQUIRE(second.found);
    REQUIRE(second.feedback.address == 202);
    REQUIRE(second.feedback.position == TurnoutPosition::Thrown);
}

TEST_CASE("Report with both outputs asserted decodes to not found")
{
    LocoNetFeedbackDecoder decoder;

    const TurnoutFeedbackLookup lookup = decoder.decode({101, true, true});

    REQUIRE_FALSE(lookup.found);
}

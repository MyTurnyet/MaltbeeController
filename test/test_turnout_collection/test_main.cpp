#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutCollection.h"
#include "domain/Turnout.h"

TEST_CASE("TurnoutCollection can construct", "[TurnoutCollection]") {
    TurnoutCollection collection;
    REQUIRE(collection.count() == 0);
}

TEST_CASE("TurnoutCollection can add turnouts", "[TurnoutCollection]") {
    TurnoutCollection collection;
    Turnout turnout(101, "Main Yard", TurnoutPosition::Closed, false, false);

    collection.add(turnout);

    REQUIRE(collection.count() == 1);
}

TEST_CASE("TurnoutCollection can add multiple turnouts", "[TurnoutCollection]") {
    TurnoutCollection collection;
    Turnout turnout1(101, "Main Yard", TurnoutPosition::Closed, false, false);
    Turnout turnout2(102, "East Siding", TurnoutPosition::Closed, false, false);
    Turnout turnout3(103, "West Junction", TurnoutPosition::Closed, false, false);

    collection.add(turnout1);
    collection.add(turnout2);
    collection.add(turnout3);

    REQUIRE(collection.count() == 3);
}

TEST_CASE("TurnoutCollection can get turnout count", "[TurnoutCollection]") {
    TurnoutCollection collection;

    REQUIRE(collection.count() == 0);

    collection.add(Turnout(1, "Test", TurnoutPosition::Closed, false, false));
    REQUIRE(collection.count() == 1);

    collection.add(Turnout(2, "Test2", TurnoutPosition::Closed, false, false));
    REQUIRE(collection.count() == 2);
}

TEST_CASE("TurnoutCollection can get turnout by address", "[TurnoutCollection]") {
    TurnoutCollection collection;
    Turnout turnout1(101, "Main Yard", TurnoutPosition::Closed, false, false);
    Turnout turnout2(102, "East Siding", TurnoutPosition::Closed, false, false);

    collection.add(turnout1);
    collection.add(turnout2);

    const Turnout* found = collection.getByAddress(101);

    REQUIRE(found != nullptr);
    REQUIRE(found->address() == 101);
    REQUIRE(found->name() == "Main Yard");
}

TEST_CASE("TurnoutCollection returns nullptr for invalid address", "[TurnoutCollection]") {
    TurnoutCollection collection;
    Turnout turnout(101, "Main Yard", TurnoutPosition::Closed, false, false);
    collection.add(turnout);

    const Turnout* found = collection.getByAddress(999);

    REQUIRE(found == nullptr);
}

TEST_CASE("TurnoutCollection returns nullptr when empty", "[TurnoutCollection]") {
    TurnoutCollection collection;

    const Turnout* found = collection.getByAddress(101);

    REQUIRE(found == nullptr);
}

TEST_CASE("TurnoutCollection can handle duplicate addresses", "[TurnoutCollection]") {
    TurnoutCollection collection;
    Turnout turnout1(101, "First", TurnoutPosition::Closed, false, false);
    Turnout turnout2(101, "Second", TurnoutPosition::Closed, false, false);

    collection.add(turnout1);
    collection.add(turnout2);

    // Should overwrite the first turnout with the second
    REQUIRE(collection.count() == 1);

    const Turnout* found = collection.getByAddress(101);
    REQUIRE(found != nullptr);
    REQUIRE(found->name() == "Second");
}

TEST_CASE("TurnoutCollection ignores a new turnout added beyond capacity", "[TurnoutCollection]") {
    TurnoutCollection collection;

    for (int address = 1; address <= 64; ++address) {
        collection.add(Turnout(address, "Existing", TurnoutPosition::Closed, false, false));
    }

    REQUIRE(collection.count() == 64);

    collection.add(Turnout(65, "Overflow", TurnoutPosition::Closed, false, false));

    REQUIRE(collection.count() == 64);
    REQUIRE(collection.getByAddress(65) == nullptr);

    const Turnout* stillThere = collection.getByAddress(1);
    REQUIRE(stillThere != nullptr);
    REQUIRE(stillThere->name() == "Existing");
}

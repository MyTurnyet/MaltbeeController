#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/MatrixScanner.h"
#include "support/FakeDigitalInput.h"
#include "support/FakeDigitalOutput.h"

TEST_CASE("isActive returns false for every cell before any update() call")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});

    for (int row = 0; row < MatrixScanner::kRowCount; ++row)
    {
        for (int col = 0; col < MatrixScanner::kColumnCount; ++col)
        {
            REQUIRE_FALSE(scanner.isActive(row, col));
        }
    }
}

TEST_CASE("the first update() call asserts row 0 and only row 0")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});

    scanner.update();

    REQUIRE(row0.isSet());
    REQUIRE_FALSE(row1.isSet());
    REQUIRE_FALSE(row2.isSet());
}

TEST_CASE("each update() deasserts the previous row before asserting the next")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});

    scanner.update();
    scanner.update();

    REQUIRE_FALSE(row0.isSet());
    REQUIRE(row1.isSet());
    REQUIRE_FALSE(row2.isSet());

    scanner.update();

    REQUIRE_FALSE(row0.isSet());
    REQUIRE_FALSE(row1.isSet());
    REQUIRE(row2.isSet());
}

TEST_CASE("scanning wraps back to row 0 after a full cycle")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});

    scanner.update();
    scanner.update();
    scanner.update();
    scanner.update();

    REQUIRE(row0.isSet());
    REQUIRE_FALSE(row1.isSet());
    REQUIRE_FALSE(row2.isSet());
}

TEST_CASE("a row's cached readings reflect the column state at the moment it was scanned")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});

    col0.active = true;
    col2.active = true;

    scanner.update();

    REQUIRE(scanner.isActive(0, 0));
    REQUIRE_FALSE(scanner.isActive(0, 1));
    REQUIRE(scanner.isActive(0, 2));
    REQUIRE_FALSE(scanner.isActive(0, 3));
}

TEST_CASE("a row's cached readings are not clobbered by scanning other rows afterward")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});

    col1.active = true;
    scanner.update();

    col1.active = false;
    col3.active = true;
    scanner.update();

    REQUIRE(scanner.isActive(0, 1));
    REQUIRE(scanner.isActive(1, 3));
    REQUIRE_FALSE(scanner.isActive(1, 1));
}

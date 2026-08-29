#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/MatrixDigitalInput.h"
#include "domain/MatrixScanner.h"
#include "support/FakeDigitalInput.h"
#include "support/FakeDigitalOutput.h"

TEST_CASE("MatrixDigitalInput reflects the scanner's cached reading for its cell")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});
    MatrixDigitalInput input(scanner, 1, 2);

    REQUIRE_FALSE(input.isActive());

    scanner.update();
    col2.active = true;
    scanner.update();

    REQUIRE(input.isActive());
}

TEST_CASE("MatrixDigitalInput for a different cell is unaffected by another cell's state")
{
    FakeDigitalOutput row0, row1, row2;
    FakeDigitalInput col0, col1, col2, col3;
    MatrixScanner scanner({&row0, &row1, &row2}, {&col0, &col1, &col2, &col3});
    MatrixDigitalInput inputA(scanner, 0, 0);
    MatrixDigitalInput inputB(scanner, 0, 1);

    col0.active = true;
    scanner.update();

    REQUIRE(inputA.isActive());
    REQUIRE_FALSE(inputB.isActive());
}

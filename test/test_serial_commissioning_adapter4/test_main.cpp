#include <catch2/catch_test_macros.hpp>

#include "adapters/SerialCommissioningAdapter.h"
#include "support/FakeConfigStore4.h"
#include "support/FakeUartPort.h"

TEST_CASE("a turnout command within the 4-channel range is dispatched")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter4 adapter(uart, session);

    uart.queueInput("turnout 4 address 7\n");
    adapter.poll();

    REQUIRE(uart.written == "OK\n");
}

TEST_CASE("a turnout command outside the 4-channel range is rejected")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter4 adapter(uart, session);

    uart.queueInput("turnout 5 address 7\n");
    adapter.poll();

    REQUIRE(uart.written.rfind("error:", 0) == 0);
}

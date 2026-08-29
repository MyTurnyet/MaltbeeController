#include <catch2/catch_test_macros.hpp>

#include "adapters/SerialCommissioningAdapter.h"
#include "support/FakeConfigStore.h"
#include "support/FakeUartPort.h"

TEST_CASE("a complete line is parsed and dispatched on newline")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.queueInput("id 5\n");
    adapter.poll();

    REQUIRE(uart.written == "OK\n");
}

TEST_CASE("a partial line with no newline yet does not dispatch")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.queueInput("id 5");
    adapter.poll();

    REQUIRE(uart.written.empty());
}

TEST_CASE("multiple commands in sequence each dispatch separately")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.queueInput("id 5\nshow\n");
    adapter.poll();

    REQUIRE(uart.written.find("OK\n") != std::string::npos);
    REQUIRE(uart.written.find("id: 5") != std::string::npos);
}

TEST_CASE("a trailing carriage return before the newline is stripped")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.queueInput("id 5\r\n");
    adapter.poll();

    REQUIRE(uart.written == "OK\n");
}

TEST_CASE("bytes arriving across multiple poll() calls still assemble into one line")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    uart.queueInput("id ");
    adapter.poll();
    REQUIRE(uart.written.empty());

    uart.queueInput("5\n");
    adapter.poll();
    REQUIRE(uart.written == "OK\n");
}

TEST_CASE("rebootRequested delegates to the underlying session")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    REQUIRE_FALSE(adapter.rebootRequested());

    uart.queueInput("reboot\n");
    adapter.poll();

    REQUIRE(adapter.rebootRequested());
}

TEST_CASE("a line far exceeding the max length does not break subsequent commands")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    FakeUartPort uart;
    SerialCommissioningAdapter adapter(uart, session);

    const std::string overlong(SerialCommissioningAdapter::kMaxLineLength * 2, 'x');
    uart.queueInput(overlong + "\nid 5\n");
    adapter.poll();

    REQUIRE(uart.written.find("OK\n") != std::string::npos);
}

#include <catch2/catch_test_macros.hpp>

#include "adapters/WebFormCommissioningAdapter.h"
#include "application/CommissioningSession.h"
#include "domain/WebFormSubmission.h"
#include "support/FakeConfigStore.h"

namespace
{
    WebFormSubmission validSubmission()
    {
        WebFormSubmission form;
        form.nodeId = "5";
        form.wifiSsid = "MyLayoutWifi";
        form.wifiPassword = "hunter2";
        form.brokerHost = "192.168.1.50";
        form.brokerPort = "1883";
        form.channelJmriNames[0] = "LT1";
        return form;
    }
}

TEST_CASE("a fully valid submission saves and requests reboot")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const std::string response = adapter.submit(validSubmission());

    REQUIRE(response == "rebooting\n");
    REQUIRE(adapter.rebootRequested());
    REQUIRE(store.saveCount == 1);
    REQUIRE(store.load().nodeId == 5);
    REQUIRE(store.load().wifiSsid == "MyLayoutWifi");
    REQUIRE(store.load().channelJmriNames[0] == "LT1");
}

TEST_CASE("an empty channel name is skipped, not sent as an empty turnout command")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const WebFormSubmission form = validSubmission();
    // channelJmriNames[1..11] are empty by default construction

    adapter.submit(form);

    REQUIRE(store.load().channelJmriNames[1].empty());
}

TEST_CASE("a non-numeric node id stops immediately and never reaches save")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    form.nodeId = "not-a-number";

    const std::string response = adapter.submit(form);

    REQUIRE(response != "rebooting\n");
    REQUIRE_FALSE(adapter.rebootRequested());
    REQUIRE(store.saveCount == 0);
}

TEST_CASE("a save failure is reported and reboot is not requested")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);
    store.failNextSave = true;

    const std::string response = adapter.submit(validSubmission());

    REQUIRE(response == "save failed: could not write to storage\n");
    REQUIRE_FALSE(adapter.rebootRequested());
    REQUIRE(store.saveCount == 0);
}

TEST_CASE("currentValues reflects a previously applied draft change")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    ParsedCommand idCommand;
    idCommand.kind = CommandKind::Id;
    idCommand.intArg = 42;
    session.apply(idCommand);

    const WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.nodeId == "42");
}

TEST_CASE("currentValues reports an unset node id as an empty string")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.nodeId.empty());
}

TEST_CASE("wifi credentials and turnout names containing spaces round-trip intact")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    form.wifiSsid = "My Layout Wifi";
    form.wifiPassword = "a pass with spaces";
    form.channelJmriNames[0] = "Yard Ladder 3";

    const std::string response = adapter.submit(form);

    REQUIRE(response == "rebooting\n");
    REQUIRE(store.load().wifiSsid == "My Layout Wifi");
    REQUIRE(store.load().wifiPassword == "a pass with spaces");
    REQUIRE(store.load().channelJmriNames[0] == "Yard Ladder 3");
}

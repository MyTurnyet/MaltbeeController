#include <catch2/catch_test_macros.hpp>

#include "adapters/WebFormCommissioningAdapter.h"
#include "application/CommissioningSession.h"
#include "domain/NodeConfig.h"
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
        form.channelTurnoutAddresses[0] = "17";
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
    REQUIRE(store.load().channelTurnoutAddresses[0] == 17);
}

TEST_CASE("a blank channel field clears a previously stored channel address")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault()
                   .withNodeId(5)
                   .withWifi("w", "p")
                   .withBroker("h", 1883)
                   .withChannelAddress(2, 6));
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    // form.channelTurnoutAddresses[1] (channel 2) is blank by default construction

    adapter.submit(form);

    REQUIRE(store.load().channelTurnoutAddresses[1] == 0);
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

TEST_CASE("a non-numeric turnout address stops immediately and never reaches save")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    form.channelTurnoutAddresses[0] = "not-a-number";

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

TEST_CASE("currentValues reports an unset turnout address as an empty string")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.channelTurnoutAddresses[0].empty());
}

TEST_CASE("currentValues reports a configured turnout address as its decimal text")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault()
                   .withNodeId(5)
                   .withWifi("w", "p")
                   .withBroker("h", 1883)
                   .withChannelAddress(3, 99));
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.channelTurnoutAddresses[2] == "99");
}

TEST_CASE("wifi credentials containing spaces round-trip intact")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    form.wifiSsid = "My Layout Wifi";
    form.wifiPassword = "a pass with spaces";

    const std::string response = adapter.submit(form);

    REQUIRE(response == "rebooting\n");
    REQUIRE(store.load().wifiSsid == "My Layout Wifi");
    REQUIRE(store.load().wifiPassword == "a pass with spaces");
}

TEST_CASE("a blank wifi password keeps the previously stored password")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault()
                   .withNodeId(5)
                   .withWifi("MyLayoutWifi", "existing-secret")
                   .withBroker("h", 1883));
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    WebFormSubmission form = validSubmission();
    form.wifiPassword = "";

    const std::string response = adapter.submit(form);

    REQUIRE(response == "rebooting\n");
    REQUIRE(store.load().wifiPassword == "existing-secret");
}

TEST_CASE("currentValues never reflects a stored wifi password")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault()
                   .withNodeId(5)
                   .withWifi("MyLayoutWifi", "existing-secret")
                   .withBroker("h", 1883));
    CommissioningSession session(store);
    WebFormCommissioningAdapter adapter(session);

    const WebFormSubmission values = adapter.currentValues();

    REQUIRE(values.wifiPassword.empty());
    REQUIRE(values.wifiSsid == "MyLayoutWifi");
}

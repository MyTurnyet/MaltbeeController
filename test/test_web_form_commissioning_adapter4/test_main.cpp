#include <catch2/catch_test_macros.hpp>

#include "adapters/WebFormCommissioningAdapter.h"
#include "application/CommissioningSession.h"
#include "domain/NodeConfig.h"
#include "domain/WebFormSubmission.h"
#include "support/FakeConfigStore4.h"

namespace
{
    WebFormSubmission4 validSubmission()
    {
        WebFormSubmission4 form;
        form.nodeId = "5";
        form.wifiSsid = "MyLayoutWifi";
        form.wifiPassword = "hunter2";
        form.brokerHost = "192.168.1.50";
        form.brokerPort = "1883";
        form.channelTurnoutAddresses[0] = "17";
        form.channelTurnoutAddresses[3] = "20";
        return form;
    }
}

TEST_CASE("a fully valid 4-channel submission saves and requests reboot")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);
    WebFormCommissioningAdapter4 adapter(session);

    const std::string response = adapter.submit(validSubmission());

    REQUIRE(response == "rebooting\n");
    REQUIRE(adapter.rebootRequested());
    REQUIRE(store.saveCount == 1);
    REQUIRE(store.load().channelTurnoutAddresses[0] == 17);
    REQUIRE(store.load().channelTurnoutAddresses[3] == 20);
}

TEST_CASE("currentValues reports exactly 4 channel slots")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);
    WebFormCommissioningAdapter4 adapter(session);

    const WebFormSubmission4 values = adapter.currentValues();

    REQUIRE(values.channelTurnoutAddresses.size() == 4);
}

TEST_CASE("a blank channel field clears a previously stored channel address on the 4-channel node")
{
    FakeConfigStore4 store;
    store.save(NodeConfig4::factoryDefault()
                   .withNodeId(5)
                   .withWifi("w", "p")
                   .withBroker("h", 1883)
                   .withChannelAddress(2, 6));
    CommissioningSession4 session(store);
    WebFormCommissioningAdapter4 adapter(session);

    WebFormSubmission4 form = validSubmission();
    // form.channelTurnoutAddresses[1] (channel 2) is blank by default construction

    adapter.submit(form);

    REQUIRE(store.load().channelTurnoutAddresses[1] == 0);
}

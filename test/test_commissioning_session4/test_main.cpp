#include <catch2/catch_test_macros.hpp>

#include "application/CommissioningSession.h"
#include "support/FakeConfigStore4.h"

TEST_CASE("a turnout address command accepts channel 4")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);

    ParsedCommand command;
    command.kind = CommandKind::TurnoutAddress;
    command.intArg = 4;
    command.intArg2 = 7;

    const std::string response = session.apply(command);

    REQUIRE(response == "OK\n");
}

TEST_CASE("a turnout address command rejects channel 5, which was valid for the 12-channel panel")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);

    ParsedCommand command;
    command.kind = CommandKind::TurnoutAddress;
    command.intArg = 5;
    command.intArg2 = 7;

    const std::string response = session.apply(command);

    REQUIRE(response == "error: turnout channel must be between 1 and 4\n");
}

TEST_CASE("show reports exactly 4 turnout lines")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);

    ParsedCommand showCommand;
    showCommand.kind = CommandKind::Show;
    const std::string response = session.apply(showCommand);

    REQUIRE(response.find("turnout 4:") != std::string::npos);
    REQUIRE(response.find("turnout 5:") == std::string::npos);
}

TEST_CASE("save persists a valid draft with all 4 channels addressed")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);

    ParsedCommand idCommand;
    idCommand.kind = CommandKind::Id;
    idCommand.intArg = 1;
    session.apply(idCommand);

    ParsedCommand wifiCommand;
    wifiCommand.kind = CommandKind::Wifi;
    wifiCommand.stringArg1 = "MyLayoutWifi";
    wifiCommand.stringArg2 = "hunter2";
    session.apply(wifiCommand);

    ParsedCommand brokerCommand;
    brokerCommand.kind = CommandKind::Broker;
    brokerCommand.stringArg1 = "192.168.1.50";
    brokerCommand.intArg2 = 1883;
    session.apply(brokerCommand);

    for (int channel = 1; channel <= 4; ++channel)
    {
        ParsedCommand addressCommand;
        addressCommand.kind = CommandKind::TurnoutAddress;
        addressCommand.intArg = channel;
        addressCommand.intArg2 = channel;
        session.apply(addressCommand);
    }

    ParsedCommand saveCommand;
    saveCommand.kind = CommandKind::Save;
    const std::string response = session.apply(saveCommand);

    REQUIRE(response == "saved\n");
    REQUIRE(store.saveCount == 1);
}

TEST_CASE("save reports a failure when the 4-channel config store cannot persist")
{
    FakeConfigStore4 store;
    CommissioningSession4 session(store);

    ParsedCommand idCommand;
    idCommand.kind = CommandKind::Id;
    idCommand.intArg = 1;
    session.apply(idCommand);

    ParsedCommand wifiCommand;
    wifiCommand.kind = CommandKind::Wifi;
    wifiCommand.stringArg1 = "MyLayoutWifi";
    wifiCommand.stringArg2 = "hunter2";
    session.apply(wifiCommand);

    ParsedCommand brokerCommand;
    brokerCommand.kind = CommandKind::Broker;
    brokerCommand.stringArg1 = "192.168.1.50";
    brokerCommand.intArg2 = 1883;
    session.apply(brokerCommand);

    store.failNextSave = true;

    ParsedCommand saveCommand;
    saveCommand.kind = CommandKind::Save;
    const std::string response = session.apply(saveCommand);

    REQUIRE(response == "save failed: could not write to storage\n");
    REQUIRE(store.saveCount == 0);
}

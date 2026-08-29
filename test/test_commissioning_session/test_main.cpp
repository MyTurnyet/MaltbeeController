#include <catch2/catch_test_macros.hpp>

#include "domain/CommissioningSession.h"
#include "support/FakeConfigStore.h"

TEST_CASE("id command updates the draft and confirms")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand command;
    command.kind = CommandKind::Id;
    command.intArg = 5;

    const std::string response = session.apply(command);

    REQUIRE(response == "OK\n");
}

TEST_CASE("wifi command updates the draft and confirms")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand command;
    command.kind = CommandKind::Wifi;
    command.stringArg1 = "MyLayoutWifi";
    command.stringArg2 = "hunter2";

    const std::string response = session.apply(command);

    REQUIRE(response == "OK\n");
}

TEST_CASE("show reports an unconfigured factory-default draft")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand command;
    command.kind = CommandKind::Show;

    const std::string response = session.apply(command);

    REQUIRE(response.find("id: 0") != std::string::npos);
    REQUIRE(response.find("(unconfigured)") != std::string::npos);
}

TEST_CASE("show reports a configured turnout name")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand nameCommand;
    nameCommand.kind = CommandKind::TurnoutName;
    nameCommand.intArg = 1;
    nameCommand.stringArg1 = "LT1";
    session.apply(nameCommand);

    ParsedCommand showCommand;
    showCommand.kind = CommandKind::Show;
    const std::string response = session.apply(showCommand);

    REQUIRE(response.find("turnout 1: LT1") != std::string::npos);
}

TEST_CASE("an out-of-range turnout channel reports an error and stores nothing")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand nameCommand;
    nameCommand.kind = CommandKind::TurnoutName;
    nameCommand.intArg = 13;
    nameCommand.stringArg1 = "LT13";

    const std::string response = session.apply(nameCommand);

    REQUIRE(response.rfind("error:", 0) == 0);

    ParsedCommand showCommand;
    showCommand.kind = CommandKind::Show;
    const std::string showResponse = session.apply(showCommand);

    REQUIRE(showResponse.find("LT13") == std::string::npos);
}

TEST_CASE("save persists a valid draft to the config store")
{
    FakeConfigStore store;
    CommissioningSession session(store);

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

    ParsedCommand saveCommand;
    saveCommand.kind = CommandKind::Save;
    const std::string response = session.apply(saveCommand);

    REQUIRE(response == "saved\n");
    REQUIRE(store.saveCount == 1);
    REQUIRE(store.load().nodeId == 1);
}

TEST_CASE("save refuses an invalid draft and does not persist")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand saveCommand;
    saveCommand.kind = CommandKind::Save;
    const std::string response = session.apply(saveCommand);

    REQUIRE(response.find("invalid config") != std::string::npos);
    REQUIRE(store.saveCount == 0);
}

TEST_CASE("save reports a failure when the config store cannot persist")
{
    FakeConfigStore store;
    CommissioningSession session(store);

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

TEST_CASE("reboot sets rebootRequested and confirms")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    REQUIRE_FALSE(session.rebootRequested());

    ParsedCommand rebootCommand;
    rebootCommand.kind = CommandKind::Reboot;
    const std::string response = session.apply(rebootCommand);

    REQUIRE(response == "rebooting\n");
    REQUIRE(session.rebootRequested());
}

TEST_CASE("an invalid parsed command echoes its error message")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    ParsedCommand invalidCommand;
    invalidCommand.kind = CommandKind::Invalid;
    invalidCommand.errorMessage = "usage: id <n>";

    const std::string response = session.apply(invalidCommand);

    REQUIRE(response == "error: usage: id <n>\n");
}

TEST_CASE("a fresh session loads the store's existing config as its draft")
{
    FakeConfigStore store;
    store.save(NodeConfig::factoryDefault().withNodeId(7));

    CommissioningSession session(store);

    ParsedCommand showCommand;
    showCommand.kind = CommandKind::Show;
    const std::string response = session.apply(showCommand);

    REQUIRE(response.find("id: 7") != std::string::npos);
}

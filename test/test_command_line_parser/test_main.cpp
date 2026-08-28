#include <catch2/catch_test_macros.hpp>

#include "domain/CommandLineParser.h"

TEST_CASE("parses id command")
{
    const ParsedCommand command = CommandLineParser::parse("id 5");

    REQUIRE(command.kind == CommandKind::Id);
    REQUIRE(command.intArg == 5);
}

TEST_CASE("id command with a non-numeric argument is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("id abc");

    REQUIRE(command.kind == CommandKind::Invalid);
    REQUIRE_FALSE(command.errorMessage.empty());
}

TEST_CASE("id command with no argument is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("id");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("parses wifi command")
{
    const ParsedCommand command = CommandLineParser::parse("wifi MyLayoutWifi hunter2");

    REQUIRE(command.kind == CommandKind::Wifi);
    REQUIRE(command.stringArg1 == "MyLayoutWifi");
    REQUIRE(command.stringArg2 == "hunter2");
}

TEST_CASE("wifi command with a missing password is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("wifi MyLayoutWifi");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("parses broker command")
{
    const ParsedCommand command = CommandLineParser::parse("broker 192.168.1.50 1883");

    REQUIRE(command.kind == CommandKind::Broker);
    REQUIRE(command.stringArg1 == "192.168.1.50");
    REQUIRE(command.intArg2 == 1883);
}

TEST_CASE("broker command with a non-numeric port is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("broker 192.168.1.50 abc");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("parses turnout name command")
{
    const ParsedCommand command = CommandLineParser::parse("turnout 3 name LT3");

    REQUIRE(command.kind == CommandKind::TurnoutName);
    REQUIRE(command.intArg == 3);
    REQUIRE(command.stringArg1 == "LT3");
}

TEST_CASE("turnout command missing the name keyword is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("turnout 3 LT3");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("turnout command with a non-numeric channel is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("turnout x name LT3");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("parses show command")
{
    const ParsedCommand command = CommandLineParser::parse("show");

    REQUIRE(command.kind == CommandKind::Show);
}

TEST_CASE("parses save command")
{
    const ParsedCommand command = CommandLineParser::parse("save");

    REQUIRE(command.kind == CommandKind::Save);
}

TEST_CASE("parses reboot command")
{
    const ParsedCommand command = CommandLineParser::parse("reboot");

    REQUIRE(command.kind == CommandKind::Reboot);
}

TEST_CASE("unknown command is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("frobnicate");

    REQUIRE(command.kind == CommandKind::Invalid);
    REQUIRE_FALSE(command.errorMessage.empty());
}

TEST_CASE("empty line is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("");

    REQUIRE(command.kind == CommandKind::Invalid);
}

TEST_CASE("blank line (whitespace only) is invalid")
{
    const ParsedCommand command = CommandLineParser::parse("   ");

    REQUIRE(command.kind == CommandKind::Invalid);
}

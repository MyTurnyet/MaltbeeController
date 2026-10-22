#pragma once

#include <string>

#include "../application/CommissioningSession.h"
#include "../domain/CommandLineParser.h"
#include "../domain/NodeConfig.h"
#include "../domain/ParsedCommand.h"
#include "../domain/WebFormSubmission.h"

template <int N>
class BasicWebFormCommissioningAdapter
{
public:
    explicit BasicWebFormCommissioningAdapter(BasicCommissioningSession<N>& session) : session_(session)
    {
    }

    std::string submit(const BasicWebFormSubmission<N>& form)
    {
        std::string response = session_.apply(CommandLineParser::parse("id " + form.nodeId));
        if (response != "OK\n")
        {
            return response;
        }

        ParsedCommand wifiCommand;
        wifiCommand.kind = CommandKind::Wifi;
        wifiCommand.stringArg1 = form.wifiSsid;
        wifiCommand.stringArg2 = form.wifiPassword.empty() ? session_.draft().wifiPassword : form.wifiPassword;
        response = session_.apply(wifiCommand);
        if (response != "OK\n")
        {
            return response;
        }

        response = session_.apply(CommandLineParser::parse("broker " + form.brokerHost + " " + form.brokerPort));
        if (response != "OK\n")
        {
            return response;
        }

        for (int i = 0; i < N; ++i)
        {
            const std::string addressText =
                form.channelTurnoutAddresses[i].empty() ? "0" : form.channelTurnoutAddresses[i];
            response = session_.apply(
                CommandLineParser::parse("turnout " + std::to_string(i + 1) + " address " + addressText));
            if (response != "OK\n")
            {
                return response;
            }
        }

        response = session_.apply(CommandLineParser::parse("save"));
        if (response != "saved\n")
        {
            return response;
        }

        return session_.apply(CommandLineParser::parse("reboot"));
    }

    [[nodiscard]] bool rebootRequested() const
    {
        return session_.rebootRequested();
    }

    [[nodiscard]] BasicWebFormSubmission<N> currentValues() const
    {
        const BasicNodeConfig<N>& config = session_.draft();

        BasicWebFormSubmission<N> form;
        form.nodeId = config.nodeId == 0 ? "" : std::to_string(config.nodeId);
        form.wifiSsid = config.wifiSsid;
        form.wifiPassword = "";
        form.brokerHost = config.brokerHost;
        form.brokerPort = std::to_string(config.brokerPort);
        for (int i = 0; i < N; ++i)
        {
            form.channelTurnoutAddresses[i] =
                config.channelTurnoutAddresses[i] == 0 ? "" : std::to_string(config.channelTurnoutAddresses[i]);
        }

        return form;
    }

private:
    BasicCommissioningSession<N>& session_;
};

using WebFormCommissioningAdapter = BasicWebFormCommissioningAdapter<12>;
using WebFormCommissioningAdapter4 = BasicWebFormCommissioningAdapter<4>;

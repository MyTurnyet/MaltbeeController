#include "WebFormCommissioningAdapter.h"

#include "../domain/CommandLineParser.h"
#include "../domain/NodeConfig.h"
#include "../domain/ParsedCommand.h"

WebFormCommissioningAdapter::WebFormCommissioningAdapter(CommissioningSession& session) : session_(session)
{
}

std::string WebFormCommissioningAdapter::submit(const WebFormSubmission& form)
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

    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        ParsedCommand turnoutCommand;
        turnoutCommand.kind = CommandKind::TurnoutName;
        turnoutCommand.intArg = i + 1;
        turnoutCommand.stringArg1 = form.channelJmriNames[i];
        response = session_.apply(turnoutCommand);
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

bool WebFormCommissioningAdapter::rebootRequested() const
{
    return session_.rebootRequested();
}

WebFormSubmission WebFormCommissioningAdapter::currentValues() const
{
    const NodeConfig& config = session_.draft();

    WebFormSubmission form;
    form.nodeId = config.nodeId == 0 ? "" : std::to_string(config.nodeId);
    form.wifiSsid = config.wifiSsid;
    form.wifiPassword = "";
    form.brokerHost = config.brokerHost;
    form.brokerPort = std::to_string(config.brokerPort);
    form.channelJmriNames = config.channelJmriNames;

    return form;
}

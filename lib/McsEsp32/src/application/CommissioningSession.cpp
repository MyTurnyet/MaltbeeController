#include "CommissioningSession.h"

CommissioningSession::CommissioningSession(ConfigStore& store)
    : store_(store), draft_(store.load())
{
}

std::string CommissioningSession::formatErrors(const std::vector<std::string>& errors) const
{
    std::string result = "invalid config:\n";
    for (const std::string& error : errors)
    {
        result += "  - " + error + "\n";
    }
    return result;
}

std::string CommissioningSession::formatShow() const
{
    std::string result;
    result += "id: " + std::to_string(draft_.nodeId) + "\n";
    result += "wifi ssid: " + (draft_.wifiSsid.empty() ? std::string("(unconfigured)") : draft_.wifiSsid) + "\n";
    result += "broker: " + (draft_.brokerHost.empty() ? std::string("(unconfigured)") : draft_.brokerHost) +
              ":" + std::to_string(draft_.brokerPort) + "\n";
    for (int i = 0; i < NodeConfig::kChannelCount; ++i)
    {
        const std::string& name = draft_.channelJmriNames[i];
        result += "turnout " + std::to_string(i + 1) + ": " +
                  (name.empty() ? std::string("(unconfigured)") : name) + "\n";
    }
    return result;
}

std::string CommissioningSession::apply(const ParsedCommand& command)
{
    switch (command.kind)
    {
    case CommandKind::Id:
        draft_ = draft_.withNodeId(command.intArg);
        return "OK\n";

    case CommandKind::Wifi:
        draft_ = draft_.withWifi(command.stringArg1, command.stringArg2);
        return "OK\n";

    case CommandKind::Broker:
        draft_ = draft_.withBroker(command.stringArg1, command.intArg2);
        return "OK\n";

    case CommandKind::TurnoutName:
        if (command.intArg < 1 || command.intArg > NodeConfig::kChannelCount)
        {
            return "error: turnout channel must be between 1 and " +
                   std::to_string(NodeConfig::kChannelCount) + "\n";
        }
        draft_ = draft_.withChannelName(command.intArg, command.stringArg1);
        return "OK\n";

    case CommandKind::Show:
        return formatShow();

    case CommandKind::Save:
    {
        const std::vector<std::string> errors = draft_.validate();
        if (!errors.empty())
        {
            return formatErrors(errors);
        }
        if (!store_.save(draft_))
        {
            return "save failed: could not write to storage\n";
        }
        return "saved\n";
    }

    case CommandKind::Reboot:
        rebootRequested_ = true;
        return "rebooting\n";

    case CommandKind::Invalid:
    default:
        return "error: " + command.errorMessage + "\n";
    }
}

bool CommissioningSession::rebootRequested() const
{
    return rebootRequested_;
}

const NodeConfig& CommissioningSession::draft() const
{
    return draft_;
}

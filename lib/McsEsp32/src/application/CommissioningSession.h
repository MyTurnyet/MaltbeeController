#pragma once

#include <string>
#include <vector>

#include "../domain/NodeConfig.h"
#include "../domain/ParsedCommand.h"
#include "../ports/ConfigStore.h"

template <int N>
class BasicCommissioningSession
{
public:
    explicit BasicCommissioningSession(BasicConfigStore<N>& store) : store_(store), draft_(store.load())
    {
    }

    std::string apply(const ParsedCommand& command)
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

        case CommandKind::TurnoutAddress:
            if (command.intArg < 1 || command.intArg > N)
            {
                return "error: turnout channel must be between 1 and " + std::to_string(N) + "\n";
            }
            draft_ = draft_.withChannelAddress(command.intArg, command.intArg2);
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

    [[nodiscard]] bool rebootRequested() const
    {
        return rebootRequested_;
    }

    [[nodiscard]] const BasicNodeConfig<N>& draft() const
    {
        return draft_;
    }

private:
    [[nodiscard]] std::string formatErrors(const std::vector<std::string>& errors) const
    {
        std::string result = "invalid config:\n";
        for (const std::string& error : errors)
        {
            result += "  - " + error + "\n";
        }
        return result;
    }

    [[nodiscard]] std::string formatShow() const
    {
        std::string result;
        result += "id: " + std::to_string(draft_.nodeId) + "\n";
        result += "wifi ssid: " + (draft_.wifiSsid.empty() ? std::string("(unconfigured)") : draft_.wifiSsid) + "\n";
        result += "broker: " + (draft_.brokerHost.empty() ? std::string("(unconfigured)") : draft_.brokerHost) +
                  ":" + std::to_string(draft_.brokerPort) + "\n";
        for (int i = 0; i < N; ++i)
        {
            const int address = draft_.channelTurnoutAddresses[i];
            result += "turnout " + std::to_string(i + 1) + ": " +
                      (address == 0 ? std::string("(unconfigured)") : std::to_string(address)) + "\n";
        }
        return result;
    }

    BasicConfigStore<N>& store_;
    BasicNodeConfig<N> draft_;
    bool rebootRequested_ = false;
};

using CommissioningSession = BasicCommissioningSession<12>;
using CommissioningSession4 = BasicCommissioningSession<4>;

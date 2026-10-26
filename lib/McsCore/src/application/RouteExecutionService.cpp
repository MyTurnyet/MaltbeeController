#include "RouteExecutionService.h"

RouteExecutionService::RouteExecutionService(TurnoutCommandPort& commandPort, Clock& clock,
                                               const unsigned long commandSpacingMs)
    : commandPort_(commandPort), clock_(clock), commandSpacingMs_(commandSpacingMs)
{
}

void RouteExecutionService::start(const Route& route)
{
    if (route.getTurnoutCount() == 0)
    {
        activeRoute_ = nullptr;
        return;
    }

    activeRoute_ = &route;
    nextCommandIndex_ = 0;

    const TurnoutCommand firstCommand = activeRoute_->commandAt(nextCommandIndex_);
    commandPort_.send(firstCommand.address, firstCommand.position);
    ++nextCommandIndex_;

    if (nextCommandIndex_ >= activeRoute_->getTurnoutCount())
    {
        activeRoute_ = nullptr;
        return;
    }

    lastSendAtMs_ = clock_.nowMilliseconds();
}

void RouteExecutionService::update()
{
    if (activeRoute_ == nullptr)
    {
        return;
    }

    if (nextCommandIndex_ >= activeRoute_->getTurnoutCount())
    {
        activeRoute_ = nullptr;
        return;
    }

    if (clock_.nowMilliseconds() - lastSendAtMs_ < commandSpacingMs_)
    {
        return;
    }

    const TurnoutCommand command = activeRoute_->commandAt(nextCommandIndex_);
    commandPort_.send(command.address, command.position);
    ++nextCommandIndex_;

    if (nextCommandIndex_ >= activeRoute_->getTurnoutCount())
    {
        activeRoute_ = nullptr;
        return;
    }

    lastSendAtMs_ = clock_.nowMilliseconds();
}

bool RouteExecutionService::isRunning() const
{
    return activeRoute_ != nullptr;
}

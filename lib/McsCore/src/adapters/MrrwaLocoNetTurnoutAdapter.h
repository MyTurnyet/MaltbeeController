#pragma once

#include "../ports/LocoNetTransport.h"
#include "../ports/TurnoutCommandPort.h"

class MrrwaLocoNetTurnoutAdapter final : public TurnoutCommandPort
{
public:
    explicit MrrwaLocoNetTurnoutAdapter(LocoNetTransport& transport);

    void send(int address, TurnoutPosition position) override;

private:
    LocoNetTransport& transport_;
};

#include "MrrwaLocoNetTurnoutAdapter.h"

MrrwaLocoNetTurnoutAdapter::MrrwaLocoNetTurnoutAdapter(LocoNetTransport& transport)
    : transport_(transport)
{
}

void MrrwaLocoNetTurnoutAdapter::send(int address, TurnoutPosition position)
{
    transport_.sendPacket({address, position});
}

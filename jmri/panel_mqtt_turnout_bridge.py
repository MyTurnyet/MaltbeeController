# panel_mqtt_turnout_bridge.py
#
# Bridges the ESP32 panel's MQTT commands/state to JMRI's real Turnout
# objects (LocoNet or otherwise) -- no shadow "MT" turnouts, no Logix.
#
# Install as a Startup script: Edit -> Preferences -> Startup -> Add ->
# "Jython script" -> point at this file -> Save -> restart JMRI.
#
# Requires the MQTT system connection to already be configured under
# Edit -> Preferences -> Connections, with "MQTT Channel" blank so the
# topics below are exactly what's on the wire.
#
#   Panel -> JMRI (command): track/turnout/<system name>   e.g. LT5
#   JMRI -> Panel (state):   panel/turnout/<panel number>/state
#
# Payload is assumed to be "THROWN" or "CLOSED" in both directions --
# that's confirmed correct for the state side already. If commands
# still don't take effect after this fix, check the JMRI System Console
# for a line like "Ignoring MQTT turnout command: track/turnout/LT5 ..."
# -- that means the topic matched but the payload text didn't, and
# you'll see the exact string the panel actually sent.

import jmri
import java

# --- Configuration: panel turnout number -> real JMRI system name ---
# Keep this in sync with the ESP32's jmriSystemName field per turnout.
PANEL_TURNOUTS = {
    1:  "LT1",  2:  "LT2",  3:  "LT3",  4:  "LT4",
    5:  "LT5",  6:  "LT6",  7:  "LT7",  8:  "LT8",
    9:  "LT9",  10: "LT10", 11: "LT11", 12: "LT12",
}

CMD_TOPIC_PATTERN = "track/turnout/+"        # matches what the panel actually sends
STATE_TOPIC = "panel/turnout/%s/state"       # published per-turnout, retained

PAYLOAD_TO_STATE = {"THROWN": THROWN, "CLOSED": CLOSED}
STATE_TO_PAYLOAD = {THROWN: "THROWN", CLOSED: "CLOSED"}


class PanelCommandListener(jmri.jmrix.mqtt.MqttEventListener):
    """Applies an MQTT command from the panel to the matching turnout.

    The topic's final segment IS the turnout's JMRI system name, e.g.
    'track/turnout/LT5' -> turnout LT5 -- no number lookup needed."""

    def notifyMqttMessage(self, topic, message):
        systemName = self._systemNameFrom(topic)
        state = PAYLOAD_TO_STATE.get(message)
        if systemName is None or state is None:
            print("Ignoring MQTT turnout command:", topic, message)
            return
        turnout = turnouts.provideTurnout(systemName)
        turnout.commandedState = state
        turnout.knownState = state  # forces the KnownState change that
                                     # TurnoutStateBroadcaster listens for

    def _systemNameFrom(self, topic):
        parts = topic.split("/")
        return parts[-1] if parts else None


class TurnoutStateBroadcaster(java.beans.PropertyChangeListener):
    """Publishes a turnout's confirmed state to MQTT on every change,
    regardless of what caused it (this panel, another panel, PanelPro,
    a dispatcher, or a physical throw with feedback)."""

    def __init__(self, mqttAdapter, panelNumber, systemName):
        self.mqtt = mqttAdapter
        self.topic = STATE_TOPIC % panelNumber
        turnouts.provideTurnout(systemName).addPropertyChangeListener(self)

    def propertyChange(self, event):
        if event.propertyName != "KnownState":
            return
        payload = STATE_TO_PAYLOAD.get(event.newValue)
        if payload is not None:
            self.mqtt.publish(self.topic, payload)


mqttAdapter = jmri.InstanceManager.getDefault(
    jmri.jmrix.mqtt.MqttSystemConnectionMemo).getMqttAdapter()

mqttAdapter.subscribe(CMD_TOPIC_PATTERN, PanelCommandListener())

# Keep references alive -- property change listeners are held weakly.
_broadcasters = [
    TurnoutStateBroadcaster(mqttAdapter, number, systemName)
    for number, systemName in PANEL_TURNOUTS.items()
]

print("Panel <-> MQTT <-> turnout bridge active for", len(PANEL_TURNOUTS), "turnouts")

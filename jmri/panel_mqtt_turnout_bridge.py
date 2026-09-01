
#
# Bridges the ESP32 panel's MQTT commands/state to JMRI's real Turnout
# objects (LocoNet or otherwise) -- no shadow "MT" turnouts, no Logix.
#
# Install as a Startup script: Edit -> Preferences -> Startup -> Add ->
# "Jython script" -> point at this file -> Save -> restart JMRI.
#
# Requires the MQTT system connection to already be configured under
# Edit -> Preferences -> Connections. Recommend setting "MQTT Channel"
# to blank so the topics below are exactly what's on the wire -- the
# ESP32 firmware has to publish/subscribe to these same literal strings.

import jmri
import java

# --- Configuration: panel turnout number -> real JMRI system name ---
# Keep this in sync with the ESP32's jmriSystemName field per turnout.
PANEL_TURNOUTS = {
    1:  "LT1",  2:  "LT2",  3:  "LT3",  4:  "LT4",
    5:  "LT5",  6:  "LT6",  7:  "LT7",  8:  "LT8",
    9:  "LT9",  10: "LT10", 11: "LT11", 12: "LT12",
}

CMD_TOPIC_PATTERN = "panel/turnout/+/cmd"    # subscribed once, wildcard
STATE_TOPIC = "panel/turnout/%s/state"       # published per-turnout, retained

PAYLOAD_TO_STATE = {"THROWN": THROWN, "CLOSED": CLOSED}
STATE_TO_PAYLOAD = {THROWN: "THROWN", CLOSED: "CLOSED"}


class PanelCommandListener(jmri.jmrix.mqtt.MqttEventListener):
    """Applies an MQTT command from the panel to the matching turnout."""

    def notifyMqttMessage(self, topic, message):
        number = self._turnoutNumberFrom(topic)
        state = PAYLOAD_TO_STATE.get(message)
        if number is None or state is None:
            print("Ignoring MQTT turnout command:", topic, message)
            return
        systemName = PANEL_TURNOUTS.get(number)
        if systemName is None:
            print("No turnout configured for panel number", number)
            return
        turnouts.provideTurnout(systemName).commandedState = state

    def _turnoutNumberFrom(self, topic):
        parts = topic.split("/")
        try:
            return int(parts[2])
        except (IndexError, ValueError):
            return None


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

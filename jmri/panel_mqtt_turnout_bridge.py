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
#   Panel -> JMRI (command): track/turnout/<system name>          e.g. LT5
#   JMRI -> Panel (state):   track/turnout/<system name>/state    e.g. LT5
#
# Payload is "THROWN" or "CLOSED" in both directions. If commands still
# don't take effect, check the JMRI System Console for a line like
# "Ignoring MQTT turnout command: track/turnout/LT5 ..." -- that means
# the topic matched but the payload text didn't, and you'll see the
# exact string the panel actually sent.

import jmri
import java

# --- Configuration: which real JMRI turnouts this bridge covers ---
# NOTE: the previous version of this list was a dict with keys 13, 14,
# and 15 each used twice -- in Python a repeated dict key silently
# keeps only the LAST value, so "LT16", "LT18", and "LT19" were being
# dropped without any error. Converting to a plain list sidesteps that
# whole class of bug (there's no key to collide on), but the three
# turnouts below are commented out until you confirm they should be
# included -- see the note at the end of this file.
TURNOUT_SYSTEM_NAMES = [
    "LT1", "LT2", "LT3", "LT4", "LT5", "LT6", "LT7",
    "LT9", "LT10", "LT11", "LT13", "LT14",
    # "LT16", "LT18", "LT19",   # <-- were silently dropped before; confirm and uncomment
    "LT20", "LT21", "LT22", "LT23", "LT24",
]

CMD_TOPIC_PATTERN = "track/turnout/+"          # subscribed once, wildcard
STATE_TOPIC = "track/turnout/%s/state"         # published per-turnout, retained

PAYLOAD_TO_STATE = {"THROWN": THROWN, "CLOSED": CLOSED}
STATE_TO_PAYLOAD = {THROWN: "THROWN", CLOSED: "CLOSED"}


class _HandleCommandOnOwnThread(java.lang.Runnable):
    """Runs a command handler off the MQTT client's callback thread.

    Calling mqttAdapter.publish() synchronously from inside
    notifyMqttMessage() can deadlock that thread -- it's the same
    thread the client uses for its own network processing. JMRI
    recovers by dropping and reconnecting, which silently swallows
    the publish. Handling the command on a plain thread avoids that
    entirely."""

    def __init__(self, listener, topic, message):
        self.listener = listener
        self.topic = topic
        self.message = message

    def run(self):
        self.listener.applyCommand(self.topic, self.message)


class PanelCommandListener(jmri.jmrix.mqtt.MqttEventListener):
    """Applies an MQTT command from the panel to the matching turnout,
    then reports the result back unconditionally -- even if the turnout
    was already in that state. A KnownState *change* event won't fire
    in that case (PropertyChangeSupport skips no-op updates), which is
    exactly the case where the panel most needs to be told, since it's
    the one that's out of sync.

    The topic's final segment IS the turnout's JMRI system name, e.g.
    'track/turnout/LT5' -> turnout LT5 -- no separate lookup needed."""

    def __init__(self, mqttAdapter):
        self.mqtt = mqttAdapter

    def notifyMqttMessage(self, topic, message):
        java.lang.Thread(_HandleCommandOnOwnThread(self, topic, message)).start()

    def applyCommand(self, topic, message):
        systemName = self._systemNameFrom(topic)
        state = PAYLOAD_TO_STATE.get(message)
        if systemName is None or state is None:
            print("Ignoring MQTT turnout command:", topic, message)
            return
        turnout = turnouts.provideTurnout(systemName)
        turnout.setCommandedState(state)
        self.mqtt.publish(STATE_TOPIC % systemName, STATE_TO_PAYLOAD[state])

    def _systemNameFrom(self, topic):
        parts = topic.split("/")
        return parts[-1] if parts else None


class TurnoutStateBroadcaster(java.beans.PropertyChangeListener):
    """Publishes a turnout's confirmed state to MQTT on every change,
    regardless of what caused it (this panel, another panel, PanelPro,
    a dispatcher, or a physical throw with feedback)."""

    def __init__(self, mqttAdapter, systemName):
        self.mqtt = mqttAdapter
        self.topic = STATE_TOPIC % systemName
        turnouts.provideTurnout(systemName).addPropertyChangeListener(self)

    def propertyChange(self, event):
        if event.propertyName != "KnownState":
            return
        payload = STATE_TO_PAYLOAD.get(event.newValue)
        if payload is not None:
            self.mqtt.publish(self.topic, payload)


mqttAdapter = jmri.InstanceManager.getDefault(
    jmri.jmrix.mqtt.MqttSystemConnectionMemo).getMqttAdapter()

mqttAdapter.subscribe(CMD_TOPIC_PATTERN, PanelCommandListener(mqttAdapter))

# Keep references alive -- property change listeners are held weakly.
_broadcasters = [
    TurnoutStateBroadcaster(mqttAdapter, systemName)
    for systemName in TURNOUT_SYSTEM_NAMES
]

print("Panel <-> MQTT <-> turnout bridge active for", len(TURNOUT_SYSTEM_NAMES), "turnouts")

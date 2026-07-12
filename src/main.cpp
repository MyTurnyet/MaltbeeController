#include <Arduino.h>

constexpr uint8_t LED_PIN = 8;

void setup()
{
    pinMode(LED_PIN, OUTPUT);

    Serial.begin(115200);
    Serial.println("Mega controller started");
}

void loop()
{
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED ON");
    delay(1000);

    digitalWrite(LED_PIN, LOW);
    Serial.println("LED OFF");
    delay(1000);
}
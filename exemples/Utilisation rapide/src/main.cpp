#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
  #define PIN_LED 2          // GPIO2 : LED intégrée de la plupart des dev boards ESP32
#else
  #include <ESP8266WiFi.h>
  #define PIN_LED D5         // GPIO14
#endif
#include <Server_MCP.h>

Server_MCP mcp("MonServeur", "1.1.2");

std::vector<MCPContent> allumerLED(const JsonObject& params) {
    digitalWrite(PIN_LED, HIGH);
    return { Server_MCP::makeTextContent("LED allumée !") };
}

void setup() {
    Serial.begin(115200);
    WiFi.begin("SSID", "PASSWORD");
    while (WiFi.status() != WL_CONNECTED) delay(500);

    mcp.setSerialDebug(true, &Serial);
    mcp.registerTool("led_on", "Allume la LED", allumerLED);
    mcp.begin(8080);
}

void loop() {
    mcp.handleClient();
}

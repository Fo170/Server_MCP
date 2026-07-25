#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Server_MCP.h>

Server_MCP mcp("MonServeur", "1.0.0");

std::vector<MCPContent> allumerLED(const JsonObject& params) {
    digitalWrite(D5, HIGH);
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

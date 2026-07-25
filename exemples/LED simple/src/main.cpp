#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "Server_MCP.h"

const char* WIFI_SSID = "MonWifi";
const char* WIFI_PASSWORD = "MonMotDePasse";

const int PIN_LED = D5;

Server_MCP mcp("ESP-LED", "1.0.0");

std::vector<MCPContent> ledOn(const JsonObject& params) {
    digitalWrite(PIN_LED, HIGH);
    return { Server_MCP::makeTextContent("LED allumée") };
}

std::vector<MCPContent> ledOff(const JsonObject& params) {
    digitalWrite(PIN_LED, LOW);
    return { Server_MCP::makeTextContent("LED éteinte") };
}

std::vector<MCPContent> ledStatus(const JsonObject& params) {
    String etat = digitalRead(PIN_LED) == HIGH ? "ON" : "OFF";
    return { Server_MCP::makeTextContent("État LED: " + etat) };
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) delay(500);

    mcp.setSerialDebug(true, &Serial);

    mcp.registerTool("led_on", "Allume la LED", ledOn);
    mcp.registerTool("led_off", "Éteint la LED", ledOff);
    mcp.registerTool("led_status", "Lit l'état de la LED", ledStatus);

    mcp.begin(8080);
    Serial.println("Serveur MCP: " + mcp.getServerURL());
}

void loop() {
    mcp.handleClient();
}

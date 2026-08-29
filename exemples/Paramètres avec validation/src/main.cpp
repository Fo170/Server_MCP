#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
  #define PIN_ROUGE 14      // GPIO14
  #define PIN_VERTE 12      // GPIO12
  #define PIN_BLEUE 13      // GPIO13
#else
  #include <ESP8266WiFi.h>
  #define PIN_ROUGE D5      // GPIO14
  #define PIN_VERTE D6      // GPIO12
  #define PIN_BLEUE D7      // GPIO13
#endif
#include <Server_MCP.h>

const char* WIFI_SSID = "MonWifi";
const char* WIFI_PASSWORD = "MonMotDePasse";

Server_MCP mcp("ESP-LED-RGB", "1.2.0");

std::vector<MCPContent> clignoterLED(const JsonObject& params) {
    String couleur = params["couleur"].as<String>();
    int duree = params["duree_ms"].as<int>();
    int repetitions = params["repetitions"].as<int>();

    int pin = (couleur == "verte") ? PIN_VERTE : (couleur == "bleue") ? PIN_BLEUE : PIN_ROUGE;

    for (int i = 0; i < repetitions; i++) {
        digitalWrite(pin, HIGH);
        delay(duree);
        digitalWrite(pin, LOW);
        delay(duree);
    }

    String msg = "Clignotement " + couleur + ": " + String(repetitions) + " cycles";
    return { Server_MCP::makeTextContent(msg) };
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_ROUGE, OUTPUT);
    pinMode(PIN_VERTE, OUTPUT);
    pinMode(PIN_BLEUE, OUTPUT);
    digitalWrite(PIN_ROUGE, LOW);
    digitalWrite(PIN_VERTE, LOW);
    digitalWrite(PIN_BLEUE, LOW);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) delay(500);

    mcp.setSerialDebug(true, &Serial);

    mcp.registerTool("clignoter", "Fait clignoter une LED", clignoterLED);
    mcp.addToolParam("couleur", "Couleur de la LED", "string", true);
    mcp.addToolParam("duree_ms", "Durée d'un clignotement (ms)", "integer", false);
    mcp.addToolParam("repetitions", "Nombre de clignotements", "integer", false);

    mcp.begin(8081);
    Serial.println("Serveur MCP: " + mcp.getServerURL());
}

void loop() {
    mcp.handleClient();
}

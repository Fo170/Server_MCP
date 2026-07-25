// ============================================
// main.cpp - ESP8266 avec Web (port 80) + MCP (port 8080)
// ============================================

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Server_MCP.h>

// ═══════════════════════════════════════════
// CONFIGURATION WIFI
// ═══════════════════════════════════════════

const char* WIFI_SSID = "TON_SSID_WIFI";
const char* WIFI_PASSWORD = "TON_MOT_DE_PASSE_WIFI";

// ═══════════════════════════════════════════
// BROCHES
// ═══════════════════════════════════════════

const int PIN_LED_ROUGE = D5;     // GPIO14
const int PIN_LED_BLANCHE = D6;   // GPIO12
const int PIN_LED_BUILTIN = LED_BUILTIN;

// ═══════════════════════════════════════════
// VARIABLES GLOBALES PARTAGEES
// ═══════════════════════════════════════════

float temperature = 22.5;   // Simule une vraie lecture capteur
float humidite = 55.0;
bool chauffageActif = false;

// ═══════════════════════════════════════════
// SERVEURS
// ═══════════════════════════════════════════

ESP8266WebServer webServer(80);    // Interface web utilisateur
Server_MCP mcpServer("ESP8266-MCP", "1.0.0");  // Serveur MCP sur port 8080

// ═══════════════════════════════════════════
// FONCTIONS WEB (Port 80)
// ═══════════════════════════════════════════

void handleWebRoot() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>ESP8266 - Dashboard</title>
    <style>
        body { font-family: Arial; background: #1a1a2e; color: #eee; text-align: center; padding: 40px; }
        .card { background: #16213e; border-radius: 15px; padding: 30px; margin: 20px auto; max-width: 400px; box-shadow: 0 4px 15px rgba(0,0,0,0.3); }
        .temp { font-size: 48px; color: #e94560; }
        .hum { font-size: 36px; color: #0f3460; }
        .led { display: inline-block; width: 30px; height: 30px; border-radius: 50%; margin: 10px; }
        .led-on { background: #00ff88; box-shadow: 0 0 15px #00ff88; }
        .led-off { background: #333; }
        button { padding: 12px 24px; margin: 5px; border: none; border-radius: 8px; cursor: pointer; font-size: 14px; }
        .btn-on { background: #e94560; color: white; }
        .btn-off { background: #0f3460; color: white; }
        .refresh { background: #533483; color: white; }
    </style>
</head>
<body>
    <h1>🌡️ ESP8266 Dashboard</h1>
    <div class="card">
        <h2>Température</h2>
        <div class="temp">)rawliteral" + String(temperature, 1) + R"rawliteral(°C</div>
        <h3>Humidité</h3>
        <div class="hum">)rawliteral" + String(humidite, 1) + R"rawliteral(%</div>
    </div>
    <div class="card">
        <h2>État des LEDs</h2>
        <div>
            <span>Rouge:</span>
            <div class="led )rawliteral" + String(digitalRead(PIN_LED_ROUGE) == HIGH ? "led-on" : "led-off") + R"rawliteral("></div>
            <span>Blanche:</span>
            <div class="led )rawliteral" + String(digitalRead(PIN_LED_BLANCHE) == HIGH ? "led-on" : "led-off") + R"rawliteral("></div>
        </div>
        <br>
        <button class="btn-on" onclick="fetch('/api/led/rouge/on').then(()=>location.reload())">Rouge ON</button>
        <button class="btn-off" onclick="fetch('/api/led/rouge/off').then(()=>location.reload())">Rouge OFF</button>
        <br>
        <button class="btn-on" onclick="fetch('/api/led/blanche/on').then(()=>location.reload())">Blanche ON</button>
        <button class="btn-off" onclick="fetch('/api/led/blanche/off').then(()=>location.reload())">Blanche OFF</button>
        <br><br>
        <button class="refresh" onclick="location.reload()">🔄 Rafraîchir</button>
    </div>
    <p style="color:#666; font-size:12px;">Serveur MCP actif sur le port 8080</p>
</body>
</html>
)rawliteral";
    webServer.send(200, "text/html", html);
}

void handleApiTemp() {
    StaticJsonDocument<256> doc;
    doc["temperature"] = temperature;
    doc["humidite"] = humidite;
    doc["unite"] = "Celsius";
    doc["timestamp"] = millis();
    String response;
    serializeJson(doc, response);
    webServer.send(200, "application/json", response);
}

void handleApiLedRougeOn() {
    digitalWrite(PIN_LED_ROUGE, HIGH);
    webServer.send(200, "text/plain", "OK");
}

void handleApiLedRougeOff() {
    digitalWrite(PIN_LED_ROUGE, LOW);
    webServer.send(200, "text/plain", "OK");
}

void handleApiLedBlancheOn() {
    digitalWrite(PIN_LED_BLANCHE, HIGH);
    webServer.send(200, "text/plain", "OK");
}

void handleApiLedBlancheOff() {
    digitalWrite(PIN_LED_BLANCHE, LOW);
    webServer.send(200, "text/plain", "OK");
}

// ═══════════════════════════════════════════
// CALLBACKS MCP (Port 8080)
// ═══════════════════════════════════════════

std::vector<MCPContent> mcp_ledRougeOn(const JsonObject& params) {
    digitalWrite(PIN_LED_ROUGE, HIGH);
    return { Server_MCP::makeTextContent("✅ LED rouge allumée") };
}

std::vector<MCPContent> mcp_ledRougeOff(const JsonObject& params) {
    digitalWrite(PIN_LED_ROUGE, LOW);
    return { Server_MCP::makeTextContent("⬛ LED rouge éteinte") };
}

std::vector<MCPContent> mcp_ledBlancheOn(const JsonObject& params) {
    digitalWrite(PIN_LED_BLANCHE, HIGH);
    return { Server_MCP::makeTextContent("✅ LED blanche allumée") };
}

std::vector<MCPContent> mcp_ledBlancheOff(const JsonObject& params) {
    digitalWrite(PIN_LED_BLANCHE, LOW);
    return { Server_MCP::makeTextContent("⬛ LED blanche éteinte") };
}

std::vector<MCPContent> mcp_allOff(const JsonObject& params) {
    digitalWrite(PIN_LED_ROUGE, LOW);
    digitalWrite(PIN_LED_BLANCHE, LOW);
    digitalWrite(PIN_LED_BUILTIN, HIGH);
    return { Server_MCP::makeTextContent("⬛ Toutes les LEDs éteintes") };
}

std::vector<MCPContent> mcp_getStatus(const JsonObject& params) {
    String status = "📊 État du système:\n";
    status += "  🌡️ Température: " + String(temperature, 1) + "°C\n";
    status += "  💧 Humidité: " + String(humidite, 1) + "%\n";
    status += "  🔴 LED Rouge: " + String(digitalRead(PIN_LED_ROUGE) == HIGH ? "ON" : "OFF") + "\n";
    status += "  ⚪ LED Blanche: " + String(digitalRead(PIN_LED_BLANCHE) == HIGH ? "ON" : "OFF") + "\n";
    status += "  🔵 LED Builtin: " + String(digitalRead(PIN_LED_BUILTIN) == LOW ? "ON" : "OFF") + "\n";
    status += "  🔥 Chauffage: " + String(chauffageActif ? "ACTIF" : "INACTIF");
    return { Server_MCP::makeTextContent(status) };
}

std::vector<MCPContent> mcp_getTemperature(const JsonObject& params) {
    String msg = "🌡️ Température actuelle: " + String(temperature, 1) + "°C\n";
    msg += "💧 Humidité: " + String(humidite, 1) + "%";
    return { Server_MCP::makeTextContent(msg) };
}

std::vector<MCPContent> mcp_setChauffage(const JsonObject& params) {
    if (!params.containsKey("actif")) {
        return { Server_MCP::makeTextContent("❌ Paramètre 'actif' requis (true/false)") };
    }
    chauffageActif = params["actif"].as<bool>();
    String msg = chauffageActif ? "🔥 Chauffage ACTIVÉ" : "❄️ Chauffage DÉSACTIVÉ";
    return { Server_MCP::makeTextContent(msg) };
}

std::vector<MCPContent> mcp_clignoter(const JsonObject& params) {
    String couleur = params.containsKey("couleur") ? params["couleur"].as<String>() : "rouge";
    int duree = params.containsKey("duree_ms") ? params["duree_ms"].as<int>() : 500;
    int repetitions = params.containsKey("repetitions") ? params["repetitions"].as<int>() : 3;

    int pin = PIN_LED_ROUGE;
    if (couleur == "blanche") pin = PIN_LED_BLANCHE;
    else if (couleur == "builtin") pin = PIN_LED_BUILTIN;

    bool isBuiltin = (pin == PIN_LED_BUILTIN);
    bool wasOn = isBuiltin ? (digitalRead(pin) == LOW) : (digitalRead(pin) == HIGH);

    for (int i = 0; i < repetitions; i++) {
        digitalWrite(pin, isBuiltin ? LOW : HIGH);
        delay(duree);
        digitalWrite(pin, isBuiltin ? HIGH : LOW);
        delay(duree);
    }
    digitalWrite(pin, wasOn ? (isBuiltin ? LOW : HIGH) : (isBuiltin ? HIGH : LOW));

    String msg = "💡 Clignotement " + couleur + ": " + String(repetitions) + "x (" + String(duree) + "ms)";
    return { Server_MCP::makeTextContent(msg) };
}

// ═══════════════════════════════════════════
// SIMULATION CAPTEUR (dans loop)
// ═══════════════════════════════════════════

void updateSensorData() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        lastUpdate = millis();
        // Simule des variations de température
        temperature += random(-10, 11) / 10.0;
        temperature = constrain(temperature, 15.0, 35.0);
        humidite += random(-5, 6) / 10.0;
        humidite = constrain(humidite, 30.0, 80.0);
    }
}

// ═══════════════════════════════════════════
// SETUP & LOOP
// ═══════════════════════════════════════════

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n╔══════════════════════════════════════════════╗");
    Serial.println("║     ESP8266 - Web (80) + MCP (8080)          ║");
    Serial.println("╚══════════════════════════════════════════════╝");

    // GPIO
    pinMode(PIN_LED_ROUGE, OUTPUT);
    pinMode(PIN_LED_BLANCHE, OUTPUT);
    pinMode(PIN_LED_BUILTIN, OUTPUT);
    digitalWrite(PIN_LED_ROUGE, LOW);
    digitalWrite(PIN_LED_BLANCHE, LOW);
    digitalWrite(PIN_LED_BUILTIN, HIGH);

    // WiFi
    Serial.print("📶 Connexion à ");
    Serial.println(WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n❌ Échec WiFi !");
        return;
    }

    Serial.println("\n✅ WiFi connecté !");
    Serial.print("🌐 IP: ");
    Serial.println(WiFi.localIP());

    // ═══════════════════════════════════════
    // CONFIGURATION SERVEUR WEB (Port 80)
    // ═══════════════════════════════════════
    webServer.on("/", HTTP_GET, handleWebRoot);
    webServer.on("/api/temp", HTTP_GET, handleApiTemp);
    webServer.on("/api/led/rouge/on", HTTP_GET, handleApiLedRougeOn);
    webServer.on("/api/led/rouge/off", HTTP_GET, handleApiLedRougeOff);
    webServer.on("/api/led/blanche/on", HTTP_GET, handleApiLedBlancheOn);
    webServer.on("/api/led/blanche/off", HTTP_GET, handleApiLedBlancheOff);
    webServer.begin();
    Serial.println("🌐 Serveur Web démarré sur le port 80");

    // ═══════════════════════════════════════
    // CONFIGURATION SERVEUR MCP (Port 8080)
    // ═══════════════════════════════════════
    mcpServer.setSerialDebug(true, &Serial);

    mcpServer.registerTool("led_rouge_on", "Allume la LED rouge", mcp_ledRougeOn);
    mcpServer.registerTool("led_rouge_off", "Éteint la LED rouge", mcp_ledRougeOff);
    mcpServer.registerTool("led_blanche_on", "Allume la LED blanche", mcp_ledBlancheOn);
    mcpServer.registerTool("led_blanche_off", "Éteint la LED blanche", mcp_ledBlancheOff);
    mcpServer.registerTool("all_off", "Éteint toutes les LEDs", mcp_allOff);
    mcpServer.registerTool("get_status", "Récupère l'état complet du système", mcp_getStatus);
    mcpServer.registerTool("get_temperature", "Lit la température et l'humidité", mcp_getTemperature);

    mcpServer.registerTool("set_chauffage", "Active ou désactive le chauffage", mcp_setChauffage);
    mcpServer.addToolParam("actif", "true pour activer, false pour désactiver", "boolean", true);

    mcpServer.registerTool("clignoter", "Fait clignoter une LED", mcp_clignoter);
    mcpServer.addToolParam("couleur", "Couleur: rouge, blanche, builtin", "string", false);
    mcpServer.addToolParam("duree_ms", "Durée en millisecondes", "integer", false);
    mcpServer.addToolParam("repetitions", "Nombre de cycles", "integer", false);

    mcpServer.begin(8080);  // ← Port 8080 explicite !
    Serial.println("🚀 Serveur MCP démarré sur le port 8080");

    Serial.println("\n═══════════════════════════════════════════════");
    Serial.println("📱 Interface Web: http://" + WiFi.localIP().toString());
    Serial.println("🤖 Serveur MCP:   http://" + WiFi.localIP().toString() + ":8080");
    Serial.println("═══════════════════════════════════════════════");
}

void loop() {
    webServer.handleClient();    // Port 80 - Interface web
    mcpServer.handleClient();    // Port 8080 - Protocole MCP
    updateSensorData();          // Mise à jour capteurs

    // Heartbeat LED
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 3000) {
        lastBlink = millis();
        digitalWrite(PIN_LED_BUILTIN, !digitalRead(PIN_LED_BUILTIN));
    }
}

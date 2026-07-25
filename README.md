# Server_MCP

Bibliothèque C++ pour ESP8266 implémentant le **Model Context Protocol (MCP)** — un protocole ouvert standardisant la communication entre les modèles de langage (LLM) et les systèmes matériels.

> **Auteur** : Olivier Fournet  
> **Licence** : GPL-3.0  
> **Version** : 1.0.0  
> **Compatibilité** : ESP8266 (NodeMCU, Wemos D1, etc.) sous PlatformIO / Arduino Framework

---

## 📑 Table des matières

- [Qu'est-ce que MCP ?](#quest-ce-que-mcp-)
- [Architecture](#architecture)
- [Installation](#installation)
- [Utilisation rapide](#utilisation-rapide)
- [API détaillée](#api-détaillée)
- [Exemples](#exemples)
- [Configuration LM Studio](#configuration-lm-studio)
- [Dépannage](#dépannage)

---

## 🤖 Qu'est-ce que MCP ?

Le **Model Context Protocol (MCP)** est un protocole ouvert développé par Anthropic qui permet aux assistants IA (Claude, LM Studio, etc.) de communiquer avec des outils et ressources externes via JSON-RPC 2.0.

**Analogie** : MCP est comme un **port USB-C pour l'IA** — une interface standardisée pour connecter un LLM à n'importe quel système.

```
┌──────────────┐      JSON-RPC 2.0      ┌──────────────┐
│   LLM        │  ◄──────────────────►   │  ESP8266     │
│  (LM Studio) │   HTTP POST /mcp        │  (hardware)  │
└──────────────┘                         └──────────────┘
```

### Méthodes MCP supportées

| Méthode | Description |
|---------|-------------|
| `initialize` | Négociation initiale du protocole |
| `tools/list` | Liste les outils disponibles avec leur schéma JSON |
| `tools/call` | Appelle un outil avec des arguments |
| `resources/list` | Liste les ressources accessibles |
| `resources/read` | Lit le contenu d'une ressource |
| `ping` | Vérification de disponibilité |

---

## 🏗️ Architecture

```
ESP8266
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  ┌─────────────────┐        ┌─────────────────────────────┐ │
│  │  Serveur Web    │        │   Serveur MCP               │ │
│  │  Port 80        │        │   Port 8080 (par défaut)    │ │
│  │                 │        │                             │ │
│  │  GET /          │        │  POST /mcp                  │ │
│  │  → Dashboard    │        │  → JSON-RPC                 │ │
│  │                 │        │                             │ │
│  │  [Navigateur]   │        │  [LM Studio / LLM]          │ │
│  └─────────────────┘        └─────────────────────────────┘ │
│                                                             │
│              Partage des mêmes GPIO / capteurs              │
└─────────────────────────────────────────────────────────────┘
```

> **Important** : Le serveur MCP utilise un **port distinct** (8080 par défaut) du serveur web (80) pour éviter tout conflit.

---

## 📦 Installation

### 1. Dépendance PlatformIO

Ajoute dans `platformio.ini` :

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200

lib_deps =
    https://github.com/Fo170/Server_MCP.git@^1.0.0

board_build.ldscript = eagle.flash.4m2m.ld
upload_speed = 921600
```

La dépendance ArduinoJson 7.x est résolue automatiquement via `library.json`.

---

## 🚀 Utilisation rapide

```cpp
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "Server_MCP.h"

// Créer l'instance du serveur MCP
Server_MCP mcp("MonServeur", "1.0.0");

// Callback pour un outil
std::vector<MCPContent> allumerLED(const JsonObject& params) {
    digitalWrite(D5, HIGH);
    return { Server_MCP::makeTextContent("LED allumée !") };
}

void setup() {
    Serial.begin(115200);
    WiFi.begin("SSID", "PASSWORD");
    while (WiFi.status() != WL_CONNECTED) delay(500);

    // Activer le debug série
    mcp.setSerialDebug(true, &Serial);

    // Enregistrer un outil
    mcp.registerTool("led_on", "Allume la LED", allumerLED);

    // Démarrer le serveur sur le port 8080
    mcp.begin(8080);
}

void loop() {
    mcp.handleClient();  // Traiter les requêtes MCP
}
```

---

## 📚 API détaillée

### Constructeur

```cpp
Server_MCP(const String& serverName = "ESP8266-MCP",
           const String& serverVersion = "1.0.0",
           uint16_t maxTools = 16,
           uint16_t maxResources = 8);
```

| Paramètre | Type | Défaut | Description |
|-----------|------|--------|-------------|
| `serverName` | `String` | `"ESP8266-MCP"` | Nom du serveur affiché au client |
| `serverVersion` | `String` | `"1.0.0"` | Version du serveur |
| `maxTools` | `uint16_t` | `16` | Nombre maximum d'outils |
| `maxResources` | `uint16_t` | `8` | Nombre maximum de ressources |

### Configuration

#### `setServerInfo(name, version)`
Modifie les informations du serveur après création.

```cpp
mcp.setServerInfo("ESP-Thermostat", "2.0.0");
```

#### `setSerialDebug(enable, serial)`
Active les logs sur le port série.

```cpp
mcp.setSerialDebug(true, &Serial);  // Activer
mcp.setSerialDebug(false);          // Désactiver
```

### Enregistrement des outils

#### `registerTool(name, description, callback)`
Enregistre un nouvel outil accessible par le LLM.

```cpp
mcp.registerTool("temperature", "Lit la température du capteur", cb_temperature);
```

**Type du callback :**
```cpp
std::vector<MCPContent> maFonction(const JsonObject& params);
```

#### `addToolParam(name, description, type, required, defaultValue)`
Ajoute un paramètre au **dernier outil enregistré**.

```cpp
mcp.registerTool("regler_chauffage", "Règle la température", cb_chauffage);
mcp.addToolParam("temperature", "Température cible en °C", "integer", true);
mcp.addToolParam("mode", "Mode: eco, confort, boost", "string", false);
```

**Types supportés :** `"string"`, `"number"`, `"integer"`, `"boolean"`, `"array"`, `"object"`

#### `unregisterTool(name)`
Supprime un outil.

```cpp
mcp.unregisterTool("led_on");
```

### Enregistrement des ressources

#### `registerResource(uri, name, description, mimeType)`
Enregistre une ressource accessible en lecture.

```cpp
mcp.registerResource("sensor://temperature", "Température", "Valeur actuelle", "text/plain");
```

### Démarrage et gestion

#### `begin(port)`
Démarre le serveur HTTP MCP sur le port spécifié.

```cpp
mcp.begin(8080);   // Port recommandé (différent du web)
mcp.begin(3000);   // Ou tout autre port libre
```

> ⚠️ **Par défaut : 8080** — ne jamais utiliser 80 si un serveur web tourne déjà.

#### `handleClient()`
**À appeler dans `loop()`** pour traiter les requêtes entrantes.

```cpp
void loop() {
    mcp.handleClient();
}
```

#### `end()`
Arrête le serveur et libère la mémoire.

```cpp
mcp.end();
```

#### `isRunning()`
Retourne `true` si le serveur est actif.

```cpp
if (mcp.isRunning()) { /* ... */ }
```

#### `getPort()`
Retourne le port actuel du serveur MCP.

```cpp
uint16_t port = mcp.getPort();  // Ex: 8080
```

#### `getServerURL()`
Retourne l'URL complète du serveur.

```cpp
String url = mcp.getServerURL();  // "http://192.168.1.42:8080"
```

### Création de contenu (retour des callbacks)

#### `makeTextContent(text)`
Crée un contenu texte.

```cpp
return { Server_MCP::makeTextContent("Opération réussie") };
```

#### `makeImageContent(base64Data, mimeType)`
Crée un contenu image encodé en base64.

```cpp
return { Server_MCP::makeImageContent(base64String, "image/png") };
```

#### `makeResourceContent(uri, text, mimeType)`
Crée un contenu ressource.

```cpp
return { Server_MCP::makeResourceContent("doc://aide", "Contenu...", "text/markdown") };
```

---

## 💡 Exemples

### Exemple 1 : LED simple

```cpp
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "Server_MCP.h"

const char* WIFI_SSID = "MonWifi";
const char* WIFI_PASSWORD = "MonMotDePasse";

const int PIN_LED = D5;

Server_MCP mcp("ESP-LED", "1.0.0");

std::vector<MCPContent> ledOn(const JsonObject& params) {
    digitalWrite(PIN_LED, HIGH);
    return { Server_MCP::makeTextContent("✅ LED allumée") };
}

std::vector<MCPContent> ledOff(const JsonObject& params) {
    digitalWrite(PIN_LED, LOW);
    return { Server_MCP::makeTextContent("⬛ LED éteinte") };
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
```

### Exemple 2 : Paramètres avec validation

```cpp
std::vector<MCPContent> clignoterLED(const JsonObject& params) {
    // Paramètres automatiquement validés par le serveur MCP
    String couleur = params["couleur"].as<String>();      // "rouge" | "verte" | "bleue"
    int duree = params["duree_ms"].as<int>();               // Durée en ms
    int repetitions = params["repetitions"].as<int>();      // Nombre de cycles

    int pin = (couleur == "verte") ? D6 : (couleur == "bleue") ? D7 : D5;

    for (int i = 0; i < repetitions; i++) {
        digitalWrite(pin, HIGH);
        delay(duree);
        digitalWrite(pin, LOW);
        delay(duree);
    }

    String msg = "💡 Clignotement " + couleur + ": " + repetitions + " cycles";
    return { Server_MCP::makeTextContent(msg) };
}

void setup() {
    // ...
    mcp.registerTool("clignoter", "Fait clignoter une LED", clignoterLED);
    mcp.addToolParam("couleur", "Couleur de la LED", "string", true);
    mcp.addToolParam("duree_ms", "Durée d'un clignotement (ms)", "integer", false);
    mcp.addToolParam("repetitions", "Nombre de clignotements", "integer", false);
    // ...
}
```

### Exemple 3 : Serveur Web + MCP côte à côte

```cpp
#include <ESP8266WebServer.h>

ESP8266WebServer webServer(80);    // Interface utilisateur
Server_MCP mcpServer("ESP-MCP", "1.0.0");  // Port 8080 par défaut

void handleWebRoot() {
    webServer.send(200, "text/html", "<h1>Dashboard ESP8266</h1>");
}

void setup() {
    // ... connexion WiFi ...

    // Serveur Web sur port 80
    webServer.on("/", handleWebRoot);
    webServer.begin();

    // Serveur MCP sur port 8080
    mcpServer.registerTool("get_temp", "Lit la température", cb_temp);
    mcpServer.begin(8080);
}

void loop() {
    webServer.handleClient();    // Port 80
    mcpServer.handleClient();    // Port 8080
}
```

---

## ⚙️ Configuration LM Studio

### 1. Ouvrir la configuration MCP

Dans LM Studio : **Program** → **Install** → **Edit mcp.json**

### 2. Ajouter le serveur ESP8266

```json
{
  "mcpServers": {
    "esp8266-mcp": {
      "url": "http://192.168.1.XX:8080"
    }
  }
}
```

Remplace `192.168.1.XX` par l'IP affichée dans le Serial Monitor au démarrage de l'ESP8266.

### 3. Redémarrer LM Studio

Le serveur MCP est automatiquement détecté. Les outils apparaissent dans l'interface.

### 4. Utilisation

Tu peux maintenant demander au LLM :

> *"Allume la LED rouge"*
> *"Quelle est la température actuelle ?"*
> *"Fais clignoter la LED blanche 5 fois rapidement"*

---

## 🧪 Tests avec curl

### Vérifier le serveur

```bash
curl http://192.168.1.XX:8080/
```

### Lister les outils

```bash
curl -X POST http://192.168.1.XX:8080/mcp   -H "Content-Type: application/json"   -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'
```

### Appeler un outil

```bash
curl -X POST http://192.168.1.XX:8080/mcp   -H "Content-Type: application/json"   -d '{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "tools/call",
    "params": {
      "name": "led_on",
      "arguments": {}
    }
  }'
```

### Appeler avec paramètres

```bash
curl -X POST http://192.168.1.XX:8080/mcp   -H "Content-Type: application/json"   -d '{
    "jsonrpc": "2.0",
    "id": 3,
    "method": "tools/call",
    "params": {
      "name": "clignoter",
      "arguments": {
        "couleur": "rouge",
        "duree_ms": 300,
        "repetitions": 5
      }
    }
  }'
```

### Ping

```bash
curl -X POST http://192.168.1.XX:8080/mcp   -H "Content-Type: application/json"   -d '{"jsonrpc":"2.0","id":4,"method":"ping","params":{}}'
```

---

## 🔧 Dépannage

### Le serveur MCP ne démarre pas

| Symptôme | Cause probable | Solution |
|----------|---------------|----------|
| `Echec allocation serveur HTTP` | Mémoire insuffisante | Réduire `maxTools` / `maxResources` |
| Port déjà utilisé | Conflit avec un autre serveur | Changer de port (ex: `mcp.begin(3000)`) |
| `Serveur deja en cours d'execution` | Double appel à `begin()` | Vérifier avec `isRunning()` |

### Le LLM ne détecte pas les outils

1. Vérifier l'URL dans `mcp.json` : `http://IP:8080`
2. Tester avec `curl` la méthode `tools/list`
3. Vérifier que le pare-feu n'a pas bloqué le port 8080
4. Activer le debug série : `mcp.setSerialDebug(true, &Serial)`

### L'ESP8266 redémarre en boucle

- Réduire la taille des documents JSON (`StaticJsonDocument`)
- Diminuer `maxTools` à 8 ou 4
- Vérifier les fuites mémoire dans les callbacks

### Erreur "Tool not found"

- Vérifier que l'outil est bien enregistré **avant** `begin()`
- Le nom est sensible à la casse
- Vérifier les logs série pour le nom exact

---

## 📋 Spécifications techniques

| Caractéristique | Valeur |
|-----------------|--------|
| Protocole | JSON-RPC 2.0 |
| Transport | HTTP POST |
| Port par défaut | 8080 |
| Max outils | 16 (configurable) |
| Max ressources | 8 (configurable) |
| RAM requise | ~15 Ko |
| Dépendances | ArduinoJson 7.x |

---

## 🔗 Ressources

- [Spécification MCP officielle](https://modelcontextprotocol.io/)
- [ArduinoJson Documentation](https://arduinojson.org/)
- [ESP8266 Arduino Core](https://arduino-esp8266.readthedocs.io/)

---

## 📝 Changelog

### v1.0.0
- Implémentation complète du protocole MCP (JSON-RPC 2.0)
- Support des outils avec paramètres typés
- Support des ressources
- Port configurable (défaut: 8080)
- Debug série optionnel
- Validation automatique des paramètres

---

*Fait avec ❤️ pour la communauté maker*

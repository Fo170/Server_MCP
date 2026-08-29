# Server_MCP

Bibliothèque C++ pour ESP8266 implémentant le **Model Context Protocol (MCP)** — un protocole ouvert standardisant la communication entre les modèles de langage (LLM) et les systèmes matériels.

> **Auteur** : Olivier Fournet  
> **Licence** : GPL-3.0  
> **Version** : 1.2.0  
> **Compatibilité** : ESP8266 (NodeMCU, Wemos D1, etc.) et ESP32 (DevKit, Wemos D1 Mini ESP32, etc.) sous PlatformIO / Arduino Framework

---

## 🔧 Modifications locales — COPIE VENDORISÉE (coulomètre)

> ⚠️ **Cette copie (`lib/mcp/`) est celle UTILISÉE par le firmware du coulomètre** (WeMos Mini — `ESP8266WebServer`). Elle remplace la dépendance git `https://github.com/Fo170/Server_MCP.git` de `platformio.ini`. Elle ne doit **PAS** être remplacée par le repo GitHub d'origine (qui n'a pas les patches ci-dessous) et **PAS** être effacée : un `rm -rf .pio/libdeps/d1_mini` réinstallerait la version git NON patchée → MCP cassé (réponses vides).

### Patches locaux appliqués

| Date | Patch | Effet |
|------|-------|-------|
| **28/08/2026** | Contrôle de flux « style TCP » (fenêtre glissante, HTTP 429) | Évite l'engorgement quand un LLM rafale des requêtes |
| **28/08/2026** | Notifications JSON-RPC → HTTP 202 corps vide | Conforme spec MCP Streamable HTTP (LM Studio / SDK officiel) |
| **28/08/2026** | Envoi des réponses par chunks ≤ 1024 o (`setContentLength` + `sendContent`) | `tools/list` (~8 Ko) n'est plus tronqué au buffer TCP |
| **29/08/2026** | `_handleToolsCall()` : réponse construite en **UN SEUL `JsonDocument`** + `contents.clear()` avant l'envoi | Corrige l'**OOM des réponses > ~600 o** sur heap serré (~6-7 Ko) : `get_log_1h` / `get_log_h` du coulomètre renvoyaient **vide** |
| **29/08/2026** | `_handleToolsList()` : même pattern mono-document | Corrige `tools/list` (~2 Ko) qui saturait en OOM |
| **29/08/2026** | **Port par défaut 8081 → 8081** (`begin(port = 8081)` + `_port(8081)`) | Cohérence avec l'écosystème du coulomètre où **8081 = Domoticz** (`DOMOTICZ_PORT`) → pas d'ambiguïté si le port n'est pas passé explicitement |

**Pourquoi le patch 29/08 ?** L'ancien chemin construisait `finalDoc` + un 2ᵉ document dans `_sendResult()` (`response["result"] = result` = deep-copy) + la `String` de sortie, tout en gardant les `std::vector<MCPContent>` vivants → pic ≈ **3× la taille du texte**. Sur ESP8266 avec heap ~6-7 Ko (coulo entre MCP + web + SerialWeb), toute réponse log > ~600 o saturait → connexion coupée / `content: []`. Le patch construit la réponse entière dans **un seul document** et libère `contents` avant l'envoi → pic ≈ **1,5× le texte**.

**Synchronisation** : cette copie représente la **v1.2.0** (les **6** patches locaux y sont intégrés). Si `Fo170/Server_MCP` publie une v1.2.0 officielle, recopier le `Server_MCP.h` officiel dans `lib/mcp/` après vérification, ou fusionner.

---

## 🆕 Nouveautés v1.2.0

6 corrections intégrées dans cette version (dont les 6 patches locaux — voir section « Modifications locales ») :

1. **Notifications JSON-RPC → HTTP 202** — `_processJSONRPC()` répond **HTTP 202 corps vide** aux notifications (message SANS `id`), comme le veut la spec MCP Streamable HTTP. Sans cela, LM Studio / SDK officiel MCP lève *« Received an unexpected response to a notification »* et n'affiche pas les outils.
2. **Envoi des réponses par chunks ≤ 1024 o** — `_sendJSONResponse()` fixe le `Content-Length` puis streame par `sendContent()`. `WebServer::send(200, type, content)` écrivait tout le corps en un seul `write()` → tronqué au-delà du buffer TCP ESP32 (`CONFIG_LWIP_TCP_SND_BUF_DEFAULT` = 5760 o) ; `tools/list` (~8 Ko) était coupé → outils invisibles pour LM Studio.
3. **Contrôle de flux « style TCP »** — fenêtre de **canaux glissante** : `setFlowControl(maxRequests, windowMs)` (défaut `MCP_FLOW_MAX` = 8 / `MCP_FLOW_WINDOW_MS` = 30 s). Au-delà du max, la requête est **rejetée immédiatement en HTTP 429** (protocole MCP standard de rate-limiting) avec une erreur JSON-RPC « Trop de demandes MCP — canaux X/Y (saturé) ». État exposé via `flowMax()` / `flowUsed()` / `flowWindowMs()` / `flowEtat()` (états `libre`, `pris en compte`, `ralentir`, `saturé`) pour le monitoring web/série.
4. **`_handleToolsCall()` en UN SEUL `JsonDocument`** + `contents.clear()` avant l'envoi — corrige l'**OOM des réponses > ~600 o** sur heap serré (ESP8266 ~6-7 Ko) : l'ancien chemin gardait les `MCPContent` vivants jusqu'au deep-copy de `_sendResult()` → pic ≈ 3× le texte → réponses vides / connexion coupée.
5. **`_handleToolsList()` en UN SEUL `JsonDocument`** — idem, corrige l'OOM de `tools/list` (~2 Ko) sur heap serré.
6. **Port par défaut 8081 → 8081** — cohérence avec l'écosystème du coulomètre (8081 = Domoticz) : `begin(port = 8081)` + `_port(8081)`. Le firmware du coulomètre passe 8081 explicitement ; ce défaut évite toute ambiguïté.

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
│  │  Port 80        │        │   Port 8081 (par défaut)    │ │
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

> **Important** : Le serveur MCP utilise un **port distinct** (8081 par défaut) du serveur web (80) pour éviter tout conflit.

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
    https://github.com/Fo170/Server_MCP.git@^1.2.0

board_build.ldscript = eagle.flash.4m2m.ld
upload_speed = 921600
```

La dépendance ArduinoJson 7.x est résolue automatiquement via `library.json`.

---

## 🚀 Utilisation rapide

```cpp
#include <Arduino.h>
#include <Server_MCP.h>
```

La classe serveur HTTP est choisie automatiquement selon la plateforme (`WebServer` sur ESP32, `ESP8266WebServer` sur ESP8266). Pour une plateforme non reconnue, définir `SERVER_MCP_WEBSERVER` (et inclure le WiFi) avant le `#include`.

```cpp
// Créer l'instance du serveur MCP
Server_MCP mcp("MonServeur", "1.2.0");

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

    // Démarrer le serveur sur le port 8081
    mcp.begin(8081);
}

void loop() {
    mcp.handleClient();  // Traiter les requêtes MCP
}
```

---

## 📚 API détaillée

### Constructeur

```cpp
Server_MCP(const String& serverName = "Server-MCP",
           const String& serverVersion = "1.2.0",
           uint16_t maxTools = 16,
           uint16_t maxResources = 8);
```

| Paramètre | Type | Défaut | Description |
|-----------|------|--------|-------------|
| `serverName` | `String` | `"Server-MCP"` | Nom du serveur affiché au client |
| `serverVersion` | `String` | `"1.2.0"` | Version du serveur |
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
mcp.begin(8081);   // Port recommandé (différent du web)
mcp.begin(3000);   // Ou tout autre port libre
```

> ⚠️ **Par défaut : 8081** — ne jamais utiliser 80 si un serveur web tourne déjà.

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
uint16_t port = mcp.getPort();  // Ex: 8081
```

#### `getServerURL()`
Retourne l'URL complète du serveur.

```cpp
String url = mcp.getServerURL();  // "http://192.168.1.42:8081"
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
#if defined(ESP32)
  #include <WiFi.h>
  #define PIN_LED 2          // GPIO2 : LED intégrée de la plupart des dev boards ESP32
#else
  #include <ESP8266WiFi.h>
  #define PIN_LED D5         // GPIO14
#endif
#include <Server_MCP.h>

const char* WIFI_SSID = "MonWifi";
const char* WIFI_PASSWORD = "MonMotDePasse";

Server_MCP mcp("ESP-LED", "1.2.0");

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

    mcp.begin(8081);
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
#if defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #define SERVER_WEB WebServer
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #define SERVER_WEB ESP8266WebServer
#endif

SERVER_WEB webServer(80);    // Interface utilisateur
Server_MCP mcpServer("ESP-MCP", "1.2.0");  // Port 8081 par défaut

void handleWebRoot() {
    webServer.send(200, "text/html", "<h1>Dashboard ESP8266</h1>");
}

void setup() {
    // ... connexion WiFi ...

    // Serveur Web sur port 80
    webServer.on("/", handleWebRoot);
    webServer.begin();

    // Serveur MCP sur port 8081
    mcpServer.registerTool("get_temp", "Lit la température", cb_temp);
    mcpServer.begin(8081);
}

void loop() {
    webServer.handleClient();    // Port 80
    mcpServer.handleClient();    // Port 8081
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
      "url": "http://192.168.1.XX:8081"
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
curl http://192.168.1.XX:8081/
```

### Lister les outils

```bash
curl -X POST http://192.168.1.XX:8081/mcp   -H "Content-Type: application/json"   -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'
```

### Appeler un outil

```bash
curl -X POST http://192.168.1.XX:8081/mcp   -H "Content-Type: application/json"   -d '{
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
curl -X POST http://192.168.1.XX:8081/mcp   -H "Content-Type: application/json"   -d '{
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
curl -X POST http://192.168.1.XX:8081/mcp   -H "Content-Type: application/json"   -d '{"jsonrpc":"2.0","id":4,"method":"ping","params":{}}'
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

1. Vérifier l'URL dans `mcp.json` : `http://IP:8081`
2. Tester avec `curl` la méthode `tools/list`
3. Vérifier que le pare-feu n'a pas bloqué le port 8081
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
| Port par défaut | 8081 |
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

### v1.2.0
- **Réponses en UN SEUL `JsonDocument`** (`_handleToolsCall` + `_handleToolsList`) + `contents.clear()` avant l'envoi — corrige l'**OOM des réponses > ~600 o** sur heap serré ESP8266 (~6-7 Ko) : `get_log_1h` / `get_log_h` / `tools/list` renvoyaient **vide / connexion coupée** (pic mémoire ramené de ~3× à ~1,5× le texte)
- **Port par défaut 8080 → 8081** (`begin(port = 8081)` + `_port(8081)`) — cohérence avec l'écosystème du coulomètre (8080 = Domoticz)
- Version des patches locaux 28/08 (HTTP 202, chunks ≤ 1024 o, contrôle de flux 429) désormais intégrée à la version

### v1.1.2
- **Notifications JSON-RPC** : réponse **HTTP 202 corps vide** aux messages sans `id` (conforme spec MCP Streamable HTTP) — corrige *« Received an unexpected response to a notification »* sous LM Studio / SDK officiel
- **Envoi chunké des réponses** (`_sendJSONResponse`) : `setContentLength()` + `sendContent()` par blocs ≤ 1024 o — corrige la troncature de `tools/list` (~8 Ko) au-delà du buffer TCP ESP32 (5760 o)
- **Contrôle de flux** : fenêtre de canaux glissante `setFlowControl(maxRequests, windowMs)` (défaut 8 / 30 s), rejet en **HTTP 429** au-delà du max ; état exposé via `flowMax()` / `flowUsed()` / `flowWindowMs()` / `flowEtat()`

### v1.1.1
- Migration du code vers l'**API ArduinoJson v7** (`JsonDocument`, `add<JsonObject>()`, `to<JsonObject>()`) — suppression des appels dépréciés v6 (`StaticJsonDocument`, `createNestedObject()`, `containsKey()`)

### v1.1.0
- Compatibilité **ESP32** (classe serveur `WebServer` du core) et **ESP8266** (`ESP8266WebServer`)
- Sélection automatique de la plateforme ; macro `SERVER_MCP_WEBSERVER` pour toute autre carte
- Exemples : environnement `esp32dev` ajouté aux 4 projets

### v1.0.0
- Implémentation complète du protocole MCP (JSON-RPC 2.0)
- Support des outils avec paramètres typés
- Support des ressources
- Port configurable (défaut: 8081)
- Debug série optionnel
- Validation automatique des paramètres

---

*Fait avec ❤️ pour la communauté maker*

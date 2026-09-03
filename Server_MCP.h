// ============================================================================
// Server_MCP.h - version 1.3.2
// Bibliotheque C++ MCP pour ESP8266/ESP32 — Port configurable
// ============================================================================

#ifndef SERVER_MCP_H
#define SERVER_MCP_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>
#include <map>

// ────────────────────────────────────────────────────────────────────────────
// Sélection de la plateforme (ESP8266 / ESP32 / autre)
// Pour une plateforme non reconnue, définir SERVER_MCP_WEBSERVER (et inclure
// le WiFi) avant le #include de ce fichier.
// ────────────────────────────────────────────────────────────────────────────
#if defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #define SERVER_MCP_WEBSERVER WebServer
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #define SERVER_MCP_WEBSERVER ESP8266WebServer
#else
  #ifndef SERVER_MCP_WEBSERVER
    #error "Server_MCP: plateforme non supportee. Definir SERVER_MCP_WEBSERVER (et inclure le WiFi) avant le #include."
  #endif
#endif

#define MCP_PROTOCOL_VERSION "2024-11-05"

#define MCP_MAX_BODY        8192        // v1.3.2 : taille max d'une requête JSON-RPC (anti-OOM sur heap serré)

#define JSONRPC_PARSE_ERROR      (-32700)
#define JSONRPC_INVALID_REQUEST  (-32600)
#define JSONRPC_METHOD_NOT_FOUND (-32601)
#define JSONRPC_INVALID_PARAMS   (-32602)
#define JSONRPC_INTERNAL_ERROR   (-32603)
#define JSONRPC_SERVER_ERROR     (-32000)

// Contrôle de flux "style TCP" (28/08/2026, v1.1.2) — fenêtre de CANAUX
// glissante. Trop de requêtes en rafale empile des appels bloquants (Domoticz
// ~3 s) dans loop_Core_1 et étrangle le polling Telegram/web. Au-delà de
// MCP_FLOW_MAX requêtes par fenêtre MCP_FLOW_WINDOW_MS, la requête est REJETÉE
// en HTTP 429 (protocole MCP standard de rate-limiting). Valeurs surchargées à
// l'exécution via setFlowControl() (appelée depuis INIT_MCP() du projet).
#define MCP_FLOW_MAX        8          // canaux max par fenêtre (défaut strict)
#define MCP_FLOW_WINDOW_MS  30000UL    // fenêtre glissante (défaut 30 s)

// Canaux pondérés + backpressure mémoire (31/08/2026, v1.3.0) :
//  - canaux PONDÉRÉS : chaque outil a un poids (poids 1 léger ... poids 3 lourd
//    Domoticz). Une rafale d'outils lourds sature la fenêtre plus vite → rejet
//    HTTP 429 AVANT tout traitement bloquant (garantie anti-crash).
//  - backpressure MÉMOIRE (XON/XOFF) : setFlowPression(callback, maxSousPression).
//    Si le callback signale une pression (heap bas/fragmenté), le max effectif
//    chute à MCP_FLOW_MAX_SOUS_PRESSION (seuls les outils légers passent) ;
//    retour au max normal quand ça va mieux.
//  - le 429 porte les en-têtes standards Retry-After + X-RateLimit-* pour
//    avertir explicitement le client (LM Studio...) de patienter.
//  - seuls les appels d'OUTIL (tools/call) consomment des canaux : ping,
//    initialize, tools/list restent gratuits (handshake non limité).
#define MCP_FLOW_MAX_SOUS_PRESSION  2   // max effectif quand la pression mémoire est haute (XOFF)

enum MCPContentType {
    MCP_CONTENT_TEXT,
    MCP_CONTENT_IMAGE,
    MCP_CONTENT_RESOURCE
};

struct MCPContent {
    MCPContentType type;
    String text;
    String mimeType;
    String data;
    String uri;
};

struct MCPParam {
    String name;
    String description;
    String type;
    bool required;
};

struct MCPTool {
    String name;
    String description;
    std::vector<MCPParam> params;
};

using MCPToolCallback = std::function<std::vector<MCPContent>(const JsonObject& params)>;

// v1.3.2 : représentation textuelle d'un id JSON-RPC (number | string) — logs.
static inline String _mcpIdToStr(const JsonVariant& id) {
    if (id.isNull()) return String("");
    if (id.is<bool>()) return id.as<bool>() ? "true" : "false";
    return id.as<String>();
}

// ============================================================================
// CLASSE PRINCIPALE
// ============================================================================

class Server_MCP {
public:
    Server_MCP(const String& serverName = "Server-MCP",
               const String& serverVersion = "1.3.2",
               uint16_t maxTools = 16,
               uint16_t maxResources = 8);
    ~Server_MCP();

    void setServerInfo(const String& name, const String& version);
    void setSerialDebug(bool enable, HardwareSerial* serial = &Serial);

    bool registerTool(const String& name,
                      const String& description,
                      MCPToolCallback callback,
                      uint8_t poids = 1);
    bool addToolParam(const String& name,
                      const String& description,
                      const String& type = "string",
                      bool required = true);
    bool unregisterTool(const String& name);

    bool registerResource(const String& uri,
                          const String& name,
                          const String& description,
                          const String& mimeType = "text/plain");

    // ═════════════════════════════════════════════════════════════════
    // DEMARRAGE AVEC PORT PERSONNALISE
    // ═════════════════════════════════════════════════════════════════
    bool begin(uint16_t port = 8081);  // défaut 8081 (29/08/2026, v1.2.0 : Domoticz=8080), pas 80 !
    
    void handleClient();
    void end();
    bool isRunning() const;
    uint16_t getPort() const;
    String getServerURL() const;

    // Contrôle de flux (fenêtre de canaux glissante).
    void setFlowControl(uint16_t maxRequests, uint32_t windowMs);
    // v1.3.0 : backpressure mémoire (XON/XOFF) — callback "pression" (ex. heap bas).
    void setFlowPression(bool (*cb)(), uint16_t maxSousPression = MCP_FLOW_MAX_SOUS_PRESSION);
    uint16_t flowMax() const;
    uint16_t flowMaxEffectif() const;   // v1.3.0 : max courant (pression ? réduit : normal)
    uint16_t flowUsed() const;
    uint16_t flowUsedPonderes() const;  // v1.3.0 : canaux consommés pondérés (jamais > maxEff)
    uint32_t flowWindowMs() const;
    uint32_t flowRetryAfterMs() const;  // v1.3.0 : ms avant libération d'un canal (429)
    String flowEtat() const;

    static MCPContent makeTextContent(const String& text);
    static MCPContent makeImageContent(const String& base64Data, const String& mimeType = "image/png");
    static MCPContent makeResourceContent(const String& uri, const String& text, const String& mimeType = "text/plain");

private:
    String _serverName;
    String _serverVersion;
    bool _running;
    bool _debugEnabled;
    HardwareSerial* _debugSerial;

    SERVER_MCP_WEBSERVER* _server;
    uint16_t _port;

    struct ToolEntry {
        MCPTool definition;
        MCPToolCallback callback;
        uint8_t poids;   // v1.3.0 : canaux consommés par appel (léger=1 ... lourd=3)
    };
    std::vector<ToolEntry> _tools;
    uint16_t _maxTools;

    struct ResourceEntry {
        String uri;
        String name;
        String description;
        String mimeType;
    };
    std::vector<ResourceEntry> _resources;
    uint16_t _maxResources;

    // Contrôle de flux (fenêtre de canaux glissante).
    uint16_t _flowMax;
    uint32_t _flowWindowMs;
    uint32_t _flowWinStart;
    uint16_t _flowUsed;           // canaux consommés PONDÉRÉS (v1.3.0)
    uint8_t  _flowEtatPrec;       // dernier état annoncé (pour log des transitions)
    bool (*_flowPressionCb)();    // v1.3.0 : callback de pression mémoire (nullptr = aucune)
    uint16_t _flowPressionMax;    // v1.3.0 : max effectif sous pression (XOFF)
    bool _suppressResponse;       // v1.3.2 : true = notification — exécuter sans réponse JSON-RPC

    void _handleRoot();
    void _handleMCP();
    void _handleSSE();
    void _handleNotFound();

    void _processJSONRPC(const String& body);
    void _handleInitialize(const JsonObject& params, const JsonVariant& id);
    void _handleToolsList(const JsonVariant& id);
    void _handleToolsCall(const JsonObject& params, const JsonVariant& id);
    void _handleResourcesList(const JsonVariant& id);
    void _handleResourcesRead(const JsonObject& params, const JsonVariant& id);
    void _handlePing(const JsonVariant& id);

    void _sendResult(const JsonVariant& id, const JsonObject& result);
    void _sendError(const JsonVariant& id, int code, const String& message, const JsonObject* data = nullptr);
    void _sendError429(const JsonVariant& id, int code, const String& message, uint32_t retryS);
    void _sendJSONResponse(const JsonObject& response, uint16_t statusCode = 200);
    bool _flowConsume(uint8_t poids);   // consomme `poids` canaux (rejet si fenêtre pleine)
    uint16_t _flowMaxEffectif() const;  // v1.3.0 : max courant (pression ? réduit : normal)

    void _log(const String& msg);
    void _logError(const String& msg);
    String _getContentTypeString(MCPContentType type);
    int _findToolIndex(const String& name);
    bool _validateParams(const JsonObject& params, const MCPTool& tool, String& errorMsg);
    void _buildToolSchema(const MCPTool& tool, JsonObject& schema);
    void _serializeContents(const std::vector<MCPContent>& contents, JsonArray& array);
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

inline Server_MCP::Server_MCP(const String& serverName,
                               const String& serverVersion,
                               uint16_t maxTools,
                               uint16_t maxResources)
    : _serverName(serverName)
    , _serverVersion(serverVersion)
    , _running(false)
    , _debugEnabled(false)
    , _debugSerial(nullptr)
    , _server(nullptr)
    , _port(8081)   // défaut 8081 (29/08/2026, v1.2.0) — Domoticz=8080, cohérent avec l'écosystème
    , _maxTools(maxTools)
    , _maxResources(maxResources)
    , _flowMax(MCP_FLOW_MAX)
    , _flowWindowMs(MCP_FLOW_WINDOW_MS)
    , _flowWinStart(0)
    , _flowUsed(0)
    , _flowEtatPrec(0)
    , _flowPressionCb(nullptr)
    , _flowPressionMax(MCP_FLOW_MAX_SOUS_PRESSION)
    , _suppressResponse(false)
{
    _tools.reserve(maxTools);
    _resources.reserve(maxResources);
}

inline Server_MCP::~Server_MCP() {
    end();
}

inline void Server_MCP::setServerInfo(const String& name, const String& version) {
    _serverName = name;
    _serverVersion = version;
}

inline void Server_MCP::setSerialDebug(bool enable, HardwareSerial* serial) {
    _debugEnabled = enable;
    _debugSerial = serial;
}

inline bool Server_MCP::registerTool(const String& name,
                                      const String& description,
                                      MCPToolCallback callback,
                                      uint8_t poids) {
    if (_tools.size() >= _maxTools) {
        _logError("Nombre max d'outils atteint: " + String(_maxTools));
        return false;
    }
    if (_findToolIndex(name) >= 0) {
        _logError("Outil deja enregistre: " + name);
        return false;
    }
    ToolEntry entry;
    entry.definition.name = name;
    entry.definition.description = description;
    entry.callback = callback;
    entry.poids = (poids == 0) ? 1 : poids;
    if (entry.poids > _flowMax) {
        _logError("Attention: poids " + String(entry.poids) + " > max flux " +
                  String(_flowMax) + " — outil jamais executable (setFlowControl()).");
    }
    _tools.push_back(entry);
    _log("Outil enregistre: " + name + " (poids " + String(entry.poids) + ")");
    return true;
}

inline bool Server_MCP::addToolParam(const String& name,
                                      const String& description,
                                      const String& type,
                                      bool required) {
    if (_tools.empty()) {
        _logError("Aucun outil enregistre. Appeler registerTool() d'abord.");
        return false;
    }
    MCPParam param;
    param.name = name;
    param.description = description;
    param.type = type;
    param.required = required;
    _tools.back().definition.params.push_back(param);
    return true;
}

inline bool Server_MCP::unregisterTool(const String& name) {
    int idx = _findToolIndex(name);
    if (idx < 0) return false;
    _tools.erase(_tools.begin() + idx);
    _log("Outil supprime: " + name);
    return true;
}

inline bool Server_MCP::registerResource(const String& uri,
                                          const String& name,
                                          const String& description,
                                          const String& mimeType) {
    if (_resources.size() >= _maxResources) {
        _logError("Nombre max de ressources atteint");
        return false;
    }
    ResourceEntry entry;
    entry.uri = uri;
    entry.name = name;
    entry.description = description;
    entry.mimeType = mimeType;
    _resources.push_back(entry);
    _log("Ressource enregistree: " + uri);
    return true;
}

inline bool Server_MCP::begin(uint16_t port) {
    if (_running) {
        _log("Serveur deja en cours d'execution sur le port " + String(_port));
        return true;
    }
    _port = port;
    _server = new SERVER_MCP_WEBSERVER(port);
    if (!_server) {
        _logError("Echec allocation serveur HTTP");
        return false;
    }
    _server->on("/", HTTP_GET, [this]() { _handleRoot(); });
    _server->on("/", HTTP_POST, [this]() { _handleMCP(); });
    _server->on("/mcp", HTTP_POST, [this]() { _handleMCP(); });
    _server->on("/sse", HTTP_GET, [this]() { _handleSSE(); });
    _server->onNotFound([this]() { _handleNotFound(); });
    _server->begin();
    _running = true;
    _log("Serveur MCP demarre sur le port " + String(port));
    _log("URL MCP: " + getServerURL());
    return true;
}

inline void Server_MCP::handleClient() {
    if (_server && _running) {
        _server->handleClient();
    }
}

inline void Server_MCP::end() {
    if (_server) {
        _server->stop();
        delete _server;
        _server = nullptr;
    }
    _running = false;
    _log("Serveur MCP arrete");
}

inline bool Server_MCP::isRunning() const {
    return _running;
}

inline uint16_t Server_MCP::getPort() const {
    return _port;
}

inline String Server_MCP::getServerURL() const {
    if (!_running) return "";
    return "http://" + WiFi.localIP().toString() + ":" + String(_port);
}

// ═════════════════════════════════════════════════════════════════
// Contrôle de flux (fenêtre de canaux glissante)
// ═════════════════════════════════════════════════════════════════
inline void Server_MCP::setFlowControl(uint16_t maxRequests, uint32_t windowMs) {
    _flowMax = maxRequests;
    _flowWindowMs = windowMs;
    _flowWinStart = 0;
    _flowUsed = 0;
    _flowEtatPrec = 0;
    _log("Controle de flux: max " + String(_flowMax) + " requetes / " +
         String(_flowWindowMs / 1000) + " s");
}

// v1.3.0 : backpressure mémoire (XON/XOFF). cb() appelé à chaque tools/call ;
// s'il renvoie true (pression : heap bas/fragmenté), le max effectif chute à
// maxSousPression → seuls les outils dont le poids le permet passent.
inline void Server_MCP::setFlowPression(bool (*cb)(), uint16_t maxSousPression) {
    _flowPressionCb = cb;
    if (maxSousPression > 0) _flowPressionMax = maxSousPression;
    _flowEtatPrec = 0;
    _log("Controle de flux: pression memoire " +
         String(_flowPressionCb ? "ACTIVEE (max " + String(_flowPressionMax) + " sous pression)"
                                : "desactivee"));
}

inline uint16_t Server_MCP::flowMax() const      { return _flowMax; }

// v1.3.0 : max effectif courant (réduit sous pression mémoire = XOFF).
inline uint16_t Server_MCP::_flowMaxEffectif() const {
    if (_flowPressionCb && _flowPressionCb()) return _flowPressionMax;
    return _flowMax;
}

inline uint16_t Server_MCP::flowMaxEffectif() const { return _flowMaxEffectif(); }

// canaux utilisés EFFECTIFS pour l'affichage (0 si la fenêtre est expirée —
// le reset est paresseux côté requête, mais le monitoring doit être à jour).
inline uint16_t Server_MCP::flowUsed() const {
    uint32_t now = millis();
    if (_flowWindowMs == 0 || (now - _flowWinStart >= _flowWindowMs) || now < _flowWinStart)
        return 0;
    return _flowUsed;
}

// v1.3.0 : canaux pondérés réellement consommés (borné au max effectif courant).
inline uint16_t Server_MCP::flowUsedPonderes() const {
    uint16_t used = flowUsed();
    uint16_t maxEff = flowMaxEffectif();
    return (used > maxEff) ? maxEff : used;
}

inline uint32_t Server_MCP::flowWindowMs() const { return _flowWindowMs; }

// v1.3.0 : ms restantes avant libération de canaux (pour l'en-tête Retry-After).
inline uint32_t Server_MCP::flowRetryAfterMs() const {
    uint32_t now = millis();
    if (_flowWinStart == 0) return 0;
    if (now < _flowWinStart) return _flowWindowMs;   // wrap de millis()
    uint32_t ecoule = now - _flowWinStart;
    if (ecoule >= _flowWindowMs) return 0;
    return _flowWindowMs - ecoule;
}

// État annoncé (monitoring web/série) : libre / pris en compte / ralentir / sature.
inline String Server_MCP::flowEtat() const {
    uint16_t maxEff = _flowMaxEffectif();
    if (maxEff == 0) return "libre";
    uint32_t now = millis();
    uint16_t used = ((_flowWindowMs == 0) || (now - _flowWinStart >= _flowWindowMs) ||
                     (now < _flowWinStart)) ? 0 : _flowUsed;
    float r = (float)used / (float)maxEff;
    String base;
    if (r < 0.5f)       base = "libre";
    else if (r < 0.8f)  base = "pris en compte";
    else if (r < 1.0f)  base = "ralentir";
    else                base = "sature";
    // XOFF actif (pression mémoire) → l'état le signale.
    if (_flowPressionCb && _flowPressionCb()) base += " (XOFF memoire)";
    return base;
}

// Consomme `poids` canaux de la fenêtre glissante. Renvoie false si la fenêtre
// est pleine (trop de requêtes) — la requête est alors rejetée en HTTP 429.
// ⚠️ v1.3.0 : canaux PONDÉRÉS — un outil lourd (poids 3) épuise la fenêtre 3×
// plus vite qu'un outil léger.
// v1.3.2 : un REFUS ne modifie jamais l'état du flux (ni _flowUsed ni
// _flowWinStart) → pas de faux Retry-After sur un rejet « structurel ».
inline bool Server_MCP::_flowConsume(uint8_t poids) {
    uint32_t now = millis();
    bool expired = (_flowWindowMs == 0) ||
                   (now - _flowWinStart >= _flowWindowMs) ||
                   (now < _flowWinStart);   // wrap de millis()
    uint16_t maxEff = _flowMaxEffectif();
    uint16_t used = expired ? 0 : _flowUsed;
    if (_flowWindowMs != 0 && (uint32_t)used + poids > maxEff) return false;  // refus : état intact
    // Acceptation : démarre une nouvelle fenêtre si besoin, puis consomme.
    if (expired) {
        _flowWinStart = now;
        _flowUsed = 0;
    }
    _flowUsed += poids;
    // Log des TRANSITIONS d'état (libre=1, pris en compte=2, ralentir=3, sature=4)
    uint8_t etat = (maxEff == 0) ? 1
                 : ((float)_flowUsed / maxEff < 0.5f) ? 1
                 : ((float)_flowUsed / maxEff < 0.8f) ? 2
                 : ((float)_flowUsed / maxEff < 1.0f) ? 3 : 4;
    if (etat != _flowEtatPrec) {
        _flowEtatPrec = etat;
        _log("Flux MCP: " + flowEtat() + " (" + String(_flowUsed) + "/" + String(maxEff) + ")");
    }
    return true;
}

inline MCPContent Server_MCP::makeTextContent(const String& text) {
    MCPContent c;
    c.type = MCP_CONTENT_TEXT;
    c.text = text;
    return c;
}

inline MCPContent Server_MCP::makeImageContent(const String& base64Data, const String& mimeType) {
    MCPContent c;
    c.type = MCP_CONTENT_IMAGE;
    c.mimeType = mimeType;
    c.data = base64Data;
    return c;
}

inline MCPContent Server_MCP::makeResourceContent(const String& uri, const String& text, const String& mimeType) {
    MCPContent c;
    c.type = MCP_CONTENT_RESOURCE;
    c.uri = uri;
    c.text = text;
    c.mimeType = mimeType;
    return c;
}

inline void Server_MCP::_handleRoot() {
    JsonDocument doc;
    JsonObject response = doc.to<JsonObject>();
    response["name"] = _serverName;
    response["version"] = _serverVersion;
    response["protocol"] = "MCP";
    response["protocolVersion"] = MCP_PROTOCOL_VERSION;
    response["status"] = "online";
    response["url"] = getServerURL();
    response["port"] = _port;
    JsonObject transports = response["transports"].to<JsonObject>();
    transports["http"] = true;
    transports["sse"] = true;
    String output;
    serializeJson(response, output);
    _server->send(200, "application/json", output);
}

inline void Server_MCP::_handleMCP() {
    if (_server->method() != HTTP_POST) {
        _server->send(405, "text/plain", "Method Not Allowed");
        return;
    }
    String body = _server->arg("plain");
    if (body.length() > MCP_MAX_BODY) {   // v1.3.2 : plafond anti-OOM
        _server->send(413, "application/json",
            "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32600,\"message\":\"Payload too large\"}}");
        return;
    }
    _log("Requete MCP: " + body.substring(0, 128));
    _processJSONRPC(body);
}

inline void Server_MCP::_handleSSE() {
    _server->send(200, "text/event-stream",
        "event: endpoint\ndata: /mcp\n\n");
}

inline void Server_MCP::_handleNotFound() {
    JsonDocument doc;
    JsonObject response = doc.to<JsonObject>();
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = JSONRPC_METHOD_NOT_FOUND;
    response["error"]["message"] = "Endpoint not found";
    String output;
    serializeJson(response, output);
    _server->send(404, "application/json", output);
}

inline void Server_MCP::_processJSONRPC(const String& body) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        _sendError(JsonVariant(), JSONRPC_PARSE_ERROR, "Parse error: " + String(error.c_str()));
        return;
    }
    JsonObject root = doc.as<JsonObject>();
    const char* jsonrpc = root["jsonrpc"];
    if (!jsonrpc || strcmp(jsonrpc, "2.0") != 0) {
        _sendError(JsonVariant(), JSONRPC_INVALID_REQUEST, "Invalid JSON-RPC version");
        return;
    }

    // v1.3.2 : écho de l'id JSON-RPC TEL QUEL (number | string) — pas de
    // conversion vers uint32_t qui cassait les ids string/négatifs/grands.
    bool notification = root["id"].isNull();
    JsonVariant id = root["id"];

    // v1.3.2 : notification (SANS "id") → la méthode est EXÉCUTÉE (effet de
    // bord, contrôle de flux compris) mais AUCUNE réponse JSON-RPC n'est émise
    // (_suppressResponse). On répond HTTP 202 corps vide (spec MCP Streamable
    // HTTP), sinon LM Studio / SDK officiel lèvent "Received an unexpected
    // response to a notification".
    if (notification) _suppressResponse = true;

    // v1.3.0 : le contrôle de flux ne s'applique QU'AUX appels d'outil
    // (tools/call) — ping/initialize/tools/list/resources restent gratuits
    // (le handshake ne doit pas consommer la fenêtre). Le rejet pondéré est
    // fait dans _handleToolsCall (poids de l'outil).

    const char* method = root["method"];
    if (!method) {
        _sendError(id, JSONRPC_INVALID_REQUEST, "Missing method");   // supprimé si notification
    } else {
        String methodStr = String(method);
        _log("Methode: " + methodStr + " (id=" + _mcpIdToStr(id) + ")");
        JsonObject params = root["params"].as<JsonObject>();
        if (methodStr == "initialize") _handleInitialize(params, id);
        else if (methodStr == "tools/list") _handleToolsList(id);
        else if (methodStr == "tools/call") _handleToolsCall(params, id);
        else if (methodStr == "resources/list") _handleResourcesList(id);
        else if (methodStr == "resources/read") _handleResourcesRead(params, id);
        else if (methodStr == "ping") _handlePing(id);
        else _sendError(id, JSONRPC_METHOD_NOT_FOUND, "Method not found: " + methodStr);
    }

    if (notification) {
        _suppressResponse = false;
        if (_server) _server->send(202, "application/json", "");
    }
}

inline void Server_MCP::_handleInitialize(const JsonObject& params, const JsonVariant& id) {
    JsonDocument resultDoc;
    JsonObject result = resultDoc.to<JsonObject>();
    result["protocolVersion"] = MCP_PROTOCOL_VERSION;
    JsonObject serverInfo = result["serverInfo"].to<JsonObject>();
    serverInfo["name"] = _serverName;
    serverInfo["version"] = _serverVersion;
    JsonObject capabilities = result["capabilities"].to<JsonObject>();
    capabilities["tools"].to<JsonObject>();
    if (_resources.size() > 0) {
        capabilities["resources"].to<JsonObject>();
    }
    _sendResult(id, result);
    _log("Initialisation reussie");
}

inline void Server_MCP::_handleToolsList(const JsonVariant& id) {
    // Réponse construite dans UN SEUL JsonDocument (29/08/2026, v1.2.0 —
    // comme _handleToolsCall) : l'ancien _sendResult deep-copait le result dans
    // un 2ᵉ doc → pic ~2× la taille (tools/list ≈ 2 Ko) → OOM sur ESP8266.
    JsonDocument doc;
    JsonObject response = doc.to<JsonObject>();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    JsonObject result = response["result"].to<JsonObject>();
    JsonArray tools = result["tools"].to<JsonArray>();
    for (const auto& entry : _tools) {
        JsonObject toolObj = tools.add<JsonObject>();
        toolObj["name"] = entry.definition.name;
        toolObj["description"] = entry.definition.description;
        JsonObject inputSchema = toolObj["inputSchema"].to<JsonObject>();
        _buildToolSchema(entry.definition, inputSchema);
    }
    _sendJSONResponse(response);
}

inline void Server_MCP::_handleToolsCall(const JsonObject& params, const JsonVariant& id) {
    if (params["name"].isNull()) {
        _sendError(id, JSONRPC_INVALID_PARAMS, "Missing tool name");
        return;
    }
    String toolName = params["name"].as<String>();
    int idx = _findToolIndex(toolName);
    if (idx < 0) {
        _sendError(id, JSONRPC_METHOD_NOT_FOUND, "Tool not found: " + toolName);
        return;
    }

    // v1.3.2 : validation des paramètres AVANT la consommation de canaux —
    // un appel mal formé ne grignote pas la fenêtre de flux.
    JsonObject arguments = params["arguments"].as<JsonObject>();
    String errorMsg;
    if (!_validateParams(arguments, _tools[idx].definition, errorMsg)) {
        _sendError(id, JSONRPC_INVALID_PARAMS, errorMsg);
        return;
    }

    // Contrôle de flux PONDÉRÉ (v1.3.0, poids de l'outil) — rejet immédiat
    // (HTTP 429 + Retry-After) si la fenêtre de canaux est pleine, AVANT tout
    // traitement bloquant (appels Domoticz ~3 s) qui étranglerait loop_Core_1.
    // Un outil lourd (poids 3) sature 3× plus vite la fenêtre.
    if (!_flowConsume(_tools[idx].poids)) {
        uint16_t maxEff = _flowMaxEffectif();
        uint32_t retryS;
        String msg;
        // v1.3.1 : distingue 2 cas — (a) outil structurellement trop lourd pour
        // le max effectif courant (poids > maxEff, ex. poids 3 quand XOFF force
        // max=2 → « canaux 0/2 » trompeur) ; (b) fenêtre pleine par rafale.
        if (_tools[idx].poids > maxEff) {
            // v1.3.2 : rejet STRUCTUREL — retry court (le temps n'est pas le
            // facteur limitant) + message selon la cause réelle.
            retryS = 2;
            if (_flowPressionCb) {
                msg = "Outil '" + toolName + "' trop lourd (poids " +
                      String(_tools[idx].poids) + ") pour l'état de flux actuel "
                      "(max " + String(maxEff) + " canaux sous pression mémoire). "
                      "Reessayez dans ~" + String(retryS) + " s.";
            } else {
                msg = "Outil '" + toolName + "' trop lourd (poids " +
                      String(_tools[idx].poids) + ") pour le flux configure "
                      "(max " + String(maxEff) + " canaux). Augmentez le max via "
                      "setFlowControl() ou reduisez le poids. Reessayez dans ~" +
                      String(retryS) + " s.";
            }
        } else {
            uint32_t retryMs = flowRetryAfterMs();
            retryS = (retryMs + 999) / 1000;
            if (retryS == 0) retryS = 1;
            msg = "Trop de demandes MCP — canaux " + String(_flowUsed) + "/" +
                  String(maxEff) + " (sature). Veuillez espacer les requetes "
                  "(outil '" + toolName + "' poids " + String(_tools[idx].poids) +
                  ", fenetre " + String(_flowWindowMs / 1000) +
                  " s). Reessayez dans ~" + String(retryS) + " s.";
        }
        _logError("Flux MCP sature (" + String(_flowUsed) + "/" + String(maxEff) +
                  ") — rejet HTTP 429 (outil '" + toolName + "', poids " +
                  String(_tools[idx].poids) + ")");
        _sendError429(id, JSONRPC_SERVER_ERROR, msg, retryS);
        return;
    }

    _log("Appel outil: " + toolName);
    std::vector<MCPContent> contents = _tools[idx].callback(arguments);

    // Réponse construite dans UN SEUL JsonDocument (29/08/2026, v1.2.0 ;
    // pas de _sendResult → deep-copy du result dans un 2ᵉ doc).
    // L'ancien chemin gardait contents (gros String de log) + finalDoc + doc
    // + output → pic ~3× la taille du texte. Sur ESP8266 (heap ~6-7 Ko), les
    // réponses get_log_* (> ~600 o) saturent → réponse vide / connexion coupée.
    // Désormais : 1 seul doc, contents libéré avant l'envoi → pic ~1,5× texte.
    JsonDocument doc;
    JsonObject response = doc.to<JsonObject>();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    JsonObject finalResult = response["result"].to<JsonObject>();
    JsonArray contentArray = finalResult["content"].to<JsonArray>();
    _serializeContents(contents, contentArray);
    finalResult["isError"] = false;
    contents.clear();          // libère les String (gros texte log) AVANT l'envoi
    contents.shrink_to_fit();
    _sendJSONResponse(response);
    _log("Outil " + toolName + " execute avec succes");
}

inline void Server_MCP::_handleResourcesList(const JsonVariant& id) {
    JsonDocument resultDoc;
    JsonObject result = resultDoc.to<JsonObject>();
    JsonArray resources = result["resources"].to<JsonArray>();
    for (const auto& entry : _resources) {
        JsonObject resObj = resources.add<JsonObject>();
        resObj["uri"] = entry.uri;
        resObj["name"] = entry.name;
        resObj["description"] = entry.description;
        resObj["mimeType"] = entry.mimeType;
    }
    _sendResult(id, result);
}

inline void Server_MCP::_handleResourcesRead(const JsonObject& params, const JsonVariant& id) {
    if (params["uri"].isNull()) {
        _sendError(id, JSONRPC_INVALID_PARAMS, "Missing resource URI");
        return;
    }
    String uri = params["uri"].as<String>();
    for (const auto& entry : _resources) {
        if (entry.uri == uri) {
            JsonDocument resultDoc;
            JsonObject result = resultDoc.to<JsonObject>();
            JsonArray contents = result["contents"].to<JsonArray>();
            JsonObject content = contents.add<JsonObject>();
            content["uri"] = uri;
            content["mimeType"] = entry.mimeType;
            content["text"] = "Resource content placeholder";
            _sendResult(id, result);
            return;
        }
    }
    _sendError(id, JSONRPC_INVALID_PARAMS, "Resource not found: " + uri);
}

inline void Server_MCP::_handlePing(const JsonVariant& id) {
    JsonDocument resultDoc;
    JsonObject result = resultDoc.to<JsonObject>();
    _sendResult(id, result);
}

inline void Server_MCP::_sendResult(const JsonVariant& id, const JsonObject& result) {
    JsonDocument doc;
    JsonObject response = doc.to<JsonObject>();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;
    _sendJSONResponse(response);
}

inline void Server_MCP::_sendError(const JsonVariant& id, int code, const String& message, const JsonObject* data) {
    JsonDocument doc;
    JsonObject response = doc.to<JsonObject>();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    JsonObject error = response["error"].to<JsonObject>();
    error["code"] = code;
    error["message"] = message;
    if (data) error["data"] = *data;
    _sendJSONResponse(response);
}

// Rejet de rate-limiting en HTTP 429 (Too Many Requests) — code standard MCP
// pour le contrôle de flux ; enveloppe JSON-RPC erreur.
// v1.3.0 : en-têtes standards Retry-After + X-RateLimit-* → le client (LM Studio)
// est explicitement averti de patienter (équivalent du XOFF série).
// v1.3.2 : retryS est passé explicitement par l'appelant (2 s si rejet
// structurel poids>maxEff, sinon temps restant de la fenêtre) — le Retry-After
// reste cohérent avec le message.
inline void Server_MCP::_sendError429(const JsonVariant& id, int code, const String& message, uint32_t retryS) {
    if (_server && !_suppressResponse) {
        uint16_t maxEff = _flowMaxEffectif();
        uint16_t used   = flowUsed();
        _server->sendHeader("Retry-After", String(retryS));
        _server->sendHeader("X-RateLimit-Limit", String(maxEff));
        _server->sendHeader("X-RateLimit-Remaining", String(used < maxEff ? maxEff - used : 0));
        _server->sendHeader("X-RateLimit-Window", String(_flowWindowMs / 1000));
    }
    JsonDocument doc;
    JsonObject response = doc.to<JsonObject>();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    JsonObject error = response["error"].to<JsonObject>();
    error["code"] = code;
    error["message"] = message;
    _sendJSONResponse(response, 429);
}

inline void Server_MCP::_sendJSONResponse(const JsonObject& response, uint16_t statusCode) {
    // v1.3.2 : notification — la méthode a été exécutée mais AUCUNE réponse
    // JSON-RPC ne doit partir (le HTTP 202 vide est envoyé par _processJSONRPC).
    if (_suppressResponse) return;
    String output;
    serializeJson(response, output);
    _log("Reponse: " + output.substring(0, 256));
    // WebServer::send(200, type, content) écrit TOUT le corps (28/08/2026, v1.1.2)
    // en UN SEUL write() → tronqué au-delà du buffer TCP ESP32
    // (CONFIG_LWIP_TCP_SND_BUF_DEFAULT=5760 o) : tools/list (~8 Ko) était coupé
    // à 5760 o et le client MCP (LM Studio) ne voyait pas les outils.
    // setContentLength() fixe le Content-Length puis on streame par CHUNKS ≤ 1024 o.
    // statusCode paramétrable (200 par défaut, 429 pour le rate-limiting).
    const size_t CHUNK = 1024;
    _server->setContentLength(output.length());
    _server->send(statusCode, "application/json");   // headers seuls (Content-Length total)
    const char* p = output.c_str();
    size_t len = output.length();
    while (len > 0) {
        size_t n = (len > CHUNK) ? CHUNK : len;
        _server->sendContent(p, n);
        p += n;
        len -= n;
    }
}

inline void Server_MCP::_log(const String& msg) {
    if (_debugEnabled && _debugSerial) {
        _debugSerial->println("[MCP:" + String(_port) + "] " + msg);
    }
}

inline void Server_MCP::_logError(const String& msg) {
    if (_debugEnabled && _debugSerial) {
        _debugSerial->println("[MCP ERROR:" + String(_port) + "] " + msg);
    }
}

inline String Server_MCP::_getContentTypeString(MCPContentType type) {
    switch (type) {
        case MCP_CONTENT_TEXT: return "text";
        case MCP_CONTENT_IMAGE: return "image";
        case MCP_CONTENT_RESOURCE: return "resource";
        default: return "text";
    }
}

inline int Server_MCP::_findToolIndex(const String& name) {
    for (size_t i = 0; i < _tools.size(); i++) {
        if (_tools[i].definition.name == name) return i;
    }
    return -1;
}

inline bool Server_MCP::_validateParams(const JsonObject& params,
                                         const MCPTool& tool,
                                         String& errorMsg) {
    for (const auto& param : tool.params) {
        if (param.required && params[param.name].isNull()) {
            errorMsg = "Missing required parameter: " + param.name;
            return false;
        }
        if (!params[param.name].isNull()) {
            const char* type = param.type.c_str();
            JsonVariant value = params[param.name];
            // v1.3.2 : contrôle de type strict ArduinoJson v7. "integer" exige un
            // entier JSON (JsonInteger, sans partie fractionnaire) ; "array" et
            // "object" sont enfin vérifiés. Type inconnu → pas de contrôle.
            bool ok = true;
            if (strcmp(type, "string") == 0) {
                ok = value.is<const char*>();
            } else if (strcmp(type, "integer") == 0) {
                ok = value.is<JsonInteger>();
            } else if (strcmp(type, "number") == 0) {
                ok = value.is<JsonInteger>() || value.is<float>() || value.is<double>();
            } else if (strcmp(type, "boolean") == 0) {
                ok = value.is<bool>();
            } else if (strcmp(type, "array") == 0) {
                ok = value.is<JsonArray>();
            } else if (strcmp(type, "object") == 0) {
                ok = value.is<JsonObject>();
            }
            if (!ok) {
                errorMsg = "Parameter " + param.name + " must be of type " + param.type;
                return false;
            }
        }
    }
    return true;
}

inline void Server_MCP::_buildToolSchema(const MCPTool& tool, JsonObject& schema) {
    schema["type"] = "object";
    JsonObject properties = schema["properties"].to<JsonObject>();
    JsonArray required = schema["required"].to<JsonArray>();
    for (const auto& param : tool.params) {
        JsonObject prop = properties[param.name].to<JsonObject>();
        prop["type"] = param.type;
        prop["description"] = param.description;
        if (param.required) required.add(param.name);
    }
}

inline void Server_MCP::_serializeContents(const std::vector<MCPContent>& contents, JsonArray& array) {
    for (const auto& content : contents) {
        JsonObject obj = array.add<JsonObject>();
        obj["type"] = _getContentTypeString(content.type);
        switch (content.type) {
            case MCP_CONTENT_TEXT:
                obj["text"] = content.text;
                break;
            case MCP_CONTENT_IMAGE:
                obj["mimeType"] = content.mimeType;
                obj["data"] = content.data;
                break;
            case MCP_CONTENT_RESOURCE:
                obj["resource"] = obj["resource"].to<JsonObject>();
                obj["resource"]["uri"] = content.uri;
                obj["resource"]["mimeType"] = content.mimeType;
                obj["resource"]["text"] = content.text;
                break;
        }
    }
}

#endif // SERVER_MCP_H

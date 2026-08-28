// ============================================================================
// Server_MCP.h
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

#define JSONRPC_PARSE_ERROR      (-32700)
#define JSONRPC_INVALID_REQUEST  (-32600)
#define JSONRPC_METHOD_NOT_FOUND (-32601)
#define JSONRPC_INVALID_PARAMS   (-32602)
#define JSONRPC_INTERNAL_ERROR   (-32603)
#define JSONRPC_SERVER_ERROR     (-32000)

// ⚠️ PATCH LOCAL (28/08/2026) : contrôle de flux "style TCP" — fenêtre de CANAUX
// glissante. Trop de requêtes en rafale empile des appels bloquants (Domoticz
// ~3 s) dans loop_Core_1 et étrangle le polling Telegram/web. Au-delà de
// MCP_FLOW_MAX requêtes par fenêtre MCP_FLOW_WINDOW_MS, la requête est REJETÉE
// en HTTP 429 (protocole MCP standard de rate-limiting). Valeurs surchargées à
// l'exécution via setFlowControl() (appelée depuis INIT_MCP() du projet).
#define MCP_FLOW_MAX        8          // canaux max par fenêtre (défaut strict)
#define MCP_FLOW_WINDOW_MS  30000UL    // fenêtre glissante (défaut 30 s)

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
    JsonVariant defaultValue;
};

struct MCPTool {
    String name;
    String description;
    std::vector<MCPParam> params;
};

using MCPToolCallback = std::function<std::vector<MCPContent>(const JsonObject& params)>;

// ============================================================================
// CLASSE PRINCIPALE
// ============================================================================

class Server_MCP {
public:
    Server_MCP(const String& serverName = "Server-MCP",
               const String& serverVersion = "1.1.2",
               uint16_t maxTools = 16,
               uint16_t maxResources = 8);
    ~Server_MCP();

    void setServerInfo(const String& name, const String& version);
    void setSerialDebug(bool enable, HardwareSerial* serial = &Serial);

    bool registerTool(const String& name,
                      const String& description,
                      MCPToolCallback callback);
    bool addToolParam(const String& name,
                      const String& description,
                      const String& type = "string",
                      bool required = true,
                      const JsonVariant& defaultValue = JsonVariant());
    bool unregisterTool(const String& name);

    bool registerResource(const String& uri,
                          const String& name,
                          const String& description,
                          const String& mimeType = "text/plain");

    // ═════════════════════════════════════════════════════════════════
    // DEMARRAGE AVEC PORT PERSONNALISE
    // ═════════════════════════════════════════════════════════════════
    bool begin(uint16_t port = 8080);  // ← Par defaut 8080, pas 80 !
    
    void handleClient();
    void end();
    bool isRunning() const;
    uint16_t getPort() const;
    String getServerURL() const;

    // ⚠️ PATCH LOCAL : contrôle de flux (fenêtre de canaux glissante).
    void setFlowControl(uint16_t maxRequests, uint32_t windowMs);
    uint16_t flowMax() const;
    uint16_t flowUsed() const;
    uint32_t flowWindowMs() const;
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

    uint32_t _requestId;

    // ⚠️ PATCH LOCAL : contrôle de flux (fenêtre de canaux glissante).
    uint16_t _flowMax;
    uint32_t _flowWindowMs;
    uint32_t _flowWinStart;
    uint16_t _flowUsed;
    uint8_t  _flowEtatPrec;   // dernier état annoncé (pour log des transitions)

    void _handleRoot();
    void _handleMCP();
    void _handleSSE();
    void _handleNotFound();

    void _processJSONRPC(const String& body);
    void _handleInitialize(const JsonObject& params, uint32_t id);
    void _handleToolsList(uint32_t id);
    void _handleToolsCall(const JsonObject& params, uint32_t id);
    void _handleResourcesList(uint32_t id);
    void _handleResourcesRead(const JsonObject& params, uint32_t id);
    void _handlePing(uint32_t id);

    void _sendResult(uint32_t id, const JsonObject& result);
    void _sendError(uint32_t id, int code, const String& message, const JsonObject* data = nullptr);
    void _sendError429(uint32_t id, int code, const String& message);
    void _sendJSONResponse(const JsonObject& response, uint16_t statusCode = 200);
    bool _flowConsume();   // PATCH LOCAL : consomme un canal (rejet si fenêtre pleine)

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
    , _port(8080)
    , _maxTools(maxTools)
    , _maxResources(maxResources)
    , _requestId(0)
    , _flowMax(MCP_FLOW_MAX)
    , _flowWindowMs(MCP_FLOW_WINDOW_MS)
    , _flowWinStart(0)
    , _flowUsed(0)
    , _flowEtatPrec(0)
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
                                      MCPToolCallback callback) {
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
    _tools.push_back(entry);
    _log("Outil enregistre: " + name);
    return true;
}

inline bool Server_MCP::addToolParam(const String& name,
                                      const String& description,
                                      const String& type,
                                      bool required,
                                      const JsonVariant& defaultValue) {
    if (_tools.empty()) {
        _logError("Aucun outil enregistre. Appeler registerTool() d'abord.");
        return false;
    }
    MCPParam param;
    param.name = name;
    param.description = description;
    param.type = type;
    param.required = required;
    param.defaultValue = defaultValue;
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
// PATCH LOCAL : contrôle de flux (fenêtre de canaux glissante)
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

inline uint16_t Server_MCP::flowMax() const      { return _flowMax; }

// canaux utilisés EFFECTIFS pour l'affichage (0 si la fenêtre est expirée —
// le reset est paresseux côté requête, mais le monitoring doit être à jour).
inline uint16_t Server_MCP::flowUsed() const {
    uint32_t now = millis();
    if (_flowWindowMs == 0 || (now - _flowWinStart >= _flowWindowMs) || now < _flowWinStart)
        return 0;
    return _flowUsed;
}

inline uint32_t Server_MCP::flowWindowMs() const { return _flowWindowMs; }

// État annoncé (monitoring web/série) : libre / pris en compte / ralentir / sature.
inline String Server_MCP::flowEtat() const {
    if (_flowMax == 0) return "libre";
    uint32_t now = millis();
    uint16_t used = ((_flowWindowMs == 0) || (now - _flowWinStart >= _flowWindowMs) ||
                     (now < _flowWinStart)) ? 0 : _flowUsed;
    float r = (float)used / (float)_flowMax;
    if (r < 0.5f)       return "libre";
    else if (r < 0.8f)  return "pris en compte";
    else if (r < 1.0f)  return "ralentir";
    return "sature";
}

// Consomme un canal de la fenêtre glissante. Renvoie false si la fenêtre est
// pleine (trop de requêtes) — la requête est alors rejetée en HTTP 429.
inline bool Server_MCP::_flowConsume() {
    uint32_t now = millis();
    if (_flowWindowMs == 0 || (now - _flowWinStart >= _flowWindowMs) || now < _flowWinStart) {
        _flowWinStart = now;   // nouvelle fenêtre (ou wrap de millis())
        _flowUsed = 0;
    }
    if (_flowUsed >= _flowMax) return false;
    _flowUsed++;
    // Log des TRANSITIONS d'état (libre=1, pris en compte=2, ralentir=3, sature=4)
    uint8_t etat = (_flowMax == 0) ? 1
                 : ((float)_flowUsed / _flowMax < 0.5f) ? 1
                 : ((float)_flowUsed / _flowMax < 0.8f) ? 2
                 : ((float)_flowUsed / _flowMax < 1.0f) ? 3 : 4;
    if (etat != _flowEtatPrec) {
        _flowEtatPrec = etat;
        _log("Flux MCP: " + flowEtat() + " (" + String(_flowUsed) + "/" + String(_flowMax) + ")");
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
        _sendError(0, JSONRPC_PARSE_ERROR, "Parse error: " + String(error.c_str()));
        return;
    }
    JsonObject root = doc.as<JsonObject>();
    const char* jsonrpc = root["jsonrpc"];
    if (!jsonrpc || strcmp(jsonrpc, "2.0") != 0) {
        _sendError(0, JSONRPC_INVALID_REQUEST, "Invalid JSON-RPC version");
        return;
    }

    // JSON-RPC notification (message SANS "id") : le client ne doit recevoir
    // AUCUNE réponse. Répondre HTTP 202 Accepted avec un corps vide, comme le
    // veut la spec MCP Streamable HTTP (sinon les clients comme LM Studio /
    // SDK officiel lèvent "Received an unexpected response to a notification").
    if (root["id"].isNull()) {
        _server->send(202, "application/json", "");
        return;
    }

    uint32_t id = 0;
    if (root["id"].is<int>()) id = root["id"].as<int>();
    else if (root["id"].is<unsigned int>()) id = root["id"].as<unsigned int>();

    // ⚠️ PATCH LOCAL : contrôle de flux — rejet immédiat (HTTP 429) si la fenêtre
    // de canaux est pleine (trop de requêtes en rafale), AVANT tout traitement
    // bloquant (appels Domoticz ~3 s) qui étranglerait loop_Core_1.
    if (!_flowConsume()) {
        _sendError429(id, JSONRPC_SERVER_ERROR,
            "Trop de demandes MCP — canaux " + String(_flowUsed) + "/" + String(_flowMax) +
            " (sature). Veuillez espacer les requetes (fenetre " +
            String(_flowWindowMs / 1000) + " s).");
        _logError("Flux MCP sature (" + String(_flowMax) + "/" + String(_flowMax) +
                  ") — rejet HTTP 429");
        return;
    }

    const char* method = root["method"];
    if (!method) {
        _sendError(id, JSONRPC_INVALID_REQUEST, "Missing method");
        return;
    }
    String methodStr = String(method);
    _log("Methode: " + methodStr + " (id=" + String(id) + ")");
    JsonObject params = root["params"].as<JsonObject>();
    if (methodStr == "initialize") _handleInitialize(params, id);
    else if (methodStr == "tools/list") _handleToolsList(id);
    else if (methodStr == "tools/call") _handleToolsCall(params, id);
    else if (methodStr == "resources/list") _handleResourcesList(id);
    else if (methodStr == "resources/read") _handleResourcesRead(params, id);
    else if (methodStr == "ping") _handlePing(id);
    else _sendError(id, JSONRPC_METHOD_NOT_FOUND, "Method not found: " + methodStr);
}

inline void Server_MCP::_handleInitialize(const JsonObject& params, uint32_t id) {
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

inline void Server_MCP::_handleToolsList(uint32_t id) {
    JsonDocument resultDoc;
    JsonObject result = resultDoc.to<JsonObject>();
    JsonArray tools = result["tools"].to<JsonArray>();
    for (const auto& entry : _tools) {
        JsonObject toolObj = tools.add<JsonObject>();
        toolObj["name"] = entry.definition.name;
        toolObj["description"] = entry.definition.description;
        JsonObject inputSchema = toolObj["inputSchema"].to<JsonObject>();
        inputSchema["type"] = "object";
        JsonObject properties = inputSchema["properties"].to<JsonObject>();
        JsonArray required = inputSchema["required"].to<JsonArray>();
        for (const auto& param : entry.definition.params) {
            JsonObject prop = properties[param.name].to<JsonObject>();
            prop["type"] = param.type;
            prop["description"] = param.description;
            if (param.required) required.add(param.name);
        }
    }
    _sendResult(id, result);
}

inline void Server_MCP::_handleToolsCall(const JsonObject& params, uint32_t id) {
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
    JsonObject arguments = params["arguments"].as<JsonObject>();
    String errorMsg;
    if (!_validateParams(arguments, _tools[idx].definition, errorMsg)) {
        _sendError(id, JSONRPC_INVALID_PARAMS, errorMsg);
        return;
    }
    _log("Appel outil: " + toolName);
    std::vector<MCPContent> contents = _tools[idx].callback(arguments);
    JsonDocument finalDoc;
    JsonObject finalResult = finalDoc.to<JsonObject>();
    JsonArray contentArray = finalResult["content"].to<JsonArray>();
    _serializeContents(contents, contentArray);
    finalResult["isError"] = false;
    _sendResult(id, finalResult);
    _log("Outil " + toolName + " execute avec succes");
}

inline void Server_MCP::_handleResourcesList(uint32_t id) {
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

inline void Server_MCP::_handleResourcesRead(const JsonObject& params, uint32_t id) {
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

inline void Server_MCP::_handlePing(uint32_t id) {
    JsonDocument resultDoc;
    JsonObject result = resultDoc.to<JsonObject>();
    _sendResult(id, result);
}

inline void Server_MCP::_sendResult(uint32_t id, const JsonObject& result) {
    JsonDocument doc;
    JsonObject response = doc.to<JsonObject>();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;
    _sendJSONResponse(response);
}

inline void Server_MCP::_sendError(uint32_t id, int code, const String& message, const JsonObject* data) {
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

// ⚠️ PATCH LOCAL : rejet de rate-limiting en HTTP 429 (Too Many Requests) —
// code standard MCP pour le contrôle de flux ; enveloppe JSON-RPC erreur.
inline void Server_MCP::_sendError429(uint32_t id, int code, const String& message) {
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
    String output;
    serializeJson(response, output);
    _log("Reponse: " + output.substring(0, 256));
    // ⚠️ PATCH LOCAL : WebServer::send(200, type, content) écrit TOUT le corps
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
            if (strcmp(type, "string") == 0 && !value.is<const char*>()) {
                errorMsg = "Parameter " + param.name + " must be a string";
                return false;
            }
            if ((strcmp(type, "number") == 0 || strcmp(type, "integer") == 0) && !value.is<int>() && !value.is<float>()) {
                errorMsg = "Parameter " + param.name + " must be a number";
                return false;
            }
            if (strcmp(type, "boolean") == 0 && !value.is<bool>()) {
                errorMsg = "Parameter " + param.name + " must be a boolean";
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

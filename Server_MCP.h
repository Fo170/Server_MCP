// ============================================================================
// Server_MCP.h
// Bibliotheque C++ MCP pour ESP8266 — Port configurable
// ============================================================================

#ifndef SERVER_MCP_H
#define SERVER_MCP_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>
#include <map>

#define MCP_PROTOCOL_VERSION "2024-11-05"

#define JSONRPC_PARSE_ERROR      (-32700)
#define JSONRPC_INVALID_REQUEST  (-32600)
#define JSONRPC_METHOD_NOT_FOUND (-32601)
#define JSONRPC_INVALID_PARAMS   (-32602)
#define JSONRPC_INTERNAL_ERROR   (-32603)
#define JSONRPC_SERVER_ERROR     (-32000)

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
    Server_MCP(const String& serverName = "ESP8266-MCP",
               const String& serverVersion = "1.0.0",
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

    static MCPContent makeTextContent(const String& text);
    static MCPContent makeImageContent(const String& base64Data, const String& mimeType = "image/png");
    static MCPContent makeResourceContent(const String& uri, const String& text, const String& mimeType = "text/plain");

private:
    String _serverName;
    String _serverVersion;
    bool _running;
    bool _debugEnabled;
    HardwareSerial* _debugSerial;

    ESP8266WebServer* _server;
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
    void _sendJSONResponse(const JsonObject& response);

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
    _server = new ESP8266WebServer(port);
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
    StaticJsonDocument<512> doc;
    JsonObject response = doc.to<JsonObject>();
    response["name"] = _serverName;
    response["version"] = _serverVersion;
    response["protocol"] = "MCP";
    response["protocolVersion"] = MCP_PROTOCOL_VERSION;
    response["status"] = "online";
    response["url"] = getServerURL();
    response["port"] = _port;
    JsonObject transports = response.createNestedObject("transports");
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
    StaticJsonDocument<256> doc;
    JsonObject response = doc.to<JsonObject>();
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = JSONRPC_METHOD_NOT_FOUND;
    response["error"]["message"] = "Endpoint not found";
    String output;
    serializeJson(response, output);
    _server->send(404, "application/json", output);
}

inline void Server_MCP::_processJSONRPC(const String& body) {
    StaticJsonDocument<2048> doc;
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
    uint32_t id = 0;
    if (root.containsKey("id")) {
        if (root["id"].is<int>()) id = root["id"].as<int>();
        else if (root["id"].is<unsigned int>()) id = root["id"].as<unsigned int>();
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
    StaticJsonDocument<1024> resultDoc;
    JsonObject result = resultDoc.to<JsonObject>();
    result["protocolVersion"] = MCP_PROTOCOL_VERSION;
    JsonObject serverInfo = result.createNestedObject("serverInfo");
    serverInfo["name"] = _serverName;
    serverInfo["version"] = _serverVersion;
    JsonObject capabilities = result.createNestedObject("capabilities");
    JsonObject toolsCapability = capabilities.createNestedObject("tools");
    if (_resources.size() > 0) {
        capabilities.createNestedObject("resources");
    }
    _sendResult(id, result);
    _log("Initialisation reussie");
}

inline void Server_MCP::_handleToolsList(uint32_t id) {
    StaticJsonDocument<8192> resultDoc;
    JsonObject result = resultDoc.to<JsonObject>();
    JsonArray tools = result.createNestedArray("tools");
    for (const auto& entry : _tools) {
        JsonObject toolObj = tools.createNestedObject();
        toolObj["name"] = entry.definition.name;
        toolObj["description"] = entry.definition.description;
        JsonObject inputSchema = toolObj.createNestedObject("inputSchema");
        inputSchema["type"] = "object";
        JsonObject properties = inputSchema.createNestedObject("properties");
        JsonArray required = inputSchema.createNestedArray("required");
        for (const auto& param : entry.definition.params) {
            JsonObject prop = properties.createNestedObject(param.name);
            prop["type"] = param.type;
            prop["description"] = param.description;
            if (param.required) required.add(param.name);
        }
    }
    _sendResult(id, result);
}

inline void Server_MCP::_handleToolsCall(const JsonObject& params, uint32_t id) {
    if (!params.containsKey("name")) {
        _sendError(id, JSONRPC_INVALID_PARAMS, "Missing tool name");
        return;
    }
    String toolName = params["name"].as<String>();
    int idx = _findToolIndex(toolName);
    if (idx < 0) {
        _sendError(id, JSONRPC_METHOD_NOT_FOUND, "Tool not found: " + toolName);
        return;
    }
    JsonObject arguments;
    if (params.containsKey("arguments")) arguments = params["arguments"].as<JsonObject>();
    String errorMsg;
    if (!_validateParams(arguments, _tools[idx].definition, errorMsg)) {
        _sendError(id, JSONRPC_INVALID_PARAMS, errorMsg);
        return;
    }
    _log("Appel outil: " + toolName);
    std::vector<MCPContent> contents = _tools[idx].callback(arguments);
    StaticJsonDocument<4096> finalDoc;
    JsonObject finalResult = finalDoc.to<JsonObject>();
    JsonArray contentArray = finalResult.createNestedArray("content");
    _serializeContents(contents, contentArray);
    finalResult["isError"] = false;
    _sendResult(id, finalResult);
    _log("Outil " + toolName + " execute avec succes");
}

inline void Server_MCP::_handleResourcesList(uint32_t id) {
    StaticJsonDocument<4096> resultDoc;
    JsonObject result = resultDoc.to<JsonObject>();
    JsonArray resources = result.createNestedArray("resources");
    for (const auto& entry : _resources) {
        JsonObject resObj = resources.createNestedObject();
        resObj["uri"] = entry.uri;
        resObj["name"] = entry.name;
        resObj["description"] = entry.description;
        resObj["mimeType"] = entry.mimeType;
    }
    _sendResult(id, result);
}

inline void Server_MCP::_handleResourcesRead(const JsonObject& params, uint32_t id) {
    if (!params.containsKey("uri")) {
        _sendError(id, JSONRPC_INVALID_PARAMS, "Missing resource URI");
        return;
    }
    String uri = params["uri"].as<String>();
    for (const auto& entry : _resources) {
        if (entry.uri == uri) {
            StaticJsonDocument<1024> resultDoc;
            JsonObject result = resultDoc.to<JsonObject>();
            JsonArray contents = result.createNestedArray("contents");
            JsonObject content = contents.createNestedObject();
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
    StaticJsonDocument<64> resultDoc;
    JsonObject result = resultDoc.to<JsonObject>();
    _sendResult(id, result);
}

inline void Server_MCP::_sendResult(uint32_t id, const JsonObject& result) {
    StaticJsonDocument<8192> doc;
    JsonObject response = doc.to<JsonObject>();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;
    _sendJSONResponse(response);
}

inline void Server_MCP::_sendError(uint32_t id, int code, const String& message, const JsonObject* data) {
    StaticJsonDocument<2048> doc;
    JsonObject response = doc.to<JsonObject>();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    JsonObject error = response["error"].to<JsonObject>();
    error["code"] = code;
    error["message"] = message;
    if (data) error["data"] = *data;
    _sendJSONResponse(response);
}

inline void Server_MCP::_sendJSONResponse(const JsonObject& response) {
    String output;
    serializeJson(response, output);
    _log("Reponse: " + output.substring(0, 256));
    _server->send(200, "application/json", output);
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
        if (param.required && !params.containsKey(param.name)) {
            errorMsg = "Missing required parameter: " + param.name;
            return false;
        }
        if (params.containsKey(param.name)) {
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
    JsonObject properties = schema.createNestedObject("properties");
    JsonArray required = schema.createNestedArray("required");
    for (const auto& param : tool.params) {
        JsonObject prop = properties.createNestedObject(param.name);
        prop["type"] = param.type;
        prop["description"] = param.description;
        if (param.required) required.add(param.name);
    }
}

inline void Server_MCP::_serializeContents(const std::vector<MCPContent>& contents, JsonArray& array) {
    for (const auto& content : contents) {
        JsonObject obj = array.createNestedObject();
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
                obj["resource"] = obj.createNestedObject("resource");
                obj["resource"]["uri"] = content.uri;
                obj["resource"]["mimeType"] = content.mimeType;
                obj["resource"]["text"] = content.text;
                break;
        }
    }
}

#endif // SERVER_MCP_H
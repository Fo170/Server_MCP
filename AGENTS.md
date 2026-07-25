# Server_MCP — Agent Guide

## What this is

Single-header C++ library (`Server_MCP.h`) implementing the Model Context Protocol (MCP) on ESP8266. Header-only — all code lives in the `.h` file, no `.cpp`.

No build system, no tests, no CI in this repo. This is a distribution header + examples.

## Platform & dependency

- **Platform**: ESP8266 + Arduino Framework (PlatformIO)
- **Install**: `lib_deps = https://github.com/Fo170/Server_MCP.git` in `platformio.ini` (`library.json` at repo root handles transitive ArduinoJson 7.x dep)
- **LD script**: `eagle.flash.4m2m.ld`
- **Monitor speed**: 115200
- Include as `#include "Server_MCP.h"` — found automatically via `lib_deps`

## Key facts

- MCP server uses port **8080 by default** (different from web server on 80)
- Tools/resources must be `registerTool()` / `registerResource()` **before** `begin()`
- Call `mcp.handleClient()` in `loop()` to process requests
- MCP endpoint: `POST /mcp` with JSON-RPC 2.0 body
- Error messages and comments are in **French**
- Max 16 tools / 8 resources by default (configurable in constructor)

## Curl testing

```bash
curl -X POST http://IP:8080/mcp -H "Content-Type: application/json" -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'
```

## SSE endpoint

`GET /sse` returns a `text/event-stream` pointing to `/mcp`.

## Resource placeholder

`resources/read` returns a placeholder `"Resource content placeholder"` — actual content reading must be added by the user via callback.

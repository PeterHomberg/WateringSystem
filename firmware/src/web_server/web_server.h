#pragma once
#include <Arduino.h>

// Async HTTP server on port WEB_SERVER_PORT (default 80).
// Uses ESPAsyncWebServer — no loop() polling needed.
// Safe to call initWebServer() multiple times (e.g. after WiFi reconnect).
//
// GET  /              → dashboard HTML
// GET  /api/status    → JSON snapshot
// POST /api/valve     → {"valve":1,"open":true}
// POST /api/time      → {"datetime":"2025-04-07T14:30:00","weekday":1}
// GET  /api/schedule  → JSON array of entries
// POST /api/schedule  → JSON array (replaces all)

void initWebServer();

// handleWebServer() no longer needed — AsyncWebServer runs on background task.
// Kept as empty stub for backwards compatibility with any existing loop() calls.
inline void handleWebServer() {}

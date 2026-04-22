#pragma once
#include <Arduino.h>

// Serves the dashboard on port WEB_SERVER_PORT (default 80).
// Uses the built-in synchronous WebServer — no AsyncTCP needed.
//
// GET  /              → dashboard HTML
// GET  /api/status    → JSON snapshot
// POST /api/valve     → {"valve":1,"open":true}
// POST /api/time      → {"datetime":"2025-04-07T14:30:00","weekday":1}
// GET  /api/schedule  → JSON array of entries
// POST /api/schedule  → JSON array (replaces all)

void initWebServer();

// Call from loop() — synchronous server requires polling
void handleWebServer();

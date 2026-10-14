#pragma once
#include <ArduinoJson.h>
#include <WebServer.h>

// HTTP server: setup UI, JSON API and OTA upload. Sign-in routes live in WebAuth.
extern WebServer server;
// Pages and explicitly marked UI traffic keep modem sleep off for 60 s.
// Ordinary API calls do not extend this window; hidden/closed tabs let it end.
void noteWebActivity();
void serviceWeb();
void jsonReply(int status, const JsonDocument &doc);
void errorReply(int status, const char *message);
void messageReply(const char *message);
void registerWeb();

#pragma once
#include <ArduinoJson.h>
#include <WebServer.h>

// HTTP server: setup UI, JSON API and OTA upload. Sign-in routes live in WebAuth.
extern WebServer server;
void jsonReply(int status, const JsonDocument &doc);
void errorReply(int status, const char *message);
void messageReply(const char *message);
void registerWeb();

#pragma once
#include <Arduino.h>
#include "Auth.h"

// Web sign-in (NVS "auth"). New chimes start without sign-in and with an open
// setup hotspot; the owner can enable it with their own user name/password.
struct AuthConfig { uint8_t version = 1; bool enabled = false; char user[33] = "admin"; char password[64]{}; };
extern AuthConfig auth;
extern String csrf; // Per-boot token required by every mutating API request.

// Loads (or migrates) sign-in settings and creates this boot's CSRF token.
void beginAuth(bool existingInstallation);
// Physical-access recovery: turns sign-in off and keeps the credentials.
bool signInOff();
bool sessionValid();
// A valid session cookie, or HTTP Basic credentials for scripts. Basic
// attempts share the sign-in throttle. Always true while sign-in is off.
bool credentialsValid();
// These send the 401/403 reply themselves when they return false.
bool authenticated();
bool mutationAuthorized();
// /api/login, /api/logout(/all), /api/auth and /api/password.
void registerAuthRoutes();

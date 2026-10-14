#include "WebAuth.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_system.h>
#include <mbedtls/md.h>
#include "App.h"
#include "LinkService.h"
#include "Web.h"

AuthConfig auth;
String csrf;

namespace {
uint32_t authEpoch = 0; // Changing it signs out every browser.
uint8_t sessionKey[32];
chime::LoginThrottle loginThrottle;
constexpr uint32_t SessionSeconds = 12 * 3600, RememberSeconds = 30 * 86400;

bool hmacSha256(const uint8_t *key, size_t keyLength, const uint8_t *data, size_t length, uint8_t out[32]) {
  return mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, keyLength, data, length, out) == 0;
}
// Session cookies are signed with a key derived from the credentials, the
// board MAC and the sign-out epoch: a credential change invalidates them all.
void deriveSessionKey() {
  uint8_t material[sizeof(auth.user) + 10]{};
  memcpy(material, auth.user, strlen(auth.user));
  const uint64_t mac = ESP.getEfuseMac();
  for (int i = 0; i < 6; ++i) material[sizeof(auth.user) + i] = uint8_t(mac >> (8 * i));
  memcpy(material + sizeof(auth.user) + 6, &authEpoch, 4);
  hmacSha256(reinterpret_cast<const uint8_t *>(auth.password), strlen(auth.password),
    material, sizeof(material), sessionKey);
}
// User 1-32 and password 8-63 printable ASCII characters; the password is
// also the WPA2 key of the setup hotspot while sign-in is enabled.
bool validCredential(const char *text, size_t min, size_t max, bool user) {
  const size_t length = strlen(text);
  if (length < min || length > max) return false;
  for (const char *c = text; *c; ++c) if (*c < 0x20 || *c > 0x7E || (user && *c == ':')) return false;
  return true;
}
bool validAuth(const AuthConfig &a) {
  return a.version == 1 && memchr(a.user, 0, sizeof(a.user)) && memchr(a.password, 0, sizeof(a.password)) &&
    validCredential(a.user, 1, 32, true) && (a.password[0] ? validCredential(a.password, 8, 63, false) : !a.enabled);
}
bool saveAuth(const AuthConfig &next, uint32_t epoch) {
  Preferences prefs; if (!prefs.begin("auth", false)) return false;
  const bool ok = prefs.putBytes("cfg", &next, sizeof(next)) == sizeof(next) && prefs.putULong("epoch", epoch) == 4;
  prefs.end(); return ok;
}
String sessionCookie() {
  const String cookies = server.header("Cookie");
  int start = -1;
  while ((start = cookies.indexOf("chime_session=", start + 1)) >= 0)
    if (start == 0 || cookies[start - 1] == ' ' || cookies[start - 1] == ';') break;
  if (start < 0) return String();
  start += 14;
  int end = cookies.indexOf(';', start); if (end < 0) end = cookies.length();
  return cookies.substring(start, end);
}
void setSessionCookie(bool remember) {
  uint8_t nonce[8]; esp_fill_random(nonce, sizeof(nonce));
  const uint32_t now = epochNow();
  char token[chime::TokenLength + 1];
  if (!chime::makeToken(sessionKey, now ? now + (remember ? RememberSeconds : SessionSeconds) : 0,
                        nonce, hmacSha256, token)) return;
  String cookie = String("chime_session=") + token + "; Path=/; HttpOnly; SameSite=Lax";
  if (remember) cookie += "; Max-Age=" + String(RememberSeconds);
  server.sendHeader("Set-Cookie", cookie);
}
void clearSessionCookie() { server.sendHeader("Set-Cookie", "chime_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0"); }
uint32_t clientAddress() { return uint32_t(server.client().remoteIP()); }
}

// Installations from before optional sign-in keep it enabled as "admin" with
// their existing password; brand-new chimes start open for initial setup.
void beginAuth(bool existingInstallation) {
  Preferences prefs; AuthConfig stored; bool found = false;
  if (prefs.begin("auth", true)) {
    found = prefs.getBytesLength("cfg") == sizeof(stored) &&
      prefs.getBytes("cfg", &stored, sizeof(stored)) == sizeof(stored) && validAuth(stored);
    authEpoch = prefs.getULong("epoch", 0);
    prefs.end();
  }
  bool save = !authEpoch;
  if (!authEpoch) authEpoch = esp_random() | 1;
  if (found) auth = stored;
  else if (existingInstallation) {
    auth.enabled = true; strlcpy(auth.user, "admin", sizeof(auth.user));
    strlcpy(auth.password, settings.adminPassword, sizeof(auth.password)); save = true;
  }
  if (save && !saveAuth(auth, authEpoch)) logf("WARNING: sign-in settings could not be saved\n");
  deriveSessionKey();
  uint8_t random[16]; esp_fill_random(random,sizeof(random)); csrf=hexBytes(random,sizeof(random));
}
bool signInOff() {
  AuthConfig next = auth; next.enabled = false; radio.pause();
  const bool ok = saveAuth(next, authEpoch);
  if (ok) { auth = next; deriveSessionKey(); }
  restartRadio();
  return ok;
}
bool sessionValid() {
  const String token = sessionCookie();
  return chime::checkToken(sessionKey, token.c_str(), token.length(), epochNow(), hmacSha256);
}
bool credentialsValid() {
  if (!auth.enabled || sessionValid()) return true;
  if (!server.header("Authorization").length()) return false;
  uint32_t wait;
  if (!loginThrottle.allowed(clientAddress(), millis(), wait)) return false;
  if (server.authenticate(auth.user, auth.password)) { loginThrottle.success(clientAddress()); return true; }
  loginThrottle.failure(clientAddress(), millis()); return false;
}
bool authenticated() {
  if (credentialsValid()) {
    // Only the visible browser UI opts in; API scripts/polling stay neutral.
    if (server.header("X-WebUI-Active") == "1") noteWebActivity();
    return true;
  }
  errorReply(401, "Sign in required"); return false; // No Basic challenge: the UI has a login page.
}
bool mutationAuthorized() {
  if (!authenticated()) return false;
  if (server.header("X-CSRF-Token") != csrf) {
    errorReply(403, "Missing or invalid CSRF token; reload the setup page"); return false;
  }
  return true;
}

void registerAuthRoutes() {
  server.on("/api/login", HTTP_POST, []() {
    const uint32_t client = clientAddress(); uint32_t wait;
    if (!loginThrottle.allowed(client, millis(), wait)) {
      server.sendHeader("Retry-After", String((wait + 999) / 1000));
      errorReply(429, (String("Too many attempts. Try again in ") + (wait + 999) / 1000 + " s.").c_str()); return;
    }
    StaticJsonDocument<384> doc;
    if (deserializeJson(doc, server.arg("plain")) || !doc["password"].is<const char *>()) { errorReply(400, "Expected {username, password}"); return; }
    if (!auth.enabled) { messageReply("Sign-in is turned off"); return; }
    const bool userOk = chime::sameSecret(doc["username"] | "", auth.user);
    if (!(chime::sameSecret(doc["password"].as<const char *>(), auth.password) && userOk)) {
      loginThrottle.failure(client, millis());
      logf("[auth] failed sign-in from %s\n", server.client().remoteIP().toString().c_str());
      errorReply(401, "Wrong user name or password"); return;
    }
    loginThrottle.success(client);
    setSessionCookie(doc["remember"] == true);
    logf("[auth] %s signed in from %s\n", auth.user, server.client().remoteIP().toString().c_str());
    messageReply("Signed in");
  });
  server.on("/api/logout", HTTP_POST, []() { clearSessionCookie(); messageReply("Signed out"); });
  server.on("/api/logout/all", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    radio.pause(); const bool ok = saveAuth(auth, authEpoch + 1); restartRadio();
    if (!ok) { errorReply(500, "Could not save"); return; }
    ++authEpoch; deriveSessionKey(); if (auth.enabled) setSessionCookie(false);
    logf("[auth] all other sessions signed out\n");
    messageReply("All other browsers are signed out");
  });
  server.on("/api/auth", HTTP_GET, []() {
    if (!authenticated()) return;
    StaticJsonDocument<192> doc;
    doc["enabled"] = auth.enabled; doc["username"] = auth.user; doc["password_set"] = auth.password[0] != 0;
    jsonReply(200, doc);
  });
  // {enabled, username, password}; a blank/missing password keeps the current one.
  server.on("/api/auth", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    StaticJsonDocument<384> doc;
    if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>()) { errorReply(400, "Expected a JSON object"); return; }
    AuthConfig next = auth;
    if (doc.containsKey("enabled")) {
      if (!doc["enabled"].is<bool>()) { errorReply(400, "enabled: true or false"); return; }
      next.enabled = doc["enabled"];
    }
    if (doc.containsKey("username")) {
      if (!doc["username"].is<const char *>() || !validCredential(doc["username"].as<const char *>(), 1, 32, true)) {
        errorReply(400, "User name: 1-32 characters, no colon"); return;
      }
      strlcpy(next.user, doc["username"], sizeof(next.user));
    }
    const char *password = doc["password"] | "";
    if (password[0]) {
      if (!validCredential(password, 8, 63, false)) { errorReply(400, "Password: 8-63 characters"); return; }
      strlcpy(next.password, password, sizeof(next.password));
    }
    if (next.enabled && !next.password[0]) { errorReply(400, "Choose a password to turn sign-in on"); return; }
    radio.pause(); const bool ok = validAuth(next) && saveAuth(next, authEpoch); restartRadio();
    if (!ok) { errorReply(500, "Could not save sign-in settings"); return; }
    const bool changed = strcmp(next.user, auth.user) || strcmp(next.password, auth.password) || next.enabled != auth.enabled;
    auth = next; deriveSessionKey();
    if (auth.enabled) setSessionCookie(false); // Keep this browser signed in.
    if (changed) logf("[auth] sign-in %s for user '%s'\n", auth.enabled ? "enabled" : "disabled", auth.user);
    messageReply(auth.enabled ? "Sign-in is on. The setup hotspot uses this password after a restart." :
      "Sign-in is off. Anyone on your network can open this page.");
  });
  server.on("/api/password", HTTP_POST, []() {
    if (!mutationAuthorized()) return;
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc,server.arg("plain")) || !doc["password"].is<const char *>()) { errorReply(400,"Expected {password}"); return; }
    // Kept for scripts; /api/auth also sets the user name and on/off state.
    const char *password=doc["password"];
    if (!validCredential(password,8,63,false)) { errorReply(400,"Password must be 8-63 characters"); return; }
    AuthConfig next=auth; strlcpy(next.password,password,sizeof(next.password));
    radio.pause(); const bool ok=saveAuth(next,authEpoch); restartRadio();
    if (!ok) { errorReply(500,"Could not save password"); return; }
    auth=next; deriveSessionKey(); if (auth.enabled) setSessionCookie(false);
    logf("Sign-in password changed (also the setup hotspot password while sign-in is on)\n");
    messageReply("Password changed");
  });
}

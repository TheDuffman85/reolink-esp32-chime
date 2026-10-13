#pragma once
#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <mqtt_client.h>

// MQTT broker, Home Assistant discovery and time server (NVS "integrations").
// Kept outside the Settings blob so existing pairings survive updates.
struct IntegrationSettings {
  uint8_t version = 1;
  bool mqtt = false, discovery = true;
  uint16_t port = 1883;
  char host[65]{}, user[65]{}, password[65]{};
  char topic[65]{};                      // Empty: "reolink_chime/<device id>".
  char prefix[33] = "homeassistant";     // Home Assistant discovery prefix.
  char ntp[65] = "pool.ntp.org";         // Empty disables time synchronisation.
};
bool validIntegrations(const IntegrationSettings &settings);
bool loadIntegrations(IntegrationSettings &settings);
bool saveIntegrations(const IntegrationSettings &settings);
// Topic levels without wildcards, empty levels or leading/trailing '/'.
bool validTopic(const char *topic);

struct MqttMessage { char topic[128]; char payload[512]; size_t length; };

// ESP-IDF MQTT client. Its own task handles the network; publishing goes
// through a second task, so the main loop (and RF reception) never blocks on
// the broker. Received messages are queued for the main loop.
class Mqtt {
 public:
  bool begin();
  // (Re)connects with these settings; base is the topic prefix for commands.
  void start(const IntegrationSettings &settings, const String &clientId, const String &base);
  void stop();
  bool running() const { return client_ != nullptr; }
  bool connected() const { return connected_; }
  // True once after each connection or Home Assistant restart.
  bool takeAnnounce() { return announce_.exchange(false); }
  bool pop(MqttMessage &message);
  // Queued; dropped while disconnected or when the queue is full.
  bool publish(const String &topic, const String &payload, bool retain);
  String state() const;
  uint32_t dropped() const { return dropped_; }
 private:
  struct Outgoing { String topic, payload; bool retain; };
  static void onEvent(void *self, esp_event_base_t base, int32_t id, void *data);
  static void publisher(void *self);
  esp_mqtt_client_handle_t client_ = nullptr;
  SemaphoreHandle_t lock_ = nullptr;
  QueueHandle_t inbox_ = nullptr, outbox_ = nullptr;
  String base_, prefix_, availability_;
  bool discovery_ = false;
  std::atomic<bool> connected_{false}, announce_{false};
  std::atomic<int> error_{0};
  std::atomic<uint32_t> dropped_{0};
};

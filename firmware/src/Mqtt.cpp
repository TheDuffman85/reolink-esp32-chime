#include "Mqtt.h"
#include <Preferences.h>
#include <new>

bool validTopic(const char *topic) {
  const size_t length = strlen(topic);
  if (!length || topic[0] == '/' || topic[length - 1] == '/' || strstr(topic, "//")) return false;
  for (const char *c = topic; *c; ++c)
    if (*c == '+' || *c == '#' || uint8_t(*c) < 0x21 || uint8_t(*c) == 0x7F) return false;
  return true;
}
bool validIntegrations(const IntegrationSettings &s) {
  const auto terminated = [](const char *text, size_t size) { return memchr(text, 0, size) != nullptr; };
  if (s.version != 1 || !s.port || !terminated(s.host, sizeof(s.host)) || !terminated(s.user, sizeof(s.user)) ||
      !terminated(s.password, sizeof(s.password)) || !terminated(s.topic, sizeof(s.topic)) ||
      !terminated(s.prefix, sizeof(s.prefix)) || !terminated(s.ntp, sizeof(s.ntp))) return false;
  if (s.mqtt && !s.host[0]) return false;
  for (const char *c = s.host; *c; ++c)
    if (!isalnum(uint8_t(*c)) && *c != '.' && *c != '-' && *c != '_' && *c != ':') return false;
  for (const char *c = s.ntp; *c; ++c)
    if (!isalnum(uint8_t(*c)) && *c != '.' && *c != '-') return false;
  return (!s.topic[0] || validTopic(s.topic)) && validTopic(s.prefix);
}
bool loadIntegrations(IntegrationSettings &settings) {
  Preferences prefs; IntegrationSettings stored;
  if (!prefs.begin("integrations", true)) return false;
  const bool found = prefs.getBytesLength("cfg") == sizeof(stored) &&
    prefs.getBytes("cfg", &stored, sizeof(stored)) == sizeof(stored) && validIntegrations(stored);
  prefs.end();
  if (found) settings = stored;
  return found;
}
bool saveIntegrations(const IntegrationSettings &settings) {
  if (!validIntegrations(settings)) return false;
  Preferences prefs;
  if (!prefs.begin("integrations", false)) return false;
  const bool ok = prefs.putBytes("cfg", &settings, sizeof(settings)) == sizeof(settings);
  prefs.end(); return ok;
}

bool Mqtt::begin() {
  if (!lock_) lock_ = xSemaphoreCreateMutex();
  if (!inbox_) inbox_ = xQueueCreate(6, sizeof(MqttMessage));
  if (!outbox_) {
    outbox_ = xQueueCreate(32, sizeof(Outgoing *));
    if (outbox_ && xTaskCreate(publisher, "mqtt-pub", 4096, this, 1, nullptr) != pdPASS) {
      vQueueDelete(outbox_); outbox_ = nullptr;
    }
  }
  return lock_ && inbox_ && outbox_;
}

void Mqtt::start(const IntegrationSettings &settings, const String &clientId, const String &base) {
  stop();
  if (!settings.mqtt || !begin()) return;
  base_ = base; prefix_ = settings.prefix; discovery_ = settings.discovery;
  availability_ = base_ + "/availability";
  const String uri = String("mqtt://") + settings.host + ":" + settings.port;
  esp_mqtt_client_config_t config{};
  config.uri = uri.c_str();
  config.client_id = clientId.c_str();
  config.username = settings.user[0] ? settings.user : nullptr;
  config.password = settings.password[0] ? settings.password : nullptr;
  config.lwt_topic = availability_.c_str(); config.lwt_msg = "offline";
  config.lwt_qos = 1; config.lwt_retain = 1;
  config.keepalive = 30;
  config.buffer_size = 2048;
  config.network_timeout_ms = 5000;
  config.reconnect_timeout_ms = 10000;
  error_ = 0; connected_ = false; announce_ = false;
  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&config); // Copies all strings.
  if (!client) { error_ = 2; return; }
  esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, onEvent, this);
  if (esp_mqtt_client_start(client) != ESP_OK) { esp_mqtt_client_destroy(client); error_ = 2; return; }
  xSemaphoreTake(lock_, portMAX_DELAY); client_ = client; xSemaphoreGive(lock_);
}

void Mqtt::stop() {
  if (!client_) return;
  // Wait for an in-flight publish, then detach the client from the publisher.
  xSemaphoreTake(lock_, portMAX_DELAY);
  esp_mqtt_client_handle_t client = client_; client_ = nullptr;
  xSemaphoreGive(lock_);
  if (connected_) esp_mqtt_client_publish(client, availability_.c_str(), "offline", 0, 1, 1);
  esp_mqtt_client_stop(client);
  esp_mqtt_client_destroy(client);
  connected_ = false; announce_ = false;
  if (inbox_) xQueueReset(inbox_);
}

bool Mqtt::pop(MqttMessage &message) {
  return inbox_ && xQueueReceive(inbox_, &message, 0) == pdTRUE;
}

bool Mqtt::publish(const String &topic, const String &payload, bool retain) {
  if (!client_ || !connected_ || !outbox_) return false;
  Outgoing *message = new (std::nothrow) Outgoing{topic, payload, retain};
  if (!message) { ++dropped_; return false; }
  if (xQueueSend(outbox_, &message, 0) != pdTRUE) { delete message; ++dropped_; return false; }
  return true;
}

String Mqtt::state() const {
  if (!client_) return "off";
  if (connected_) return "connected";
  switch (error_) {
    case 0: return "connecting";
    case 1: return "cannot reach the broker";
    case 2: return "client could not start";
    case 100 + MQTT_CONNECTION_REFUSE_ID_REJECTED: return "refused: client ID rejected";
    case 100 + MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE: return "refused: broker unavailable";
    case 100 + MQTT_CONNECTION_REFUSE_BAD_USERNAME:
    case 100 + MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED: return "refused: wrong user name or password";
    default: return "refused by the broker";
  }
}

void Mqtt::publisher(void *self) {
  auto &mqtt = *static_cast<Mqtt *>(self);
  while (true) {
    Outgoing *message = nullptr;
    if (xQueueReceive(mqtt.outbox_, &message, portMAX_DELAY) != pdTRUE || !message) continue;
    xSemaphoreTake(mqtt.lock_, portMAX_DELAY);
    if (mqtt.client_ && mqtt.connected_ &&
        esp_mqtt_client_publish(mqtt.client_, message->topic.c_str(), message->payload.c_str(),
                                int(message->payload.length()), 0, message->retain ? 1 : 0) < 0)
      ++mqtt.dropped_;
    xSemaphoreGive(mqtt.lock_);
    delete message;
  }
}

// Runs in the MQTT client task.
void Mqtt::onEvent(void *self, esp_event_base_t, int32_t id, void *data) {
  auto &mqtt = *static_cast<Mqtt *>(self);
  auto *event = static_cast<esp_mqtt_event_handle_t>(data);
  switch (static_cast<esp_mqtt_event_id_t>(id)) {
    case MQTT_EVENT_CONNECTED: {
      const String base = mqtt.base_;
      for (const char *suffix : {"/set", "/set/+", "/play", "/get"})
        esp_mqtt_client_subscribe(event->client, (base + suffix).c_str(), 0);
      if (mqtt.discovery_) esp_mqtt_client_subscribe(event->client, (mqtt.prefix_ + "/status").c_str(), 0);
      esp_mqtt_client_publish(event->client, mqtt.availability_.c_str(), "online", 0, 1, 1);
      mqtt.error_ = 0; mqtt.connected_ = true; mqtt.announce_ = true;
      break;
    }
    case MQTT_EVENT_DISCONNECTED:
      mqtt.connected_ = false;
      break;
    case MQTT_EVENT_ERROR:
      if (event->error_handle && event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        mqtt.error_ = 100 + event->error_handle->connect_return_code;
      else if (event->error_handle && event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        mqtt.error_ = 1;
      break;
    case MQTT_EVENT_DATA: {
      // Commands are small; fragmented or oversized messages are ignored.
      if (event->current_data_offset || event->total_data_len != event->data_len ||
          event->topic_len <= 0 || size_t(event->topic_len) >= sizeof(MqttMessage::topic) ||
          size_t(event->data_len) >= sizeof(MqttMessage::payload)) break;
      MqttMessage message;
      memcpy(message.topic, event->topic, event->topic_len); message.topic[event->topic_len] = 0;
      memcpy(message.payload, event->data, event->data_len); message.payload[event->data_len] = 0;
      message.length = event->data_len;
      // Home Assistant announces restarts; resend discovery and states.
      if (mqtt.discovery_ && mqtt.prefix_ + "/status" == message.topic) {
        if (!strcmp(message.payload, "online")) mqtt.announce_ = true;
        break;
      }
      if (xQueueSend(mqtt.inbox_, &message, 0) != pdTRUE) ++mqtt.dropped_;
      break;
    }
    default: break;
  }
}

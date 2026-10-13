#pragma once
#include <Arduino.h>
#include "Mqtt.h"
#include "RingLog.h"

// MQTT commands and state, and Home Assistant discovery (NVS "integrations").
// Commands reach only chime settings and playback, never administration.
extern IntegrationSettings integrations;
extern Mqtt mqtt;
String defaultMqttBase();
void startMqtt();
void publishRing(const chime::RingEvent &e);
// Announces, handles one queued command and publishes changed state/status.
void serviceMqtt();

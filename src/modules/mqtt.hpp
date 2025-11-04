#pragma once
#include "mqtt_client.h"

class MQTTManager {
public:
    MQTTManager();
    void begin(const char* broker, int port, const char* client_id, const char* username, const char* password);
    void publish(const char* topic, const char* payload);
    void loop();
private:
}
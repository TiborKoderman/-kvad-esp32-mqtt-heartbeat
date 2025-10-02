#ifdef MQTT_MODULE

#include <Arduino.h>
extern "C" {
  #include "mqtt_client.h"
}


class MqttModule {
  public:
    explicit MqttModule(JsonObjectConst cfg) : _cfg(cfg) {}
    void connect(const char* server, uint16_t port);
    void publish(const char* topic, const char* payload);
    void subscribe(const char* topic);
    void loop();
private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
};

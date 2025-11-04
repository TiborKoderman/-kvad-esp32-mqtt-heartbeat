#pragma once

class MqttClient {
public:
  void init(const MqttConfig& config);
  void connect(const char* broker, int port);
  void disconnect();
  void publish(const char* topic, const char* message);
  void subscribe(const char* topic);
}


struct MqttConfig {
  const char* broker_address;
  int broker_port;
  const char* client_id;
  const char* username;
  const char* password;
  bool use_tls;
};

enum class MqttQoS {
  AT_MOST_ONCE = 0,
  AT_LEAST_ONCE = 1,
  EXACTLY_ONCE = 2
};

enum class MqttAuthMethod {
  NONE,
  USERNAME_PASSWORD,
  CERTIFICATE
};
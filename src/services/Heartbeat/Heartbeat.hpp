#pragma once

class HeartbeatService {
public:
  void init(const HeartbeatConfig& config);
  void start(int interval_ms);
  void stop();
private:
  int interval_ms_;
  bool running_;
};


struct HeartbeatConfig {
  int interval_ms;
  const char* heartbeat_topic;
  bool retain;
};
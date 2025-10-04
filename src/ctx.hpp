#pragma once
#include <cstdint>

struct Ctx {
   bool nvs_ok


};

struct metrics_t {
  uint32_t wifi_reconnects = 0;
  uint32_t mqtt_reconnects = 0;
  uint32_t mqtt_msgs_sent = 0;
  uint32_t mqtt_msgs_recv = 0;
  uint32_t mqtt_msgs_dropped = 0;
  uint32_t heartbeat_sent = 0;
  uint32_t heartbeat_failed = 0;
};




#pragma once
#include <cstdint>

struct Ctx {
   bool nvs_ok;
   bool flash_ok;
   bool psram_ok;
   bool timer_ok;
   bool gpio_ok;
   bool wifi_hw_ok;
   uint32_t heap_size;
   uint32_t psram_size;
   uint32_t flash_size;
   const char* chip_model;
   uint8_t chip_revision;
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




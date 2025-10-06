#pragma once
#include "ctx.hpp"
#include "fsm.hpp"
#include "core/hardware_check.hpp"
#include <esp_log.h>
#include "core/config_manager.hpp"

extern const StateDesc<Ctx> ST_NOMINAL, ST_ERR;

namespace {
static constexpr const char* TAG = "INIT";
}

static void enter(Ctx* ctx) {
  ESP_LOGI(TAG, "Initializing system...");
  HardwareCheck::performHardwareCheck(ctx);

  // Initialize configuration manager
  static ConfigManager cfg_mgr;
  if (!cfg_mgr.init()) {
    ESP_LOGE(TAG, "Configuration manager initialization failed");
    // Mark flash as not OK to trigger error state
    ctx->flash_ok = false;
  } else {
    ESP_LOGI(TAG, "Configuration loaded successfully");
    ctx->flash_ok = true;
  }

  



}

static const StateDesc<Ctx>* next(Ctx* c) {
  // Require all critical systems to be working
  bool critical_ok = c->nvs_ok && c->flash_ok && c->timer_ok && c->gpio_ok && c->wifi_hw_ok;
  return critical_ok ? &ST_NOMINAL : &ST_ERR;
}

const StateDesc<Ctx> ST_INIT{
  "INIT", enter, nullptr, nullptr, nullptr, next
};
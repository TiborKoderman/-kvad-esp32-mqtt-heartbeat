#pragma once
#include "ctx.hpp"
#include "fsm.hpp"
#include "core/hardware_check.hpp"
#include <esp_log.h>

extern const StateDesc<Ctx> ST_READY, ST_ERR;

namespace {
static const char* TAG = "INIT";
}

static void enter(Ctx* ctx) {
  ESP_LOGI(TAG, "Initializing system...");
  HardwareCheck::performHardwareCheck(ctx);
}

static const StateDesc<Ctx>* next(Ctx* c) {
  // Require all critical systems to be working
  bool critical_ok = c->nvs_ok && c->flash_ok && c->timer_ok && c->gpio_ok && c->wifi_hw_ok;
  return critical_ok ? &ST_READY : &ST_ERR;
}

const StateDesc<Ctx> ST_INIT{
  "INIT", enter, nullptr, nullptr, nullptr, next
};
#pragma once
#include "ctx.hpp"
#include "include/fsm.hpp"
#include "core/hw_diag.hpp"
#include <esp_log.h>
#include <esp_system.h>

extern const StateDesc<Ctx> ST_INIT, ST_DEGRADED;

static const char* TAG = "ERROR";

static void enter(Ctx* ctx) {
  ESP_LOGE(TAG, "🚨 SYSTEM ERROR STATE ENTERED 🚨");
  ESP_LOGE(TAG, "Critical hardware failure detected!");
  
  // Print detailed hardware status for debugging
  HwDiag::printHardwareInfo(ctx);
  
  // Log the specific failures
  if (!ctx->nvs_ok) ESP_LOGE(TAG, "❌ NVS Flash failure");
  if (!ctx->flash_ok) ESP_LOGE(TAG, "❌ Main Flash failure"); 
  if (!ctx->timer_ok) ESP_LOGE(TAG, "❌ Timer subsystem failure");
  if (!ctx->gpio_ok) ESP_LOGE(TAG, "❌ GPIO subsystem failure");
  if (!ctx->wifi_hw_ok) ESP_LOGE(TAG, "❌ WiFi hardware failure");
  
  ESP_LOGE(TAG, "System cannot operate safely. Manual intervention required.");
  ESP_LOGE(TAG, "Will attempt restart in 30 seconds...");
}

static void tick(Ctx* ctx, uint32_t now_ms) {
  // Log error state every 10 seconds
  static uint32_t last_log = 0;
  if (now_ms - last_log > 10000) {
    ESP_LOGE(TAG, "Still in error state - hardware score: %d/100", 
             HwDiag::getHardwareScore(ctx));
    last_log = now_ms;
  }
  
  // Auto-restart after 30 seconds in error state
  static uint32_t error_start_time = 0;
  if (error_start_time == 0) {
    error_start_time = now_ms;
  }
  
  if (now_ms - error_start_time > 30000) {
    ESP_LOGE(TAG, "Timeout reached. Restarting system...");
    esp_restart();
  }
}

static void event(Ctx* ctx, const FsmEvent& evt) {
  switch (evt.id) {
    case 1: // Manual recovery attempt
      ESP_LOGW(TAG, "Manual recovery attempt requested");
      // Could trigger hardware re-check here
      break;
    case 2: // Force restart
      ESP_LOGW(TAG, "Force restart requested");
      esp_restart();
      break;
    default:
      ESP_LOGD(TAG, "Unhandled event: %d", evt.id);
      break;
  }
}

static const StateDesc<Ctx>* next(Ctx* c) {
  // In a real system, you might check if hardware recovered
  // For now, only manual intervention can exit error state
  return nullptr; // Stay in error state
}

const StateDesc<Ctx> ST_ERR{
  "ERROR", enter, nullptr, tick, event, next
};
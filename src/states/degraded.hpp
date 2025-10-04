#pragma once
#include "ctx.hpp"
#include "fsm.hpp"
#include "core/hw_diag.hpp"
#include <esp_log.h>

extern const StateDesc<Ctx> ST_NOMINAL, ST_ERR;

namespace {
static const char* TAG = "DEGRADED";
}

static void enter(Ctx* ctx) {
  ESP_LOGW(TAG, "⚠️  DEGRADED MODE ENTERED");
  ESP_LOGW(TAG, "System running with reduced functionality");
  
  // Print current hardware status
  HwDiag::printHardwareInfo(ctx);
  
  uint8_t hw_score = HwDiag::getHardwareScore(ctx);
  ESP_LOGW(TAG, "Hardware score: %d/100 (degraded threshold)", hw_score);
  
  // Log what's working and what's not
  ESP_LOGW(TAG, "Operating with limited capabilities:");
  if (ctx->nvs_ok) ESP_LOGI(TAG, "✓ Configuration storage available");
  if (ctx->wifi_hw_ok) ESP_LOGI(TAG, "✓ WiFi connectivity possible");
  if (!ctx->psram_ok && ctx->psram_size == 0) ESP_LOGW(TAG, "⚠ No PSRAM - memory limited");
  
  ESP_LOGW(TAG, "Some features may be disabled or unreliable");
}

static void exit(Ctx* ctx) {
  ESP_LOGI(TAG, "Exiting degraded mode");
}

static void tick(Ctx* ctx, uint32_t now_ms) {
  // Periodic health monitoring in degraded mode
  static uint32_t last_health_check = 0;
  if (now_ms - last_health_check > 60000) { // Check every minute
    
    uint8_t hw_score = HwDiag::getHardwareScore(ctx);
    ESP_LOGW(TAG, "Health check - score: %d/100", hw_score);
    
    // Check if we have minimum memory
    if (!HwDiag::hasMinimumMemory(30)) { // Need at least 30KB
      ESP_LOGW(TAG, "⚠ Low memory warning");
    }
    
    last_health_check = now_ms;
  }
  
  // Placeholder for future degraded mode operations
  // Could implement:
  // - Reduced-frequency heartbeats
  // - Limited sensor reading
  // - Basic connectivity only
}

static void event(Ctx* ctx, const FsmEvent& evt) {
  switch (evt.id) {
    case 1: // Health check request
      ESP_LOGI(TAG, "Manual health check requested");
      HwDiag::printHardwareInfo(ctx);
      break;
    case 2: // Try to recover to nominal
      ESP_LOGI(TAG, "Recovery to nominal mode requested");
      // Could trigger re-initialization of failed subsystems
      break;
    default:
      ESP_LOGD(TAG, "Unhandled event: %d", evt.id);
      break;
  }
}

static const StateDesc<Ctx>* next(Ctx* c) {
  // Check if we can return to nominal operation
  if (HwDiag::isHardwareHealthy(c) && HwDiag::hasMinimumMemory(50)) {
    ESP_LOGI(TAG, "Hardware recovered - transitioning to nominal");
    return &ST_NOMINAL;
  }
  
  // Check if things got worse - transition to error
  uint8_t hw_score = HwDiag::getHardwareScore(c);
  if (hw_score < 40) { // Below critical threshold
    ESP_LOGE(TAG, "Hardware condition worsened - transitioning to error");
    return &ST_ERR;
  }
  
  // Stay in degraded mode
  return nullptr;
}

const StateDesc<Ctx> ST_DEGRADED{
  "DEGRADED", enter, exit, tick, event, next
};
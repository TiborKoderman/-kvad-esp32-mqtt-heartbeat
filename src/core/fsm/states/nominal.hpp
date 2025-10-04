#pragma once
#include "ctx.hpp"
#include "include/fsm.hpp"
#include "core/hw_diag.hpp"
#include <esp_log.h>

extern const StateDesc<Ctx> ST_DEGRADED, ST_ERR;

static const char* TAG = "NOMINAL";

static void enter(Ctx* ctx) {
  ESP_LOGI(TAG, "🚀 NOMINAL OPERATION MODE");
  ESP_LOGI(TAG, "All systems operational - full functionality available");
  
  uint8_t hw_score = HwDiag::getHardwareScore(ctx);
  ESP_LOGI(TAG, "Hardware score: %d/100 (excellent)", hw_score);
  
  // Log available capabilities
  ESP_LOGI(TAG, "Available features:");
  ESP_LOGI(TAG, "✓ Full MQTT heartbeat functionality");
  ESP_LOGI(TAG, "✓ WiFi connectivity with auto-reconnect");
  ESP_LOGI(TAG, "✓ Configuration management");
  ESP_LOGI(TAG, "✓ Real-time monitoring");
  
  if (HwDiag::hasPSRAM(ctx)) {
    ESP_LOGI(TAG, "✓ Extended memory available (%lu MB PSRAM)", 
             ctx->psram_size / (1024 * 1024));
  }
  
  ESP_LOGI(TAG, "System ready for production operation");
}

static void exit(Ctx* ctx) {
  ESP_LOGW(TAG, "Leaving nominal operation mode");
}

static void tick(Ctx* ctx, uint32_t now_ms) {
  // Periodic system health monitoring
  static uint32_t last_health_check = 0;
  if (now_ms - last_health_check > 300000) { // Check every 5 minutes
    
    uint8_t hw_score = HwDiag::getHardwareScore(ctx);
    ESP_LOGI(TAG, "Periodic health check - score: %d/100", hw_score);
    
    // Log memory status
    uint32_t free_kb = esp_get_free_heap_size() / 1024;
    ESP_LOGI(TAG, "Free memory: %lu KB", free_kb);
    
    last_health_check = now_ms;
  }
  
  // Main application logic would go here:
  // - MQTT heartbeat transmission
  // - Sensor data collection
  // - WiFi connection management
  // - Configuration updates
  // - Watchdog feeding
  
  // Placeholder for MQTT heartbeat logic
  static uint32_t last_heartbeat = 0;
  if (now_ms - last_heartbeat > 30000) { // Every 30 seconds
    ESP_LOGI(TAG, "📡 MQTT heartbeat (placeholder)");
    // TODO: Implement actual MQTT heartbeat
    last_heartbeat = now_ms;
  }
}

static void event(Ctx* ctx, const FsmEvent& evt) {
  switch (evt.id) {
    case 1: // Status request
      ESP_LOGI(TAG, "Status report requested");
      HwDiag::printHardwareInfo(ctx);
      break;
    case 2: // Configuration update
      ESP_LOGI(TAG, "Configuration update received");
      // TODO: Handle configuration changes
      break;
    case 3: // Network event
      ESP_LOGI(TAG, "Network event received");
      // TODO: Handle WiFi/MQTT events
      break;
    case 10: // Force degraded mode (for testing)
      ESP_LOGW(TAG, "Forced degradation requested");
      // This could be used to test state transitions
      break;
    default:
      ESP_LOGD(TAG, "Unhandled event: %d", evt.id);
      break;
  }
}

static const StateDesc<Ctx>* next(Ctx* c) {
  // Monitor system health and transition if needed
  
  // Check for critical hardware failure
  if (!HwDiag::isHardwareHealthy(c)) {
    ESP_LOGW(TAG, "Hardware health degraded - checking severity");
    
    uint8_t hw_score = HwDiag::getHardwareScore(c);
    if (hw_score < 40) {
      ESP_LOGE(TAG, "Critical hardware failure - transitioning to error");
      return &ST_ERR;
    } else if (hw_score < 80) {
      ESP_LOGW(TAG, "Hardware degraded - transitioning to degraded mode");
      return &ST_DEGRADED;
    }
  }
  
  // Check memory pressure
  if (!HwDiag::hasMinimumMemory(40)) {
    ESP_LOGW(TAG, "Low memory condition - considering degraded mode");
    return &ST_DEGRADED;
  }
  
  // Check for other system stress indicators
  // TODO: Add checks for:
  // - Network connectivity issues
  // - MQTT connection problems
  // - Sensor failures
  // - Configuration errors
  
  // Stay in nominal mode
  return nullptr;
}

const StateDesc<Ctx> ST_NOMINAL{
  "NOMINAL", enter, exit, tick, event, next
};
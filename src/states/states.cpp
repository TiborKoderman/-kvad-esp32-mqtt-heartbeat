#include "states.hpp"
#include "core/hardware_check.hpp"
#include "core/hw_diag.hpp"
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_system.h>

// ============================================================================
// INIT STATE
// ============================================================================
namespace InitState {
static const char* TAG = "INIT";

static void enter(Ctx* ctx) {
  ESP_LOGI(TAG, "Initializing system...");
  HardwareCheck::performHardwareCheck(ctx);
}

static const StateDesc<Ctx>* next(Ctx* c) {
  bool critical_ok = c->nvs_ok && c->flash_ok && c->timer_ok && c->gpio_ok && c->wifi_hw_ok;
  return critical_ok ? &ST_NOMINAL : &ST_ERR;
}
} // namespace InitState

const StateDesc<Ctx> ST_INIT{
  .name = "INIT",
  .onEnter = InitState::enter,
  .onExit = nullptr,
  .onTick = nullptr,
  .onEvent = nullptr,
  .next = InitState::next
};

// ============================================================================
// ERROR STATE  
// ============================================================================
namespace ErrorState {
static const char* TAG = "ERROR";
static uint32_t error_start_time = 0;
static constexpr uint32_t RESTART_TIMEOUT_MS = 30000;

static void enter(Ctx* ctx) {
  ESP_LOGE(TAG, "💥 SYSTEM ERROR - Critical hardware failure detected!");
  ESP_LOGE(TAG, "Hardware status:");
  ESP_LOGE(TAG, "  NVS: %s", ctx->nvs_ok ? "OK" : "FAIL");
  ESP_LOGE(TAG, "  Flash: %s", ctx->flash_ok ? "OK" : "FAIL");
  ESP_LOGE(TAG, "  PSRAM: %s", ctx->psram_ok ? "OK" : "FAIL");
  ESP_LOGE(TAG, "  Timer: %s", ctx->timer_ok ? "OK" : "FAIL");
  ESP_LOGE(TAG, "  GPIO: %s", ctx->gpio_ok ? "OK" : "FAIL");
  ESP_LOGE(TAG, "  WiFi HW: %s", ctx->wifi_hw_ok ? "OK" : "FAIL");
  ESP_LOGE(TAG, "  Heap: %u KB", (unsigned)(ctx->heap_size / 1024));
  ESP_LOGE(TAG, "System will restart in %d seconds...", (int)(RESTART_TIMEOUT_MS / 1000));
  
  error_start_time = esp_timer_get_time() / 1000; // Convert to ms
}

static void tick(Ctx* ctx, uint32_t now_ms) {
  static uint32_t last_warn = 0;
  
  // Print countdown every 5 seconds
  if ((now_ms - last_warn) >= 5000) {
    uint32_t elapsed = now_ms - error_start_time;
    uint32_t remaining = (RESTART_TIMEOUT_MS > elapsed) ? (RESTART_TIMEOUT_MS - elapsed) : 0;
    ESP_LOGW(TAG, "⏰ Restart in %d seconds...", (int)(remaining / 1000));
    last_warn = now_ms;
  }
}

static void event(Ctx* ctx, const FsmEvent& evt) {
  switch (evt.id) {
    case 1: // HARDWARE_CHECK
      ESP_LOGI(TAG, "Hardware check requested in error state");
      HardwareCheck::performHardwareCheck(ctx);
      break;
    case 2: // SYSTEM_RESTART
      ESP_LOGW(TAG, "🔄 Manual restart requested");
      esp_restart();
      break;
    default:
      ESP_LOGD(TAG, "Ignoring event %d in error state", (int)evt.id);
      break;
  }
}

static const StateDesc<Ctx>* next(Ctx* c) {
  uint32_t now = esp_timer_get_time() / 1000;
  
  if ((now - error_start_time) >= RESTART_TIMEOUT_MS) {
    ESP_LOGE(TAG, "🔄 Timeout reached, restarting system...");
    esp_restart();
    return &ST_INIT; // This won't be reached, but for completeness
  }
  
  return nullptr; // Stay in error state
}
} // namespace ErrorState

const StateDesc<Ctx> ST_ERR{
  .name = "ERROR",
  .onEnter = ErrorState::enter,
  .onExit = nullptr,
  .onTick = ErrorState::tick,
  .onEvent = ErrorState::event,
  .next = ErrorState::next
};

// ============================================================================
// DEGRADED STATE
// ============================================================================
namespace DegradedState {
static const char* TAG = "DEGRADED";

static void enter(Ctx* ctx) {
  ESP_LOGW(TAG, "⚠️  System running in DEGRADED mode");
  ESP_LOGW(TAG, "Some non-critical systems may not be fully functional");
  ESP_LOGW(TAG, "Will periodically check if full functionality can be restored");
}

static void exit(Ctx* ctx) {
  ESP_LOGI(TAG, "Exiting degraded mode");
}

static void tick(Ctx* ctx, uint32_t now_ms) {
  static uint32_t last_health_check = 0;
  constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 60000; // 1 minute
  
  if ((now_ms - last_health_check) >= HEALTH_CHECK_INTERVAL_MS) {
    ESP_LOGI(TAG, "🔍 Performing periodic health check...");
    
    // Re-check hardware to see if we can upgrade to nominal
    bool all_ok = HardwareCheck::performHardwareCheck(ctx);
    
    if (all_ok) {
      ESP_LOGI(TAG, "✅ Hardware issues resolved, can transition to nominal");
    } else {
      ESP_LOGW(TAG, "⚠️  Still in degraded state, will check again later");
    }
    
    last_health_check = now_ms;
  }
}

static void event(Ctx* ctx, const FsmEvent& evt) {
  switch (evt.id) {
    case 1: // HARDWARE_CHECK
      ESP_LOGI(TAG, "Manual hardware check in degraded state");
      HardwareCheck::performHardwareCheck(ctx);
      break;
    case 2: // SYSTEM_RESTART
      ESP_LOGW(TAG, "🔄 Restart requested from degraded state");
      esp_restart();
      break;
    default:
      ESP_LOGD(TAG, "Event %d processed in degraded state", (int)evt.id);
      break;
  }
}

static const StateDesc<Ctx>* next(Ctx* c) {
  // Get current hardware health score
  uint8_t health_score = HwDiag::getHardwareScore(c);
  
  if (health_score >= 90) {
    ESP_LOGI(TAG, "Health score %d%% - transitioning to NOMINAL", health_score);
    return &ST_NOMINAL;
  }
  
  if (health_score < 50) {
    ESP_LOGE(TAG, "Health score %d%% - transitioning to ERROR", health_score);
    return &ST_ERR;
  }
  
  // Stay in degraded state
  return nullptr;
}
} // namespace DegradedState

const StateDesc<Ctx> ST_DEGRADED{
  .name = "DEGRADED",
  .onEnter = DegradedState::enter,
  .onExit = DegradedState::exit,
  .onTick = DegradedState::tick,
  .onEvent = DegradedState::event,
  .next = DegradedState::next
};

// ============================================================================
// NOMINAL STATE
// ============================================================================
namespace NominalState {
static const char* TAG = "NOMINAL";
static uint32_t last_health_check = 0;
static uint32_t last_heartbeat = 0;

static void enter(Ctx* ctx) {
  ESP_LOGI(TAG, "🟢 System running in NOMINAL mode");
  ESP_LOGI(TAG, "All systems operational, starting services...");
  
  // Reset counters
  last_health_check = esp_timer_get_time() / 1000;
  last_heartbeat = last_health_check;
  
  // TODO: Start WiFi connection
  // TODO: Start MQTT client
  // TODO: Start heartbeat service
  
  ESP_LOGI(TAG, "✅ All services started successfully");
}

static void exit(Ctx* ctx) {
  ESP_LOGI(TAG, "Shutting down nominal mode services...");
  
  // TODO: Stop MQTT client
  // TODO: Stop WiFi (if needed)
  
  ESP_LOGI(TAG, "Services stopped");
}

static void tick(Ctx* ctx, uint32_t now_ms) {
  constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 300000;  // 5 minutes
  constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000;      // 30 seconds
  
  // Periodic hardware health check
  if ((now_ms - last_health_check) >= HEALTH_CHECK_INTERVAL_MS) {
    ESP_LOGI(TAG, "🔍 Performing periodic health check...");
    
    // Log memory usage
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "Memory: %u KB free (min was %u KB)", 
             (unsigned)(free_heap / 1024), (unsigned)(min_free / 1024));
    
    // Quick health check
    bool health_ok = HardwareCheck::performHardwareCheck(ctx);
    if (!health_ok) {
      ESP_LOGW(TAG, "Health check failed, may need to degrade");
    }
    
    last_health_check = now_ms;
  }
  
  // Send heartbeat
  if ((now_ms - last_heartbeat) >= HEARTBEAT_INTERVAL_MS) {
    ESP_LOGI(TAG, "💓 Sending heartbeat...");
    
    // TODO: Send MQTT heartbeat with system status
    // For now, just log the heartbeat
    ESP_LOGI(TAG, "Heartbeat: uptime=%d s, free_heap=%d KB", 
             (int)(now_ms / 1000), (int)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
    
    last_heartbeat = now_ms;
  }
}

static void event(Ctx* ctx, const FsmEvent& evt) {
  switch (evt.id) {
    case 1: // HARDWARE_CHECK
      ESP_LOGI(TAG, "Manual hardware check requested");
      HardwareCheck::performHardwareCheck(ctx);
      break;
    case 3: // MQTT_CONNECTED
      ESP_LOGI(TAG, "✅ MQTT connected");
      break;
    case 4: // MQTT_DISCONNECTED
      ESP_LOGW(TAG, "⚠️  MQTT disconnected");
      break;
    case 5: // WIFI_CONNECTED
      ESP_LOGI(TAG, "✅ WiFi connected");
      break;
    case 6: // WIFI_DISCONNECTED
      ESP_LOGW(TAG, "⚠️  WiFi disconnected");
      break;
    case 2: // SYSTEM_RESTART
      ESP_LOGW(TAG, "🔄 Restart requested from nominal state");
      esp_restart();
      break;
    default:
      ESP_LOGD(TAG, "Event %d processed in nominal state", (int)evt.id);
      break;
  }
}

static const StateDesc<Ctx>* next(Ctx* c) {
  // Get hardware health score
  uint8_t health_score = HwDiag::getHardwareScore(c);
  
  if (health_score < 50) {
    ESP_LOGE(TAG, "Critical health score %d%% - transitioning to ERROR", health_score);
    return &ST_ERR;
  }
  
  if (health_score < 75) {
    ESP_LOGW(TAG, "Low health score %d%% - transitioning to DEGRADED", health_score);
    return &ST_DEGRADED;
  }
  
  // Stay in nominal state
  return nullptr;
}
} // namespace NominalState

const StateDesc<Ctx> ST_NOMINAL{
  .name = "NOMINAL",
  .onEnter = NominalState::enter,
  .onExit = NominalState::exit,
  .onTick = NominalState::tick,
  .onEvent = NominalState::event,
  .next = NominalState::next
};
#pragma once
#include "ctx.hpp"
#include <esp_log.h>
#include <esp_system.h>
#include "esp_heap_caps.h"

// Hardware diagnostic utilities
namespace HwDiag {

namespace {
static const char* TAG = "HWDIAG";
}

// Print detailed hardware information
void printHardwareInfo(const Ctx* ctx) {
    ESP_LOGI(TAG, "=== Hardware Status ===");
    ESP_LOGI(TAG, "Chip: %s rev%d", ctx->chip_model, ctx->chip_revision);
    ESP_LOGI(TAG, "Flash: %s (%u MB)", ctx->flash_ok ? "✓" : "✗", (unsigned)(ctx->flash_size / (1024 * 1024)));
    ESP_LOGI(TAG, "PSRAM: %s (%u MB)", ctx->psram_ok ? "✓" : "✗", (unsigned)(ctx->psram_size / (1024 * 1024)));
    ESP_LOGI(TAG, "Heap: %u KB total, %u KB free", 
             (unsigned)(ctx->heap_size / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
    ESP_LOGI(TAG, "NVS: %s", ctx->nvs_ok ? "✓" : "✗");
    ESP_LOGI(TAG, "Timer: %s", ctx->timer_ok ? "✓" : "✗");
    ESP_LOGI(TAG, "GPIO: %s", ctx->gpio_ok ? "✓" : "✗");
    ESP_LOGI(TAG, "WiFi HW: %s", ctx->wifi_hw_ok ? "✓" : "✗");
    ESP_LOGI(TAG, "=====================");
}

// Quick health check - returns true if all critical systems are OK
bool isHardwareHealthy(const Ctx* ctx) {
    return ctx->nvs_ok && ctx->flash_ok && ctx->timer_ok && 
           ctx->gpio_ok && ctx->wifi_hw_ok;
}

// Check if we have enough memory for operation
bool hasMinimumMemory(uint32_t required_kb = 50) {
    uint32_t free_kb = heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024;
    return free_kb >= required_kb;
}

// Check if PSRAM is available and working
bool hasPSRAM(const Ctx* ctx) {
    return ctx->psram_ok && ctx->psram_size > 0;
}

// Get a simple hardware score (0-100)
uint8_t getHardwareScore(const Ctx* ctx) {
    uint8_t score = 0;
    if (ctx->nvs_ok) score += 20;      // Critical
    if (ctx->flash_ok) score += 20;    // Critical  
    if (ctx->timer_ok) score += 15;    // Important
    if (ctx->gpio_ok) score += 15;     // Important
    if (ctx->wifi_hw_ok) score += 20;  // Critical for MQTT
    if (ctx->psram_ok) score += 10;    // Nice to have
    return score;
}

} // namespace HwDiag
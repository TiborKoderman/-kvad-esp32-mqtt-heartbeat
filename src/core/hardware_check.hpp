#pragma once
#include "ctx.hpp"
#include <esp_err.h>
#include "nvs_flash.h"
#include <esp_log.h>
#include <esp_flash.h>
#include <esp_psram.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <esp_wifi.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>

namespace HardwareCheck {

namespace {
static const char* TAG = "HW_CHECK";
}

// Perform comprehensive hardware initialization and checks
// Returns true if all critical systems are OK
bool performHardwareCheck(Ctx* ctx) {
  ESP_LOGI(TAG, "Starting hardware initialization checks...");
  
  // Initialize all flags to false
  ctx->nvs_ok = false;
  ctx->flash_ok = false;
  ctx->psram_ok = false;
  ctx->timer_ok = false;
  ctx->gpio_ok = false;
  ctx->wifi_hw_ok = false;
  
  // Get chip information
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  ctx->chip_revision = chip_info.revision;
  
  switch(chip_info.model) {
    case CHIP_ESP32:   ctx->chip_model = "ESP32"; break;
    case CHIP_ESP32S2: ctx->chip_model = "ESP32-S2"; break;
    case CHIP_ESP32S3: ctx->chip_model = "ESP32-S3"; break;
    case CHIP_ESP32C3: ctx->chip_model = "ESP32-C3"; break;
    case CHIP_ESP32C2: ctx->chip_model = "ESP32-C2"; break;
    case CHIP_ESP32C6: ctx->chip_model = "ESP32-C6"; break;
    case CHIP_ESP32H2: ctx->chip_model = "ESP32-H2"; break;
    default: ctx->chip_model = "Unknown"; break;
  }
  
  ESP_LOGI(TAG, "Chip: %s rev%d, %d cores", ctx->chip_model, ctx->chip_revision, chip_info.cores);
  
  // 1. Check NVS Flash
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS partition truncated, erasing...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ctx->nvs_ok = (ret == ESP_OK);
  if (ctx->nvs_ok) {
    ESP_LOGI(TAG, "✓ NVS flash initialized");
  } else {
    ESP_LOGE(TAG, "✗ NVS init failed: %s", esp_err_to_name(ret));
  }
  
  // 2. Check main flash
  uint32_t flash_size;
  ret = esp_flash_get_size(NULL, &flash_size);
  ctx->flash_ok = (ret == ESP_OK);
  ctx->flash_size = ctx->flash_ok ? flash_size : 0;
  if (ctx->flash_ok) {
    ESP_LOGI(TAG, "✓ Flash: %u MB", (unsigned)(flash_size / (1024 * 1024)));
  } else {
    ESP_LOGE(TAG, "✗ Flash check failed: %s", esp_err_to_name(ret));
  }
  
  // 3. Check PSRAM (if available)
  #if CONFIG_SPIRAM_SUPPORT || CONFIG_SPIRAM
  size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  if (psram_size > 0) {
    ctx->psram_size = psram_size;
    ctx->psram_ok = true;
    ESP_LOGI(TAG, "✓ PSRAM: %u MB", (unsigned)(ctx->psram_size / (1024 * 1024)));
  } else {
    ctx->psram_ok = true; // Not having PSRAM is OK
    ctx->psram_size = 0;
    ESP_LOGW(TAG, "PSRAM: Not detected or not configured");
  }
  #else
  ctx->psram_ok = true; // Not having PSRAM is OK
  ctx->psram_size = 0;
  ESP_LOGW(TAG, "PSRAM: Support not compiled in");
  #endif
  
  // 4. Check heap memory
  ctx->heap_size = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
  bool heap_ok = (ctx->heap_size > 100 * 1024); // At least 100KB
  if (heap_ok) {
    ESP_LOGI(TAG, "✓ Heap: %u KB free of %u KB total", 
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024),
             (unsigned)(ctx->heap_size / 1024));
  } else {
    ESP_LOGE(TAG, "✗ Insufficient heap memory: %u KB", (unsigned)(ctx->heap_size / 1024));
  }
  
  // 5. Check high-resolution timer
  ret = esp_timer_init();
  ctx->timer_ok = (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE); // Already initialized is OK
  if (ctx->timer_ok) {
    ESP_LOGI(TAG, "✓ High-resolution timer initialized");
  } else {
    ESP_LOGE(TAG, "✗ Timer init failed: %s", esp_err_to_name(ret));
  }
  
  // 6. Check GPIO subsystem (test a safe pin)
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << GPIO_NUM_2), // Usually LED pin, safe to test
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  ret = gpio_config(&io_conf);
  ctx->gpio_ok = (ret == ESP_OK);
  if (ctx->gpio_ok) {
    // Test GPIO by toggling it
    gpio_set_level(GPIO_NUM_2, 1);
    gpio_set_level(GPIO_NUM_2, 0);
    ESP_LOGI(TAG, "✓ GPIO subsystem working");
  } else {
    ESP_LOGE(TAG, "✗ GPIO config failed: %s", esp_err_to_name(ret));
  }
  
  // 7. Check WiFi hardware initialization
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ret = esp_wifi_init(&cfg);
  ctx->wifi_hw_ok = (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE);
  if (ctx->wifi_hw_ok) {
    ESP_LOGI(TAG, "✓ WiFi hardware initialized");
    // Get MAC address as additional verification
    uint8_t mac[6];
    ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "  MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
  } else {
    ESP_LOGE(TAG, "✗ WiFi hardware init failed: %s", esp_err_to_name(ret));
  }
  
  // Summary
  bool all_ok = ctx->nvs_ok && ctx->flash_ok && ctx->psram_ok && 
                ctx->timer_ok && ctx->gpio_ok && ctx->wifi_hw_ok && heap_ok;
                
  if (all_ok) {
    ESP_LOGI(TAG, "🎉 All hardware checks passed!");
  } else {
    ESP_LOGE(TAG, "❌ Some hardware checks failed!");
    ESP_LOGE(TAG, "Status: NVS=%d Flash=%d PSRAM=%d Timer=%d GPIO=%d WiFi=%d Heap=%d",
             ctx->nvs_ok, ctx->flash_ok, ctx->psram_ok, 
             ctx->timer_ok, ctx->gpio_ok, ctx->wifi_hw_ok, heap_ok);
  }
  
  // Return true if all critical systems are working
  return ctx->nvs_ok && ctx->flash_ok && ctx->timer_ok && ctx->gpio_ok && ctx->wifi_hw_ok && heap_ok;
}

} // namespace HardwareCheck
#include "main.hpp"
#include "ctx.hpp"
#include "fsm.hpp"
#include "states.hpp"
#include "wifi.hpp"
#include "core/config_manager.hpp"

static const char* TAG = "MAIN";

extern "C" void app_main(void)
{
    Ctx ctx{};
    Fsm<Ctx> fsm(&ctx, &ST_INIT);

    // Initialize ConfigManager and load configuration
    ConfigManager configManager;
    if (!configManager.init("/config.json", "/config.msgpack")) {
        ESP_LOGE(TAG, "ConfigManager initialization failed");
        return;
    }

    // Get WiFi configuration from loaded config
    JsonVariantConst wifiConfig = configManager.get("modules.wifi");
    if (wifiConfig.isNull()) {
        ESP_LOGE(TAG, "WiFi configuration not found in config");
        return;
    }

    // Initialize and start WiFiManager
    WiFiManager wifiManager;
    if (!wifiManager.begin(wifiConfig.as<JsonObjectConst>())) {
        ESP_LOGE(TAG, "WiFi initialization failed");
        return;
    }

    if (!wifiManager.start()) {
        ESP_LOGE(TAG, "WiFi start failed");
        return;
    }

    ESP_LOGI(TAG, "System initialized successfully");
}
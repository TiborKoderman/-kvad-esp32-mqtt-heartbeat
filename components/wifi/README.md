# WiFi Manager Usage Guide

## Overview

The WiFi Manager provides a complete WiFi configuration system for ESP32 that supports both Station (STA) and Access Point (AP) modes, with comprehensive JSON-based configuration following the `wifi.schema.json` specification.

## Features

- **Station Mode (STA)**: Connect to existing WiFi networks
- **Access Point Mode (AP)**: Create your own WiFi network
- **Dual Mode**: Run STA and AP simultaneously
- **Security**: Support for Open, WPA2, and WPA3 authentication
- **PMF**: Protected Management Frames (802.11w)
- **Static/DHCP IP**: Full IPv4 configuration
- **Fallback AP**: Automatically start AP when STA disconnects
- **Auto-reconnect**: Automatic reconnection on disconnect
- **Country-specific settings**: Regulatory domain configuration
- **Power management**: TX power control and sleep modes

## Basic Usage

```cpp
#include "wifi.hpp"
#include <ArduinoJson.h>

// Create WiFi manager instance
WiFiManager wifiManager;

void setup() {
    // Parse JSON configuration
    StaticJsonDocument<2048> doc;
    
    // Load from file or define inline
    const char* config = R"({
        "enabled": true,
        "hostname": "my-esp32",
        "sta": {
            "enabled": true,
            "ssid": "MyHomeNetwork",
            "password": "MySecurePassword",
            "auth": "wpa2",
            "pmf": "optional",
            "ipv4": {
                "dhcp": true
            },
            "autoReconnect": true
        }
    })";
    
    deserializeJson(doc, config);
    JsonObjectConst cfg = doc.as<JsonObjectConst>();
    
    // Initialize WiFi Manager
    if (wifiManager.begin(cfg)) {
        ESP_LOGI("APP", "WiFi Manager initialized");
        
        // Start WiFi
        if (wifiManager.start()) {
            ESP_LOGI("APP", "WiFi started successfully");
        }
    }
}

void loop() {
    // Check connection status
    if (wifiManager.isSTAConnected()) {
        // Connected to WiFi
    }
    
    delay(1000);
}
```

## Configuration Examples

### Example 1: Simple STA Mode (DHCP)

```json
{
    "enabled": true,
    "hostname": "esp32-sensor",
    "sta": {
        "enabled": true,
        "ssid": "HomeWiFi",
        "password": "MyPassword123",
        "auth": "wpa2",
        "pmf": "optional",
        "ipv4": {
            "dhcp": true
        },
        "autoReconnect": true
    }
}
```

### Example 2: STA with Static IP

```json
{
    "enabled": true,
    "hostname": "esp32-device",
    "sta": {
        "enabled": true,
        "ssid": "OfficeNetwork",
        "password": "SecurePass456",
        "auth": "wpa2",
        "pmf": "optional",
        "ipv4": {
            "dhcp": false,
            "address": "192.168.1.100",
            "gateway": "192.168.1.1",
            "netmask": "255.255.255.0",
            "dns": ["8.8.8.8", "8.8.4.4"]
        },
        "autoReconnect": true
    }
}
```

### Example 3: WPA3 with Required PMF

```json
{
    "enabled": true,
    "sta": {
        "enabled": true,
        "ssid": "SecureNetwork",
        "password": "VerySecurePassword",
        "auth": "wpa3",
        "pmf": "required",
        "ipv4": {
            "dhcp": true
        }
    }
}
```

### Example 4: STA with BSSID Lock

```json
{
    "enabled": true,
    "sta": {
        "enabled": true,
        "ssid": "MultiAPNetwork",
        "password": "password",
        "auth": "wpa2",
        "bssidLock": true,
        "bssid": "AA:BB:CC:DD:EE:FF",
        "ipv4": {
            "dhcp": true
        }
    }
}
```

### Example 5: Access Point Mode

```json
{
    "enabled": true,
    "hostname": "esp32-ap",
    "ap": {
        "enabled": true,
        "ssid": "ESP32-Config",
        "password": "configure123",
        "auth": "wpa2",
        "channel": 6,
        "hidden": false,
        "maxClients": 4,
        "ipv4": {
            "address": "192.168.4.1",
            "gateway": "192.168.4.1",
            "netmask": "255.255.255.0",
            "dhcp": {
                "enabled": true,
                "start": "192.168.4.2",
                "end": "192.168.4.100",
                "netmask": "255.255.255.0"
            }
        },
        "policy": "always-on"
    }
}
```

### Example 6: STA with Fallback AP

```json
{
    "enabled": true,
    "hostname": "esp32-device",
    "sta": {
        "enabled": true,
        "ssid": "PrimaryNetwork",
        "password": "password123",
        "auth": "wpa2",
        "ipv4": {
            "dhcp": true
        },
        "autoReconnect": true
    },
    "ap": {
        "enabled": true,
        "ssid": "ESP32-Fallback",
        "password": "fallback123",
        "auth": "wpa2",
        "channel": "auto",
        "maxClients": 2,
        "ipv4": {
            "address": "192.168.4.1",
            "gateway": "192.168.4.1",
            "netmask": "255.255.255.0",
            "dhcp": {
                "enabled": true,
                "start": "192.168.4.2",
                "end": "192.168.4.50"
            }
        },
        "policy": "fallback"
    }
}
```

### Example 7: Dual Mode (STA + AP Always-On)

```json
{
    "enabled": true,
    "hostname": "esp32-gateway",
    "country": "US",
    "txPowerDbm": 20,
    "sleep": "none",
    "sta": {
        "enabled": true,
        "ssid": "InternetRouter",
        "password": "routerpass",
        "auth": "wpa2",
        "ipv4": {
            "dhcp": true
        }
    },
    "ap": {
        "enabled": true,
        "ssid": "ESP32-Hotspot",
        "password": "hotspot123",
        "auth": "wpa2",
        "channel": "auto",
        "maxClients": 8,
        "ipv4": {
            "address": "10.0.0.1",
            "gateway": "10.0.0.1",
            "netmask": "255.255.255.0",
            "dhcp": {
                "enabled": true,
                "start": "10.0.0.10",
                "end": "10.0.0.100"
            }
        },
        "policy": "always-on"
    }
}
```

### Example 8: Open Network (No Password)

```json
{
    "enabled": true,
    "ap": {
        "enabled": true,
        "ssid": "ESP32-Public",
        "auth": "open",
        "channel": 1,
        "maxClients": 4,
        "ipv4": {
            "address": "192.168.5.1",
            "gateway": "192.168.5.1",
            "netmask": "255.255.255.0",
            "dhcp": {
                "enabled": true,
                "start": "192.168.5.10",
                "end": "192.168.5.50"
            }
        },
        "policy": "always-on"
    }
}
```

## Default Values

When configuration values are omitted, the following defaults are applied:

### Global Defaults
- `enabled`: `true`
- `hostname`: `"esp32-XXXXXX"` (auto-generated from MAC)
- `country`: `"US"`
- `txPowerDbm`: `20`
- `sleep`: `"none"`

### STA Defaults
- `enabled`: `true`
- `auth`: `"wpa2"`
- `pmf`: `"optional"`
- `bssidLock`: `false`
- `ipv4.dhcp`: `true`
- `autoReconnect`: `true`

### AP Defaults
- `enabled`: `false`
- `auth`: `"wpa2"`
- `channel`: `0` (auto)
- `hidden`: `false`
- `maxClients`: `4`
- `pmf`: `"optional"`
- `ipv4.address`: `"192.168.4.1"`
- `ipv4.gateway`: `"192.168.4.1"`
- `ipv4.netmask`: `"255.255.255.0"`
- `ipv4.dhcp.enabled`: `true`
- `ipv4.dhcp.start`: `"192.168.4.2"`
- `ipv4.dhcp.end`: `"192.168.4.100"`
- `policy`: `"off"`

## API Reference

### WiFiManager Class

#### Public Methods

##### `bool begin(const ArduinoJson::JsonObjectConst& cfg)`
Initialize WiFi manager with JSON configuration.
- **Parameters**: JSON configuration object
- **Returns**: `true` on success, `false` on failure

##### `bool start()`
Start WiFi with the configured settings.
- **Returns**: `true` on success, `false` on failure

##### `void stop()`
Stop WiFi and cleanup resources.

##### `bool isSTAConnected() const`
Check if STA is connected to an access point.
- **Returns**: `true` if connected, `false` otherwise

##### `bool isAPActive() const`
Check if AP is currently active.
- **Returns**: `true` if active, `false` otherwise

##### `const WiFiConfig& getConfig() const`
Get the current WiFi configuration.
- **Returns**: Reference to WiFiConfig structure

## Error Handling

The WiFi Manager logs errors using ESP-IDF logging system. Common error scenarios:

1. **Invalid JSON**: Check that the configuration matches the schema
2. **Missing SSID**: STA requires an SSID when enabled
3. **Short Password**: WPA2/WPA3 requires passwords ≥8 characters
4. **Invalid IP**: Check IPv4 address format (e.g., "192.168.1.1")
5. **PMF Requirements**: WPA3 requires PMF to be "required"

## Best Practices

1. **Security**: Use WPA2 or WPA3 with strong passwords (≥12 characters)
2. **PMF**: Enable PMF (at least "optional") for better security
3. **Auto-reconnect**: Enable for reliable connections
4. **Fallback AP**: Use fallback policy for easier reconfiguration
5. **Static IP**: Use for servers/fixed devices to avoid DHCP delays
6. **Country Code**: Set correct country for regulatory compliance
7. **TX Power**: Reduce if devices are close to save power
8. **Sleep Mode**: Use "modem" or "light" for battery-powered devices

## Integration Example

```cpp
#include "wifi.hpp"
#include <SPIFFS.h>

WiFiManager wifi;

bool loadWiFiConfig() {
    File file = SPIFFS.open("/config.json", "r");
    if (!file) {
        ESP_LOGE("APP", "Failed to open config file");
        return false;
    }
    
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        ESP_LOGE("APP", "Failed to parse config: %s", error.c_str());
        return false;
    }
    
    JsonObjectConst wifiCfg = doc["modules"]["wifi"];
    
    if (!wifi.begin(wifiCfg)) {
        ESP_LOGE("APP", "Failed to initialize WiFi");
        return false;
    }
    
    return wifi.start();
}

void setup() {
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        ESP_LOGE("APP", "SPIFFS mount failed");
        return;
    }
    
    // Load and start WiFi
    if (loadWiFiConfig()) {
        ESP_LOGI("APP", "WiFi initialized successfully");
    }
}
```

## Notes

- The WiFi Manager automatically handles event loops and network interface initialization
- Configuration is validated against the schema requirements
- Event handlers are registered automatically for connection state management
- Memory is managed efficiently with RAII principles
- Thread-safe event handling through ESP-IDF event system

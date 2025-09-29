#ifndef CONFIG_H
#define CONFIG_H


#define UUID "504b1696-2ad5-4109-ac28-5158965d6675"

// WiFi Configuration
#define WIFI_SSID "Koderman"
#define WIFI_PASSWORD "kodermancki"

// MQTT Configuration
#define MQTT_BROKER "172.16.1.21"  // e.g. "192.168.1.100" or "mqtt.example.com"
#define MQTT_PORT 1883
#define MQTT_TOPIC_PREFIX "device"
#define MQTT_HEARTBEAT_TOPIC "hb"
#define MQTT_TELEMETRY_TOPIC "telemetry"
#define MQTT_CLIENT_ID "esp32s3_hb"


// Heartbeat Configuration
#define HEARTBEAT_INTERVAL 30000 // 30 seconds in milliseconds

// Device Configuration
#define DEVICE_NAME "ESP32-S3-DevKitC"

// mDNS Configuration
#define MDNS_HOSTNAME "esp32s3-heartbeat"
#define MDNS_INSTANCE_NAME "ESP32-S3 Heartbeat Monitor"
#define MDNS_SERVICE_TYPE "_heartbeat"
#define MDNS_PROTOCOL "_tcp"
#define MDNS_PORT 80  // HTTP port for web interface (if needed later)

// NTP Configuration
#define NTP_SERVER1 "172.16.0.1"
#define NTP_SERVER2 "time.nist.gov"
#define NTP_SERVER3 "time.google.com"
#define GMT_OFFSET_SEC 0        // UTC offset in seconds (0 for UTC)
#define DAYLIGHT_OFFSET_SEC 0   // Daylight saving time offset in seconds

#endif // CONFIG_H
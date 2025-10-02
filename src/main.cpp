#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <time.h>
#include <ESPmDNS.h>
#include "config.h"

// Global variables
WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastHeartbeat = 0;
uint32_t sequenceNumber = 0;
uint64_t bootId;
unsigned long bootTime;
String mqttTopicString;

// Function declarations
void setupWiFi();
void setupMQTT();
void setupNTP();
void setupMDNS();
void reconnectMQTT();
void sendHeartbeat();
uint64_t getTimestamp();
bool isTimeSet();

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32-S3 MQTT Heartbeat Starting ===");
    
    // Generate unique boot ID using chip ID and boot time
    bootId = ESP.getEfuseMac();
    bootTime = millis();
    
    // Build MQTT topic string
    mqttTopicString = String(MQTT_TOPIC_PREFIX) + "/" + String(UUID) + "/" + String(MQTT_HEARTBEAT_TOPIC);
    
    Serial.printf("Boot ID: %llx\n", bootId);
    Serial.printf("Device: %s\n", DEVICE_NAME);
    Serial.printf("MQTT Topic: %s\n", mqttTopicString.c_str());
    
    // Initialize WiFi and MQTT
    setupWiFi();
    setupNTP();
    setupMDNS();
    setupMQTT();
    
    Serial.println("=== Setup Complete ===\n");
}

void loop() {
    // Ensure MQTT connection is maintained
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    mqttClient.loop();
    
    // Send heartbeat every HEARTBEAT_INTERVAL
    unsigned long currentTime = millis();
    if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        sendHeartbeat();
        lastHeartbeat = currentTime;
    }
    
    delay(100); // Small delay to prevent watchdog issues
}

void setupWiFi() {
    Serial.printf("Connecting to WiFi network: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.printf("WiFi connected successfully!\n");
        Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Signal strength (RSSI): %d dBm\n", WiFi.RSSI());
    } else {
        Serial.println();
        Serial.println("Failed to connect to WiFi!");
        ESP.restart();
    }
}

void setupMQTT() {
    // Set MQTT buffer size to accommodate larger JSON payloads
    mqttClient.setBufferSize(1024);
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.publish_P(mqttTopicString.c_str(), 0, true, ""); // Retain empty message to create topic
    
    Serial.printf("MQTT buffer size set to: %d bytes\n", 1024);
    reconnectMQTT();
}

void setupNTP() {
    Serial.println("Setting up NTP time synchronization...");
    
    // Configure NTP
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
    
    Serial.print("Waiting for NTP time sync");
    int attempts = 0;
    while (!isTimeSet() && attempts < 30) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (isTimeSet()) {
        Serial.println();
        time_t now = time(nullptr);
        Serial.printf("NTP time synchronized: %s", ctime(&now));
    } else {
        Serial.println();
        Serial.println("Warning: Failed to synchronize NTP time!");
    }
}

void setupMDNS() {
    Serial.println("Setting up mDNS advertisement...");
    
    if (!MDNS.begin(MDNS_HOSTNAME)) {
        Serial.println("Error setting up mDNS responder!");
        return;
    }
    
    Serial.printf("mDNS responder started: %s.local\n", MDNS_HOSTNAME);
    
    // Add service advertisement
    MDNS.addService(MDNS_SERVICE_TYPE, MDNS_PROTOCOL, MDNS_PORT);
    
    // Add service instance name
    MDNS.setInstanceName(MDNS_INSTANCE_NAME);
    
    // Add useful TXT records for service discovery
    MDNS.addServiceTxt(MDNS_SERVICE_TYPE, MDNS_PROTOCOL, "device", DEVICE_NAME);
    MDNS.addServiceTxt(MDNS_SERVICE_TYPE, MDNS_PROTOCOL, "version", "1.0");
    MDNS.addServiceTxt(MDNS_SERVICE_TYPE, MDNS_PROTOCOL, "bootId", String(bootId, HEX));
    MDNS.addServiceTxt(MDNS_SERVICE_TYPE, MDNS_PROTOCOL, "mqttTopic", mqttTopicString.c_str());
    MDNS.addServiceTxt(MDNS_SERVICE_TYPE, MDNS_PROTOCOL, "heartbeatInterval", String(HEARTBEAT_INTERVAL / 1000) + "s");
    
    Serial.printf("mDNS service advertised: %s%s.local:%d\n", MDNS_SERVICE_TYPE, MDNS_PROTOCOL, MDNS_PORT);
    Serial.println("Device discoverable as: " + String(MDNS_HOSTNAME) + ".local");
}

void reconnectMQTT() {
    int attempts = 0;
    while (!mqttClient.connected() && attempts < 5) {
        Serial.printf("Attempting MQTT connection to %s:%d...\n", MQTT_BROKER, MQTT_PORT);
        Serial.printf("Client ID: %s\n", MQTT_CLIENT_ID);
        Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
        
        if (mqttClient.connect(MQTT_CLIENT_ID)) {
            Serial.println("MQTT connected successfully!");
            Serial.printf("MQTT state: %d\n", mqttClient.state());
            // Send an immediate heartbeat on successful connect/reconnect
            Serial.println("Sending immediate heartbeat after MQTT connect...");
            sendHeartbeat();
            break;
        } else {
            int state = mqttClient.state();
            Serial.printf("MQTT connection failed, rc=%d", state);
            
            // Decode MQTT error codes
            switch(state) {
                case -4: Serial.println(" (connection timeout)"); break;
                case -3: Serial.println(" (connection lost)"); break;
                case -2: Serial.println(" (connect failed)"); break;
                case -1: Serial.println(" (disconnected)"); break;
                case 1: Serial.println(" (bad protocol version)"); break;
                case 2: Serial.println(" (bad client ID)"); break;
                case 3: Serial.println(" (unavailable)"); break;
                case 4: Serial.println(" (bad credentials)"); break;
                case 5: Serial.println(" (unauthorized)"); break;
                default: Serial.println(" (unknown error)"); break;
            }
            
            Serial.printf("Retrying in 5 seconds... (attempt %d/5)\n", attempts + 1);
            delay(5000);
            attempts++;
        }
    }
    
    if (!mqttClient.connected()) {
        Serial.println("Failed to connect to MQTT broker after 5 attempts!");
        Serial.printf("WiFi status: %d\n", WiFi.status());
        Serial.printf("WiFi SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("WiFi IP: %s\n", WiFi.localIP().toString().c_str());
    }
}

void sendHeartbeat() {
    if (!mqttClient.connected()) {
        Serial.println("MQTT not connected, skipping heartbeat");
        return;
    }
    
    // Create JSON document with larger capacity
    StaticJsonDocument<768> doc;
    
    // Add heartbeat data
    doc["ts"] = getTimestamp();
    doc["timestampType"] = isTimeSet() ? "unix_epoch" : "boot_seconds";
    doc["bootId"] = String(bootId, HEX);
    doc["seq"] = sequenceNumber++;
    doc["uptimeS"] = (millis() - bootTime) / 1000;
    
    // Additional useful data
    doc["device"] = DEVICE_NAME;
    doc["hostname"] = String(MDNS_HOSTNAME) + ".local";
    doc["rssi"] = WiFi.RSSI();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["chipModel"] = ESP.getChipModel();
    
    // Check if JSON document has enough capacity
    if (doc.overflowed()) {
        Serial.println("ERROR: JSON document overflow! Increase StaticJsonDocument size.");
        return;
    }
    
    // Serialize JSON to string
    String jsonString;
    size_t jsonSize = serializeJson(doc, jsonString);
    
    if (jsonSize == 0) {
        Serial.println("ERROR: Failed to serialize JSON document!");
        return;
    }
    
    Serial.printf("JSON size: %zu bytes\n", jsonSize);
    Serial.printf("Free heap before publish: %u bytes\n", ESP.getFreeHeap());
    
    // Publish to MQTT
    bool success = mqttClient.publish(mqttTopicString.c_str(), jsonString.c_str());
    
    if (success) {
        Serial.printf("Heartbeat #%d sent successfully\n", sequenceNumber - 1);
        Serial.printf("Payload: %s\n", jsonString.c_str());
    } else {
        Serial.println("ERROR: Failed to send heartbeat!");
        Serial.printf("MQTT state: %d\n", mqttClient.state());
        Serial.printf("Topic: %s\n", mqttTopicString.c_str());
        Serial.printf("Payload length: %d\n", jsonString.length());
        
        // Try to reconnect MQTT if publish failed
        if (!mqttClient.connected()) {
            Serial.println("MQTT disconnected, attempting reconnection...");
            reconnectMQTT();
        }
    }
}

uint64_t getTimestamp() {
    if (isTimeSet()) {
        // Return Unix epoch timestamp in seconds
        return time(nullptr);
    } else {
        // Fallback to milliseconds since boot if NTP time not available
        return esp_timer_get_time() / 1000000; // Convert microseconds to seconds
    }
}

bool isTimeSet() {
    time_t now = time(nullptr);
    // Time is considered set if it's after year 2000 (Unix timestamp > 946684800)
    return now > 946684800;
}
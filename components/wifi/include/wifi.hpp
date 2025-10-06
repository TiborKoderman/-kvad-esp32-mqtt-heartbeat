#ifndef WIFI_MANAGER_HPP
#define WIFI_MANAGER_HPP

#include <string>
#include <cstring>

extern "C" {
    #include <esp_wifi.h>
    #include <esp_wifi_types.h>
    #include <esp_event.h>
    #include <esp_log.h>
    #include <esp_netif.h>
}

#include <ArduinoJson.h>

/**
 * WiFi Configuration structures matching wifi.schema.json
 */

struct IPv4Config {
    bool dhcp;
    esp_netif_ip_info_t ip_info;  // contains address, gateway, netmask
    esp_ip4_addr_t dns_primary;
    esp_ip4_addr_t dns_secondary;
    bool has_dns;
    
    IPv4Config() : dhcp(true), has_dns(false) {
        memset(&ip_info, 0, sizeof(ip_info));
        memset(&dns_primary, 0, sizeof(dns_primary));
        memset(&dns_secondary, 0, sizeof(dns_secondary));
    }
};

struct STAConfig {
    bool enabled;
    std::string ssid;
    std::string password;
    wifi_auth_mode_t auth;
    wifi_pmf_config_t pmf;
    bool bssid_lock;
    uint8_t bssid[6];
    IPv4Config ipv4;
    bool auto_reconnect;
    
    STAConfig() : enabled(true), auth(WIFI_AUTH_WPA2_PSK), 
                  bssid_lock(false), auto_reconnect(true) {
        memset(bssid, 0, sizeof(bssid));
        pmf.capable = true;
        pmf.required = false;
    }
};

struct DHCPServerConfig {
    bool enabled;
    esp_netif_ip_info_t lease_start;
    esp_netif_ip_info_t lease_end;
    
    DHCPServerConfig() : enabled(true) {
        memset(&lease_start, 0, sizeof(lease_start));
        memset(&lease_end, 0, sizeof(lease_end));
    }
};

struct APConfig {
    bool enabled;
    std::string ssid;
    std::string password;
    uint8_t channel;
    bool hidden;
    uint8_t max_clients;
    wifi_auth_mode_t auth;
    wifi_pmf_config_t pmf;
    esp_netif_ip_info_t ip_info;
    DHCPServerConfig dhcp_server;
    std::string policy;  // "always-on", "fallback", "off"
    
    APConfig() : enabled(false), channel(0), hidden(false), 
                 max_clients(4), auth(WIFI_AUTH_WPA2_PSK), policy("off") {
        memset(&ip_info, 0, sizeof(ip_info));
        pmf.capable = true;
        pmf.required = false;
    }
};

struct WiFiConfig {
    bool enabled;
    std::string hostname;
    std::string country;
    int8_t tx_power_dbm;
    wifi_ps_type_t sleep_mode;
    STAConfig sta;
    APConfig ap;
    
    WiFiConfig() : enabled(true), tx_power_dbm(20), sleep_mode(WIFI_PS_NONE) {}
};

/**
 * WiFi Manager Class
 * Manages WiFi STA and AP modes according to configuration
 */
class WiFiManager {
public:
    WiFiManager();
    ~WiFiManager();

    /**
     * Initialize and configure WiFi from JSON config
     * @param cfg JSON configuration object matching wifi.schema.json
     * @return true if initialization successful, false otherwise
     */
    bool begin(const ArduinoJson::JsonObjectConst& cfg);
    
    /**
     * Start WiFi with current configuration
     * @return true if started successfully
     */
    bool start();
    
    /**
     * Stop WiFi
     */
    void stop();
    
    /**
     * Check if STA is connected
     */
    bool isSTAConnected() const { return _sta_connected; }
    
    /**
     * Check if AP is active
     */
    bool isAPActive() const { return _ap_active; }
    
    /**
     * Get current configuration
     */
    const WiFiConfig& getConfig() const { return _config; }
    
    /**
     * Event handler for WiFi events
     */
    static void eventHandler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data);

private:
    WiFiConfig _config;
    esp_netif_t* _sta_netif;
    esp_netif_t* _ap_netif;
    bool _initialized;
    bool _sta_connected;
    bool _ap_active;
    esp_event_handler_instance_t _wifi_event_handler;
    esp_event_handler_instance_t _ip_event_handler;
    
    static constexpr const char* TAG = "WiFiManager";
    
    /**
     * Parse JSON configuration into WiFiConfig structure
     */
    bool parseConfig(const ArduinoJson::JsonObjectConst& cfg);
    
    /**
     * Parse STA configuration
     */
    bool parseSTAConfig(const ArduinoJson::JsonObjectConst& sta_json, STAConfig& sta);
    
    /**
     * Parse AP configuration
     */
    bool parseAPConfig(const ArduinoJson::JsonObjectConst& ap_json, APConfig& ap);
    
    /**
     * Parse IPv4 configuration
     */
    bool parseIPv4Config(const ArduinoJson::JsonObjectConst& ipv4_json, IPv4Config& ipv4);
    
    /**
     * Parse authentication mode from string
     */
    wifi_auth_mode_t parseAuthMode(const char* auth_str);
    
    /**
     * Parse PMF configuration from string
     */
    wifi_pmf_config_t parsePMFConfig(const char* pmf_str);
    
    /**
     * Parse sleep mode from string
     */
    wifi_ps_type_t parseSleepMode(const char* sleep_str);
    
    /**
     * Parse IPv4 address string to esp_ip4_addr_t
     */
    bool parseIPv4Address(const char* ip_str, esp_ip4_addr_t* addr);
    
    /**
     * Parse BSSID string (MAC address)
     */
    bool parseBSSID(const char* bssid_str, uint8_t* bssid);
    
    /**
     * Apply default configuration values
     */
    void applyDefaults();
    
    /**
     * Initialize ESP-IDF WiFi subsystem
     */
    bool initWiFi();
    
    /**
     * Configure STA mode
     */
    bool configureSTA();
    
    /**
     * Configure AP mode
     */
    bool configureAP();
    
    /**
     * Set WiFi country configuration
     */
    bool setCountryConfig();
    
    /**
     * Set TX power
     */
    bool setTxPower();
    
    /**
     * Handle STA connection
     */
    void handleSTAConnect();
    
    /**
     * Handle STA disconnection
     */
    void handleSTADisconnect();
    
    /**
     * Handle AP start
     */
    void handleAPStart();
    
    /**
     * Handle AP stop
     */
    void handleAPStop();
};

#endif // WIFI_MANAGER_HPP
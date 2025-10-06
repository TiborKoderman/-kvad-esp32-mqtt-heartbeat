#include "wifi.hpp"
#include <cstring>
#include <lwip/ip4_addr.h>
#include <esp_mac.h>

// Constructor
WiFiManager::WiFiManager() 
    : _sta_netif(nullptr), _ap_netif(nullptr), _initialized(false),
      _sta_connected(false), _ap_active(false),
      _wifi_event_handler(nullptr), _ip_event_handler(nullptr) {
}

// Destructor
WiFiManager::~WiFiManager() {
    stop();
}

// Parse authentication mode from string
wifi_auth_mode_t WiFiManager::parseAuthMode(const char* auth_str) {
    if (!auth_str) return WIFI_AUTH_WPA2_PSK;  // Default
    
    if (strcmp(auth_str, "open") == 0) return WIFI_AUTH_OPEN;
    if (strcmp(auth_str, "wpa2") == 0) return WIFI_AUTH_WPA2_PSK;
    if (strcmp(auth_str, "wpa3") == 0) return WIFI_AUTH_WPA3_PSK;
    
    ESP_LOGW(TAG, "Unknown auth mode '%s', defaulting to WPA2", auth_str);
    return WIFI_AUTH_WPA2_PSK;
}

// Parse PMF configuration from string
wifi_pmf_config_t WiFiManager::parsePMFConfig(const char* pmf_str) {
    wifi_pmf_config_t pmf;
    
    if (!pmf_str || strcmp(pmf_str, "optional") == 0) {
        pmf.capable = true;
        pmf.required = false;
    } else if (strcmp(pmf_str, "required") == 0) {
        pmf.capable = true;
        pmf.required = true;
    } else if (strcmp(pmf_str, "disabled") == 0) {
        pmf.capable = false;
        pmf.required = false;
    } else {
        ESP_LOGW(TAG, "Unknown PMF mode '%s', defaulting to optional", pmf_str);
        pmf.capable = true;
        pmf.required = false;
    }
    
    return pmf;
}

// Parse sleep mode from string
wifi_ps_type_t WiFiManager::parseSleepMode(const char* sleep_str) {
    if (!sleep_str || strcmp(sleep_str, "none") == 0) return WIFI_PS_NONE;
    if (strcmp(sleep_str, "modem") == 0) return WIFI_PS_MIN_MODEM;
    if (strcmp(sleep_str, "light") == 0) return WIFI_PS_MAX_MODEM;
    
    ESP_LOGW(TAG, "Unknown sleep mode '%s', defaulting to none", sleep_str);
    return WIFI_PS_NONE;
}

// Parse IPv4 address string
bool WiFiManager::parseIPv4Address(const char* ip_str, esp_ip4_addr_t* addr) {
    if (!ip_str || !addr) return false;
    
    ip4_addr_t ip4;
    if (ip4addr_aton(ip_str, &ip4) == 0) {
        ESP_LOGE(TAG, "Invalid IPv4 address: %s", ip_str);
        return false;
    }
    
    addr->addr = ip4.addr;
    return true;
}

// Parse BSSID (MAC address)
bool WiFiManager::parseBSSID(const char* bssid_str, uint8_t* bssid) {
    if (!bssid_str || !bssid) return false;
    
    int values[6];
    if (sscanf(bssid_str, "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != 6) {
        ESP_LOGE(TAG, "Invalid BSSID format: %s", bssid_str);
        return false;
    }
    
    for (int i = 0; i < 6; i++) {
        bssid[i] = (uint8_t)values[i];
    }
    
    return true;
}

// Parse IPv4 configuration
bool WiFiManager::parseIPv4Config(const ArduinoJson::JsonObjectConst& ipv4_json, IPv4Config& ipv4) {
    if (!ipv4_json) {
        // Use defaults
        ipv4.dhcp = true;
        return true;
    }
    
    ipv4.dhcp = ipv4_json["dhcp"] | true;
    
    if (!ipv4.dhcp) {
        // Static IP configuration required
        const char* addr_str = ipv4_json["address"];
        const char* gateway_str = ipv4_json["gateway"];
        const char* netmask_str = ipv4_json["netmask"] | "255.255.255.0";
        
        if (!addr_str || !gateway_str) {
            ESP_LOGE(TAG, "Static IP requires address and gateway");
            return false;
        }
        
        if (!parseIPv4Address(addr_str, &ipv4.ip_info.ip) ||
            !parseIPv4Address(gateway_str, &ipv4.ip_info.gw) ||
            !parseIPv4Address(netmask_str, &ipv4.ip_info.netmask)) {
            return false;
        }
        
        // Optional DNS servers
        ArduinoJson::JsonArrayConst dns_array = ipv4_json["dns"];
        if (dns_array && dns_array.size() > 0) {
            const char* dns1 = dns_array[0];
            if (dns1 && parseIPv4Address(dns1, &ipv4.dns_primary)) {
                ipv4.has_dns = true;
                
                if (dns_array.size() > 1) {
                    const char* dns2 = dns_array[1];
                    if (dns2) {
                        parseIPv4Address(dns2, &ipv4.dns_secondary);
                    }
                }
            }
        }
    }
    
    return true;
}

// Parse STA configuration
bool WiFiManager::parseSTAConfig(const ArduinoJson::JsonObjectConst& sta_json, STAConfig& sta) {
    if (!sta_json) {
        // STA disabled by default if not specified
        sta.enabled = false;
        return true;
    }
    
    sta.enabled = sta_json["enabled"] | true;
    
    if (!sta.enabled) {
        return true;
    }
    
    // SSID is required when enabled
    const char* ssid = sta_json["ssid"];
    if (!ssid) {
        ESP_LOGE(TAG, "STA enabled but no SSID provided");
        return false;
    }
    sta.ssid = ssid;
    
    // Password (optional for open networks)
    const char* password = sta_json["password"];
    if (password) {
        sta.password = password;
    }
    
    // Authentication mode
    const char* auth = sta_json["auth"] | "wpa2";
    sta.auth = parseAuthMode(auth);
    
    // PMF configuration
    const char* pmf = sta_json["pmf"] | "optional";
    sta.pmf = parsePMFConfig(pmf);
    
    // Enforce WPA3 requires PMF
    if (sta.auth == WIFI_AUTH_WPA3_PSK) {
        sta.pmf.required = true;
    }
    
    // BSSID lock
    sta.bssid_lock = sta_json["bssidLock"] | false;
    if (sta.bssid_lock) {
        const char* bssid_str = sta_json["bssid"];
        if (!bssid_str || !parseBSSID(bssid_str, sta.bssid)) {
            ESP_LOGE(TAG, "BSSID lock enabled but invalid/missing BSSID");
            return false;
        }
    }
    
    // IPv4 configuration
    if (!parseIPv4Config(sta_json["ipv4"], sta.ipv4)) {
        return false;
    }
    
    // Auto reconnect
    sta.auto_reconnect = sta_json["autoReconnect"] | true;
    
    return true;
}

// Parse AP configuration
bool WiFiManager::parseAPConfig(const ArduinoJson::JsonObjectConst& ap_json, APConfig& ap) {
    if (!ap_json) {
        ap.enabled = false;
        ap.policy = "off";
        return true;
    }
    
    ap.enabled = ap_json["enabled"] | false;
    ap.policy = ap_json["policy"] | "off";
    
    // If policy is "off", AP is disabled
    if (ap.policy == "off") {
        ap.enabled = false;
    }
    
    if (!ap.enabled && ap.policy == "off") {
        return true;
    }
    
    // SSID is required
    const char* ssid = ap_json["ssid"];
    if (!ssid) {
        ESP_LOGE(TAG, "AP configuration missing SSID");
        return false;
    }
    ap.ssid = ssid;
    
    // Authentication and password
    const char* auth = ap_json["auth"] | "wpa2";
    ap.auth = parseAuthMode(auth);
    
    const char* password = ap_json["password"];
    if (ap.auth != WIFI_AUTH_OPEN) {
        if (!password || strlen(password) < 8) {
            ESP_LOGE(TAG, "AP with WPA2/WPA3 requires password >= 8 chars");
            return false;
        }
        ap.password = password;
    }
    
    // PMF configuration
    const char* pmf = ap_json["pmf"] | "optional";
    ap.pmf = parsePMFConfig(pmf);
    
    // Enforce WPA3 requires PMF
    if (ap.auth == WIFI_AUTH_WPA3_PSK) {
        ap.pmf.required = true;
    }
    
    // Channel
    if (ap_json["channel"].is<int>()) {
        ap.channel = ap_json["channel"];
    } else {
        ap.channel = 0;  // Auto
    }
    
    // Other settings
    ap.hidden = ap_json["hidden"] | false;
    ap.max_clients = ap_json["maxClients"] | 4;
    
    // IPv4 configuration for AP
    ArduinoJson::JsonObjectConst ipv4_json = ap_json["ipv4"];
    if (ipv4_json) {
        const char* addr = ipv4_json["address"] | "192.168.4.1";
        const char* gateway = ipv4_json["gateway"] | "192.168.4.1";
        const char* netmask = ipv4_json["netmask"] | "255.255.255.0";
        
        if (!parseIPv4Address(addr, &ap.ip_info.ip) ||
            !parseIPv4Address(gateway, &ap.ip_info.gw) ||
            !parseIPv4Address(netmask, &ap.ip_info.netmask)) {
            ESP_LOGE(TAG, "Invalid AP IPv4 configuration");
            return false;
        }
        
        // DHCP server configuration
        ArduinoJson::JsonObjectConst dhcp_json = ipv4_json["dhcp"];
        if (dhcp_json) {
            ap.dhcp_server.enabled = dhcp_json["enabled"] | true;
            
            if (ap.dhcp_server.enabled) {
                const char* start = dhcp_json["start"] | "192.168.4.2";
                const char* end = dhcp_json["end"] | "192.168.4.100";
                
                parseIPv4Address(start, &ap.dhcp_server.lease_start.ip);
                parseIPv4Address(end, &ap.dhcp_server.lease_end.ip);
            }
        }
    } else {
        // Set default AP IP configuration
        parseIPv4Address("192.168.4.1", &ap.ip_info.ip);
        parseIPv4Address("192.168.4.1", &ap.ip_info.gw);
        parseIPv4Address("255.255.255.0", &ap.ip_info.netmask);
    }
    
    return true;
}

// Apply default configuration values
void WiFiManager::applyDefaults() {
    if (_config.hostname.empty()) {
        // Generate default hostname from MAC
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char hostname[32];
        snprintf(hostname, sizeof(hostname), "esp32-%02x%02x%02x",
                 mac[3], mac[4], mac[5]);
        _config.hostname = hostname;
    }
    
    if (_config.country.empty()) {
        _config.country = "US";  // Default regulatory domain
    }
}

// Parse JSON configuration
bool WiFiManager::parseConfig(const ArduinoJson::JsonObjectConst& cfg) {
    if (!cfg) {
        ESP_LOGE(TAG, "Invalid JSON configuration");
        return false;
    }
    
    // Master enable
    _config.enabled = cfg["enabled"] | true;
    
    if (!_config.enabled) {
        ESP_LOGW(TAG, "WiFi module disabled");
        return true;
    }
    
    // Hostname
    const char* hostname = cfg["hostname"];
    if (hostname) {
        _config.hostname = hostname;
    }
    
    // Country code
    const char* country = cfg["country"];
    if (country) {
        _config.country = country;
    }
    
    // TX Power
    _config.tx_power_dbm = cfg["txPowerDbm"] | 20;
    
    // Sleep mode
    const char* sleep = cfg["sleep"] | "none";
    _config.sleep_mode = parseSleepMode(sleep);
    
    // Parse STA configuration
    if (!parseSTAConfig(cfg["sta"], _config.sta)) {
        return false;
    }
    
    // Parse AP configuration
    if (!parseAPConfig(cfg["ap"], _config.ap)) {
        return false;
    }
    
    // Apply defaults for missing values
    applyDefaults();
    
    // Validate configuration
    if (!_config.sta.enabled && !_config.ap.enabled) {
        ESP_LOGW(TAG, "Both STA and AP are disabled");
    }
    
    return true;
}

// Initialize ESP-IDF WiFi subsystem
bool WiFiManager::initWiFi() {
    if (_initialized) {
        return true;
    }
    
    // Initialize TCP/IP adapter
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Set storage to RAM (config in NVS handled separately)
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiManager::eventHandler, this, &_wifi_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, ESP_EVENT_ANY_ID, &WiFiManager::eventHandler, this, &_ip_event_handler));
    
    _initialized = true;
    ESP_LOGI(TAG, "WiFi subsystem initialized");
    
    return true;
}

// Set WiFi country configuration
bool WiFiManager::setCountryConfig() {
    if (_config.country.empty()) {
        return true;
    }
    
    wifi_country_t country = {
        .cc = {0},
        .schan = 1,
        .nchan = 13,  // Will be adjusted based on country
        .max_tx_power = _config.tx_power_dbm,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    
    strncpy(country.cc, _config.country.c_str(), sizeof(country.cc) - 1);
    
    esp_err_t ret = esp_wifi_set_country(&country);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set country: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "WiFi country set to: %s", _config.country.c_str());
    return true;
}

// Set TX power
bool WiFiManager::setTxPower() {
    // Convert dBm to 0.25dBm units
    int8_t power = _config.tx_power_dbm * 4;
    
    esp_err_t ret = esp_wifi_set_max_tx_power(power);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set TX power: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "WiFi TX power set to: %d dBm", _config.tx_power_dbm);
    return true;
}

// Configure STA mode
bool WiFiManager::configureSTA() {
    if (!_config.sta.enabled) {
        return true;
    }
    
    ESP_LOGI(TAG, "Configuring STA mode");
    
    // Create STA network interface
    if (!_sta_netif) {
        _sta_netif = esp_netif_create_default_wifi_sta();
    }
    
    // Set hostname
    if (!_config.hostname.empty()) {
        esp_netif_set_hostname(_sta_netif, _config.hostname.c_str());
    }
    
    // Configure static IP if not using DHCP
    if (!_config.sta.ipv4.dhcp) {
        ESP_ERROR_CHECK(esp_netif_dhcpc_stop(_sta_netif));
        ESP_ERROR_CHECK(esp_netif_set_ip_info(_sta_netif, &_config.sta.ipv4.ip_info));
        
        // Set DNS servers if specified
        if (_config.sta.ipv4.has_dns) {
            esp_netif_dns_info_t dns_info;
            dns_info.ip.u_addr.ip4 = _config.sta.ipv4.dns_primary;
            dns_info.ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_set_dns_info(_sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);
            
            if (_config.sta.ipv4.dns_secondary.addr != 0) {
                dns_info.ip.u_addr.ip4 = _config.sta.ipv4.dns_secondary;
                esp_netif_set_dns_info(_sta_netif, ESP_NETIF_DNS_BACKUP, &dns_info);
            }
        }
        
        ESP_LOGI(TAG, "STA using static IP: " IPSTR, IP2STR(&_config.sta.ipv4.ip_info.ip));
    } else {
        esp_netif_dhcpc_start(_sta_netif);
        ESP_LOGI(TAG, "STA using DHCP");
    }
    
    // Configure WiFi STA settings
    wifi_config_t wifi_config = {};
    
    strncpy((char*)wifi_config.sta.ssid, _config.sta.ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, _config.sta.password.c_str(), sizeof(wifi_config.sta.password) - 1);
    
    wifi_config.sta.threshold.authmode = _config.sta.auth;
    wifi_config.sta.pmf_cfg = _config.sta.pmf;
    
    if (_config.sta.bssid_lock) {
        wifi_config.sta.bssid_set = true;
        memcpy(wifi_config.sta.bssid, _config.sta.bssid, sizeof(wifi_config.sta.bssid));
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    ESP_LOGI(TAG, "STA configured: SSID=%s, Auth=%d", _config.sta.ssid.c_str(), _config.sta.auth);
    
    return true;
}

// Configure AP mode
bool WiFiManager::configureAP() {
    if (!_config.ap.enabled || _config.ap.policy == "off") {
        return true;
    }
    
    ESP_LOGI(TAG, "Configuring AP mode");
    
    // Create AP network interface
    if (!_ap_netif) {
        _ap_netif = esp_netif_create_default_wifi_ap();
    }
    
    // Stop DHCP server first
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(_ap_netif));
    
    // Set AP IP configuration
    ESP_ERROR_CHECK(esp_netif_set_ip_info(_ap_netif, &_config.ap.ip_info));
    
    // Configure DHCP server if enabled
    if (_config.ap.dhcp_server.enabled) {
        // Set DHCP server IP range
        esp_netif_dhcp_option_id_t opt_id = ESP_NETIF_IP_REQUEST_RETRY_TIME;
        
        // In ESP-IDF 5.x, DHCP server range configuration uses esp_netif_dhcps_option
        // with ESP_NETIF_SUBNET_MASK option or automatic range based on netmask
        uint32_t lease_start = _config.ap.dhcp_server.lease_start.ip.addr;
        uint32_t lease_end = _config.ap.dhcp_server.lease_end.ip.addr;
        
        // Set IP address range using the ESP_NETIF_DOMAIN_NAME_SERVER option as workaround
        // For ESP-IDF 5.x, the DHCP server automatically allocates based on the subnet mask
        // So we just start the DHCP server
        ESP_ERROR_CHECK(esp_netif_dhcps_start(_ap_netif));
        ESP_LOGI(TAG, "DHCP server enabled (start: " IPSTR ", end: " IPSTR ")",
                 IP2STR(&_config.ap.dhcp_server.lease_start.ip),
                 IP2STR(&_config.ap.dhcp_server.lease_end.ip));
    }
    
    // Configure WiFi AP settings
    wifi_config_t wifi_config = {};
    
    strncpy((char*)wifi_config.ap.ssid, _config.ap.ssid.c_str(), sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = _config.ap.ssid.length();
    
    if (!_config.ap.password.empty()) {
        strncpy((char*)wifi_config.ap.password, _config.ap.password.c_str(), sizeof(wifi_config.ap.password) - 1);
    }
    
    wifi_config.ap.channel = _config.ap.channel;
    wifi_config.ap.authmode = _config.ap.auth;
    wifi_config.ap.ssid_hidden = _config.ap.hidden ? 1 : 0;
    wifi_config.ap.max_connection = _config.ap.max_clients;
    wifi_config.ap.pmf_cfg = _config.ap.pmf;
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    
    ESP_LOGI(TAG, "AP configured: SSID=%s, Channel=%d, Auth=%d", 
             _config.ap.ssid.c_str(), _config.ap.channel, _config.ap.auth);
    
    return true;
}

// Begin WiFi with configuration
bool WiFiManager::begin(const ArduinoJson::JsonObjectConst& cfg) {
    ESP_LOGI(TAG, "Starting WiFi Manager");
    
    // Parse configuration
    if (!parseConfig(cfg)) {
        ESP_LOGE(TAG, "Failed to parse configuration");
        return false;
    }
    
    if (!_config.enabled) {
        ESP_LOGW(TAG, "WiFi disabled in configuration");
        return false;
    }
    
    // Initialize WiFi subsystem
    if (!initWiFi()) {
        return false;
    }
    
    // Determine WiFi mode
    wifi_mode_t mode = WIFI_MODE_NULL;
    
    if (_config.sta.enabled && (_config.ap.enabled && _config.ap.policy == "always-on")) {
        mode = WIFI_MODE_APSTA;
    } else if (_config.sta.enabled) {
        mode = WIFI_MODE_STA;
    } else if (_config.ap.enabled) {
        mode = WIFI_MODE_AP;
    }
    
    if (mode == WIFI_MODE_NULL) {
        ESP_LOGW(TAG, "No WiFi mode enabled");
        return false;
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    ESP_LOGI(TAG, "WiFi mode set to: %d", mode);
    
    // Set power save mode
    ESP_ERROR_CHECK(esp_wifi_set_ps(_config.sleep_mode));
    
    // Configure interfaces
    if (_config.sta.enabled) {
        if (!configureSTA()) {
            return false;
        }
    }
    
    if (_config.ap.enabled && _config.ap.policy == "always-on") {
        if (!configureAP()) {
            return false;
        }
    }
    
    // Set country and TX power
    setCountryConfig();
    setTxPower();
    
    ESP_LOGI(TAG, "WiFi Manager initialized successfully");
    
    return true;
}

// Start WiFi
bool WiFiManager::start() {
    if (!_initialized) {
        ESP_LOGE(TAG, "WiFi not initialized, call begin() first");
        return false;
    }
    
    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "WiFi started");
    
    // Connect STA if enabled
    if (_config.sta.enabled) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Connecting to SSID: %s", _config.sta.ssid.c_str());
    }
    
    return true;
}

// Stop WiFi
void WiFiManager::stop() {
    if (_initialized) {
        esp_wifi_stop();
        esp_wifi_deinit();
        
        if (_wifi_event_handler) {
            esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, _wifi_event_handler);
            _wifi_event_handler = nullptr;
        }
        
        if (_ip_event_handler) {
            esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, _ip_event_handler);
            _ip_event_handler = nullptr;
        }
        
        _initialized = false;
        _sta_connected = false;
        _ap_active = false;
        
        ESP_LOGI(TAG, "WiFi stopped");
    }
}

// Event handler
void WiFiManager::eventHandler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    WiFiManager* manager = static_cast<WiFiManager*>(arg);
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA started");
                break;
                
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi STA connected");
                manager->handleSTAConnect();
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = 
                    static_cast<wifi_event_sta_disconnected_t*>(event_data);
                ESP_LOGW(TAG, "WiFi STA disconnected (reason: %d)", event->reason);
                manager->handleSTADisconnect();
                
                // Auto-reconnect if enabled
                if (manager->_config.sta.auto_reconnect) {
                    ESP_LOGI(TAG, "Attempting reconnect...");
                    esp_wifi_connect();
                }
                break;
            }
            
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "WiFi AP started");
                manager->handleAPStart();
                break;
                
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "WiFi AP stopped");
                manager->handleAPStop();
                break;
                
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = 
                    static_cast<wifi_event_ap_staconnected_t*>(event_data);
                ESP_LOGI(TAG, "Client connected to AP: " MACSTR, MAC2STR(event->mac));
                break;
            }
            
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = 
                    static_cast<wifi_event_ap_stadisconnected_t*>(event_data);
                ESP_LOGI(TAG, "Client disconnected from AP: " MACSTR, MAC2STR(event->mac));
                break;
            }
            
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);
                ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
                manager->_sta_connected = true;
                break;
            }
            
            case IP_EVENT_STA_LOST_IP:
                ESP_LOGW(TAG, "Lost IP address");
                manager->_sta_connected = false;
                break;
                
            default:
                break;
        }
    }
}

// Handle STA connection
void WiFiManager::handleSTAConnect() {
    // Additional connection handling can be added here
}

// Handle STA disconnection
void WiFiManager::handleSTADisconnect() {
    _sta_connected = false;
    
    // If AP fallback policy is active and AP is not running, start it
    if (_config.ap.policy == "fallback" && !_ap_active) {
        ESP_LOGI(TAG, "STA disconnected, starting fallback AP");
        
        wifi_mode_t current_mode;
        esp_wifi_get_mode(&current_mode);
        
        if (current_mode == WIFI_MODE_STA) {
            esp_wifi_set_mode(WIFI_MODE_APSTA);
            configureAP();
        }
    }
}

// Handle AP start
void WiFiManager::handleAPStart() {
    _ap_active = true;
}

// Handle AP stop
void WiFiManager::handleAPStop() {
    _ap_active = false;
}

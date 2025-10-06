#pragma once
#include <esp_vfs.h>
#include "esp_littlefs.h"
#include <esp_log.h>
#include <ArduinoJson.h>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <algorithm>

class ConfigManager
{
private:
  static constexpr const char* TAG = "ConfigManager";
  
public:
  bool init(const char *jsonPath = "/config.json", const char *mpkPath = "/config.mpk", size_t jsonPoolCapacity = 0)
  {
    _jsonPath = jsonPath;
    _mpkPath = mpkPath;

    // Initialize LittleFS
    esp_vfs_littlefs_conf_t conf = {
      .base_path = "/",
      .partition_label = "littlefs",
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "Failed to mount or format filesystem");
      } else if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to find LittleFS partition");
      } else {
        ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
      }
      return false;
    }

    // Fast path: load MessagePack if present
    if (fileExists(_mpkPath))
    {
      if (loadMsgPack())
        return true;
      ESP_LOGW(TAG, "MsgPack load failed, trying JSON...");
    }

    // Slow path: load JSON and (re)write MsgPack
    if (!fileExists(_jsonPath))
    {
      ESP_LOGE(TAG, "config.json not found at %s", _jsonPath);
      return false;
    }
    if (!loadJson(jsonPoolCapacity))
      return false;
    if (!saveMsgPack())
    {
      ESP_LOGW(TAG, "Warning: failed to write config.msgpack");
    }
    return true;
  }

  // Root of the configuration (read-only view)
  JsonVariantConst root() const
  {
    if (!_doc)
      return JsonVariantConst();
    return _doc->as<JsonVariantConst>();
  }

  JsonVariantConst get(const char *dottedPath) const
  {
    if (!_doc)
      return JsonVariantConst();
    
    // Simple dotted path traversal
    JsonVariantConst current = _doc->as<JsonVariantConst>();
    char pathCopy[256];
    strncpy(pathCopy, dottedPath, sizeof(pathCopy) - 1);
    pathCopy[sizeof(pathCopy) - 1] = '\0';
    
    char* token = strtok(pathCopy, ".");
    while (token && !current.isNull()) {
      current = current[token];
      token = strtok(nullptr, ".");
    }
    return current;
  }

  template <typename T>
  T getOr(const char *dottedPath, const T &fallback) const
  {
    JsonVariantConst v = get(dottedPath);
    if (v.isNull())
      return fallback;
    T out = fallback;
    v.as<T &>(out);
    return out;
  }

  // Free all JSON memory (invalidate all outstanding JsonVariant/JsonObject refs)
  void dispose()
  {
    _doc.reset();
  }

  bool isLoaded() const { return (bool)_doc; }

  bool saveMsgPack() const
  {
    if (!_doc)
      return false;
    
    // Calculate size needed for MessagePack serialization
    size_t size = measureMsgPack(*_doc);
    if (size == 0) {
      ESP_LOGE(TAG, "Failed to measure MsgPack size");
      return false;
    }
    
    // Allocate buffer for serialization
    uint8_t* buffer = (uint8_t*)malloc(size);
    if (!buffer) {
      ESP_LOGE(TAG, "Failed to allocate %zu bytes for MsgPack", size);
      return false;
    }
    
    // Serialize to buffer
    size_t written = serializeMsgPack(*_doc, buffer, size);
    if (written == 0 || written > size) {
      ESP_LOGE(TAG, "MsgPack serialization failed");
      free(buffer);
      return false;
    }
    
    // Write buffer to file
    FILE* f = fopen(_mpkPath, "wb");
    if (!f) {
      ESP_LOGE(TAG, "Failed to open %s for writing", _mpkPath);
      free(buffer);
      return false;
    }
    
    size_t bytes_written = fwrite(buffer, 1, written, f);
    fflush(f);
    fclose(f);
    free(buffer);
    
    if (bytes_written != written) {
      ESP_LOGE(TAG, "Failed to write complete MsgPack file");
      return false;
    }
    
    ESP_LOGI(TAG, "Saved MsgPack config (%zu bytes)", bytes_written);
    return true;
  }

private:
  // Helper function to check if file exists
  bool fileExists(const char* path) const
  {
    FILE* f = fopen(path, "r");
    if (f) {
      fclose(f);
      return true;
    }
    return false;
  }

  // Load MessagePack into RAM
  bool loadMsgPack()
  {
    FILE* f = fopen(_mpkPath, "rb");
    if (!f) {
      ESP_LOGW(TAG, "Failed to open %s for reading", _mpkPath);
      return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    size_t fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize == 0) {
      ESP_LOGW(TAG, "MsgPack file is empty");
      fclose(f);
      return false;
    }

    // Read file into buffer
    uint8_t* buffer = (uint8_t*)malloc(fileSize);
    if (!buffer) {
      ESP_LOGE(TAG, "Failed to allocate %zu bytes for MsgPack", fileSize);
      fclose(f);
      return false;
    }

    size_t bytes_read = fread(buffer, 1, fileSize, f);
    fclose(f);

    if (bytes_read != fileSize) {
      ESP_LOGE(TAG, "Failed to read complete MsgPack file");
      free(buffer);
      return false;
    }

    // Estimate JSON document size (MsgPack is more compact, so add extra space)
    const size_t est = (fileSize > 8 * 1024) ? fileSize * 2 + 2048 : 8 * 1024;
    _doc.reset(new DynamicJsonDocument(est));
    
    // Deserialize from buffer
    DeserializationError err = deserializeMsgPack(*_doc, buffer, fileSize);
    free(buffer);

    if (err)
    {
      ESP_LOGE(TAG, "MsgPack parse error: %s", err.c_str());
      _doc.reset();
      return false;
    }
    
    ESP_LOGI(TAG, "Loaded MsgPack config (%zu bytes)", fileSize);
    return true;
  }

  bool loadJson(size_t capacityOverride = 0)
  {
    FILE* f = fopen(_jsonPath, "r");
    if (!f) {
      ESP_LOGE(TAG, "Failed to open %s for reading", _jsonPath);
      return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    size_t fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize == 0) {
      ESP_LOGE(TAG, "JSON file is empty");
      fclose(f);
      return false;
    }

    // Read file into buffer
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
      ESP_LOGE(TAG, "Failed to allocate %zu bytes for JSON", fileSize + 1);
      fclose(f);
      return false;
    }

    size_t bytes_read = fread(buffer, 1, fileSize, f);
    fclose(f);
    buffer[bytes_read] = '\0';  // Null-terminate

    if (bytes_read != fileSize) {
      ESP_LOGE(TAG, "Failed to read complete JSON file");
      free(buffer);
      return false;
    }

    // Determine capacity
    size_t cap = capacityOverride;
    if (cap == 0)
    {
      // Heuristic: 1.3x file size + 4 KB headroom
      cap = (fileSize > 8 * 1024) ? (size_t)(fileSize * 13 / 10) + 4096 : 8 * 1024;
    }

    _doc.reset(new DynamicJsonDocument(cap));
    
    // Deserialize from buffer
    DeserializationError err = deserializeJson(*_doc, buffer);
    free(buffer);

    if (err)
    {
      ESP_LOGE(TAG, "JSON parse error: %s", err.c_str());
      _doc.reset();
      return false;
    }
    
    ESP_LOGI(TAG, "Loaded JSON config (%zu bytes)", fileSize);
    return true;
  }

private:
  std::unique_ptr<DynamicJsonDocument> _doc;
  const char *_jsonPath = "/config.json";
  const char *_mpkPath = "/config.msgpack";
};
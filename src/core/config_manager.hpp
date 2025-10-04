#pragma once
#include <LittleFS.h>
#include <ArduinoJson.h>

class ConfigManager
{
public:
  bool init(const char *jsonPath = "/config.json", const char *mpkPath = "/config.mpk", size_t jsonPoolCapacity = 0)
  {
    _jsonPath = jsonPath;
    _mpkPath = mpkPath;
    if (!LittleFS.begin(true))
    {
      Serial.println("[Config] LittleFS mount failed");
      return false;
    }

    // Fast path: load MessagePack if present
    if (LittleFS.exists(_mpkPath))
    {
      if (loadMsgPack())
        return true;
      Serial.println("[Config] MsgPack load failed, trying JSON…");
    }

    // Slow path: load JSON and (re)write MsgPack
    if (!LittleFS.exists(_jsonPath))
    {
      Serial.println("[Config] /config.json not found");
      return false;
    }
    if (!loadJson(jsonPoolCapacity))
      return false;
    if (!saveMsgPack())
    {
      Serial.println("[Config] Warning: failed to write /config.msgpack");
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
    File f = LittleFS.open(_mpkPath, "w");
    if (!f)
      return false;
    const auto n = serializeMsgPack(*_doc, f);
    f.flush();
    f.close();
    return n > 0;
  }

private:
  // Load MessagePack into RAM
  bool loadMsgPack()
  {
    File f = LittleFS.open(_mpkPath, "r");
    if (!f)
      return false;

    // If you know your config size, set a fixed capacity; otherwise estimate from file size.
    const size_t est = max<size_t>(8 * 1024, f.size() + 2048);
    _doc.reset(new DynamicJsonDocument(est));
    DeserializationError err = deserializeMsgPack(*_doc, f);
    f.close();

    if (err)
    {
      Serial.printf("[Config] MsgPack parse error: %s\n", err.c_str());
      _doc.reset();
      return false;
    }
    return true;
  }

  bool loadJson(size_t capacityOverride = 0)
  {
    File f = LittleFS.open(_jsonPath, "r");
    if (!f)
      return false;

    size_t cap = capacityOverride;
    if (cap == 0)
    {
      // Heuristic: 1.3x file size + 4 KB headroom
      const size_t sz = f.size();
      cap = max<size_t>(8 * 1024, (size_t)(sz * 13 / 10) + 4096);
    }

    _doc.reset(new DynamicJsonDocument(cap));
    DeserializationError err = deserializeJson(*_doc, f);
    f.close();

    if (err)
    {
      Serial.printf("[Config] JSON parse error: %s\n", err.c_str());
      _doc.reset();
      return false;
    }
    return true;
  }

private:
  std::unique_ptr<DynamicJsonDocument> _doc;
  const char *_jsonPath = "/config.json";
  const char *_mpkPath = "/config.msgpack";
};
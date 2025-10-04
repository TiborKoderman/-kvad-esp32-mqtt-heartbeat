#pragma once
#include "ctx.hpp"
#include "include/fsm.hpp"
#include <esp_err.h>
#include "nvs_flash.h"
#include <esp_log.h>

extern const StateDesc<Ctx> ST_READY, ST_ERR;

static void enter(Ctx* ctx) {
  //check nvs and set context state accordingly
  esp_err_t ret = nvs_flash_init();
  ctx->nvs_ok = (ret == ESP_OK);
  if (!ctx->nvs_ok) {
    ESP_LOGE("init", "NVS init failed: %s", esp_err_to_name(ret));
  }


}



const StateDesc<Ctx> ST_INIT{
"INIT", enter, nullptr, nullptr, nullptr, nullptr
};
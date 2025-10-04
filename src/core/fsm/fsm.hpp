#pragma once
#include <stdint.h>
#include "include/fsm"

struct FsmEvent {
    uint16_t id;
    uint16_t param;
    const void* ptr;
};
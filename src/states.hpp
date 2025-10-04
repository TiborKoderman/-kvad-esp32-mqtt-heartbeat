#pragma once

#include "ctx.hpp"
#include "fsm.hpp"

// Forward declarations of all states
extern const StateDesc<Ctx> ST_INIT;
extern const StateDesc<Ctx> ST_NOMINAL;
extern const StateDesc<Ctx> ST_ERR;
extern const StateDesc<Ctx> ST_DEGRADED;
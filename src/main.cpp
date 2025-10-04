#include "main.hpp"
#include "ctx.hpp"
#include "fsm.hpp"
#include "states.hpp"

static const char* TAG = "MAIN";

extern "C" void app_main(void)
{
    Ctx ctx{};
    Fsm<Ctx> fsm(&ctx, &ST_INIT);
    fsm.start();
}
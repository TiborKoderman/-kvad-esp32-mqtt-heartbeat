#include "main.hpp"
#include "ctx.hpp"
#include "core/fsm/states/init.hpp"

static const char* TAG = "LFS";

extern "C" void app_main(void)
{
    Ctx ctx{};
    Fsm<Ctx> fsm(&ctx, &ST_INIT);
    fsm.start();
}
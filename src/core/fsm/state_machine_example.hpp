#pragma once
#include "ctx.hpp"
#include "include/fsm.hpp"

// Include all state implementations
#include "init.hpp"
#include "error.hpp"
#include "degraded.hpp"
#include "nominal.hpp"

// State machine example usage
// This shows how to set up and run the complete state machine

namespace StateMachineExample {

// Create the state machine with initial state
Fsm<Ctx> createStateMachine(Ctx* context) {
    return Fsm<Ctx>(context, &ST_INIT);
}

// Example main loop integration
void runStateMachine(Fsm<Ctx>* fsm) {
    // Start the state machine
    fsm->start();
    
    // Main application loop
    while (true) {
        uint32_t now_ms = esp_timer_get_time() / 1000;
        
        // Process state machine
        fsm->tick(now_ms);
        
        // Example of sending events
        // fsm->dispatch(1); // Send event ID 1
        
        // Sleep for a bit
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms
    }
}

} // namespace StateMachineExample
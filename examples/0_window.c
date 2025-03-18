#include "example.h"
// open a quick window
int main(void) {
    pinc_preinit_set_error_callback(example_error_callback);
    pinc_incomplete_init();
    // We don't care what we get, so don't set anything.
    // Everything is left default.
    if(pinc_complete_init(pinc_window_backend_any, pinc_graphics_api_any, 0, 1, 0) == pinc_return_code_error) {
        // Something went wrong. The error callback should have been called.
        return 100;
    }
    pinc_window window = pinc_window_create_incomplete();
    // Enter 0 for length so pinc does the work of finding the length for us
    pinc_window_set_title(window, "Minimal Pinc Window!", 0);
    if(pinc_window_complete(window) == pinc_return_code_error) {
        // Something went wrong. The error callback should have been called.
        return 100;
    }
    bool running = true;
    while(running) {
        // Over the course of a step, Pinc will collect all events for your application.
        // The events you get are the ones collected since the last call to step, in a sort of double-buffered type way.
        // So if you receive an event right after step(), you will not see it until the next call to step().
        pinc_step();
        // Then we can iterate the events
        uint32_t num_events = pinc_event_get_num();
        for(uint32_t i=0; i<num_events; ++i) {
            switch(pinc_event_get_type(i)) {
                case pinc_event_type_close_signal: {
                    // Just in case there are other windows that we don't know about
                    if(window == pinc_event_close_signal_window(i)) {
                        running = false;
                        printf("Closed window\n");
                    }
                    break;
                }
            }
        }
        pinc_window_present_framebuffer(window);
    }
    pinc_window_deinit(window);
    pinc_deinit();
}

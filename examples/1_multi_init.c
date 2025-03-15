// This example demonstrates that Pinc can be fully initialized and used, deinitialized, and initialized again in the same process.

#include "example.h"
// open a quick window
int main(void) {
    // Do it three times
    for(int i=0; i<3; ++i){
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
            pinc_step();
            if(pinc_event_window_closed(window)) {
                running = false;
            }
            pinc_window_present_framebuffer(window);
        }
        pinc_window_deinit(window);
        pinc_deinit();
    }
}

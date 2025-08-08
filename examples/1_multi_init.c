// This example demonstrates that Pinc can be fully initialized and used, deinitialized, and initialized again in the same process.

#include "example.h"
#include "pinc.h"
// open a quick window
int main(void) {
    // Do it three times
    for(int i=0; i<3; ++i){
        pincPreinitSetErrorCallback(exampleErrorCallback);
        pincInitIncomplete();
        // We don't care what we get, so don't set anything.
        // Everything is left default.
        pincInitComplete(PincWindowBackend_any, PincGraphicsApi_any, 0);
        PincWindowHandle window = pincWindowCreateIncomplete();
        // Enter 0 for length so pinc does the work of finding the length for us
        pincWindowSetTitle(window, "Minimal Pinc Window!", 0);
        pincWindowComplete(window);
        bool running = true;
        while(running) {
            pincStep();
            uint32_t num_events = pincEventGetNum();
            for(uint32_t i=0; i<num_events; ++i) {
                switch(pincEventGetType(i)) {
                    case PincEventType_closeSignal: {
                        // Just in case there are other windows that we don't know about
                        if(window == pincEventCloseSignalWindow(i)) {
                            running = false;
                            printf("Closed window\n");
                        }
                        break;
                    }
                }
            }
            pincWindowPresentFramebuffer(window);
        }
        pincWindowDeinit(window);
        pincDeinit();
    }
}

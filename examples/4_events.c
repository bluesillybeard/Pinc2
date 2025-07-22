#include "example.h"
#include "pinc.h"

// open a quick window
int main(void) {
    pincPreinitSetErrorCallback(exampleErrorCallback);
    pincInitIncomplete();
    // We don't care what we get, so don't set anything.
    // Everything is left default.
    if(pincInitComplete(PincWindowBackend_any, PincGraphicsApi_any, 0) == PincReturnCode_error) {
        // Something went wrong. The error callback should have been called.
        return 100;
    }
    PincWindowHandle window = pincWindowCreateIncomplete();
    // Enter 0 for length so pinc does the work of finding the length for us
    pincWindowSetTitle(window, "Minimal Pinc Window!", 0);
    if(pincWindowComplete(window) == PincReturnCode_error) {
        // Something went wrong. The error callback should have been called.
        return 100;
    }
    bool running = true;
    while(running) {
        // Over the course of a step, Pinc will collect all events for your application.
        // The events you get are the ones collected since the last call to step, in a sort of double-buffered type way.
        // So if you receive an event right after step(), you will not see it until the next call to step().
        pincStep();
        // Then we can iterate the events
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
                case PincEventType_clipboardChanged: {
                    switch(pincEventClipboardChangedMediaType(i)) {
                        case PincMediaType_unknown: {
                            printf("The user copied something unknown\n");
                            break;
                        }
                        case PincMediaType_text: {
                            printf("the user copied some text: %s\n", pincEventClipboardChangedData(i));
                            break;
                        }
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

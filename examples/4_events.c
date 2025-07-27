#include "example.h"
#include "pinc.h"
#include <stdio.h>

int main(void) {
    pincPreinitSetErrorCallback(exampleErrorCallback);
    pincInitIncomplete();
    if(pincInitComplete(PincWindowBackend_any, PincGraphicsApi_any, 0) == PincReturnCode_error) {
        return 100;
    }
    PincWindowHandle window = pincWindowCreateIncomplete();
    pincWindowSetTitle(window, "Minimal Pinc Window!", 0);
    if(pincWindowComplete(window) == PincReturnCode_error) {
        return 100;
    }
    bool running = true;
    while(running) {
        // Over the course of a step, Pinc will collect all events for your application.
        // Events are buffered into a back buffer, where the Pinc frontend API only makes the front buffer visible.
        // On step(), the buffers are swapped, the back buffer reset, then step() returns back to the application code.
        // In the future, Pinc may support an immediate-mode event loop for event based applications.
        pincStep();
        // Then we can iterate the events
        uint32_t num_events = pincEventGetNum();
        for(uint32_t i=0; i<num_events; ++i) {
            switch(pincEventGetType(i)) {
                case PincEventType_closeSignal: {
                    PincWindowHandle window_closed = pincEventCloseSignalWindow(i);
                    printf("Window %i was signalled to close.\n", window_closed);
                    // Follow through with closing the window, as long as it's actually the right one
                    if(window_closed == window) {
                        running = false;
                    }
                    break;
                }
                case PincEventType_mouseButton: {
                    uint32_t state = pincEventMouseButtonState(i);
                    uint32_t old_state = pincEventMouseButtonOldState(i);
                    // Checking each button individually is generally a good way to do things
                    char const* button_names[] = {
                        "left",
                        "right",
                        "middle",
                        "back",
                        "front",
                    };
                    for(uint32_t button_index=0; button_index<5; button_index++) {
                        // Isolate the bit for this button
                        char const* button_name = button_names[button_index];
                        bool button_state = ((1 << button_index) & state) != 0;
                        bool old_button_state = ((1 << button_index) & old_state) != 0;

                        printf("Mouse %s button went from being ", button_name);
                        if(old_button_state) {
                            printf("down to ");
                        } else {
                            printf("up to ");
                        }

                        if(button_state) {
                            printf("down.\n");
                        } else {
                            printf("up.\n");
                        }
                    }
                    break;
                }
                case PincEventType_resize: {
                    PincWindowHandle resized_window = pincEventResizeWindow(i);
                    uint32_t old_width = pincEventResizeOldWidth(i);
                    uint32_t old_height = pincEventResizeOldHeight(i);
                    uint32_t width = pincEventResizeWidth(i);
                    uint32_t height = pincEventResizeHeight(i);
                    printf("Window %i was resized from %ix%i to %ix%i\n", resized_window, old_width, old_height, width, height);
                    break;
                }
                case PincEventType_focus: {
                    PincWindowHandle focused_window = pincEventFocusWindow(i);
                    PincWindowHandle old_focused_window = pincEventFocusOldWindow(i);
                    printf("Window focus changed from %i to %i\n", old_focused_window, focused_window);
                    break;
                }
                case PincEventType_exposure: {
                    PincWindowHandle exposed_window = pincEventExposureWindow(i);
                    uint32_t exposed_x = pincEventExposureX(i);
                    uint32_t exposed_y = pincEventExposureY(i);
                    uint32_t exposed_width = pincEventExposureWidth(i);
                    uint32_t exposed_height = pincEventExposureHeight(i);
                    printf("Window %i was exposed at (%i, %i), %ix%i\n", exposed_window, exposed_x, exposed_y, exposed_width, exposed_height);
                    break;
                }
                case PincEventType_keyboardButton: {
                    PincKeyboardKey key = pincEventKeyboardButtonKey(i);
                    // the old state is not at all required,
                    // as it will the opposite of the current state, or the same if repeated
                    bool state = pincEventKeyboardButtonState(i);
                    bool repeat = pincEventKeyboardButtonRepeat(i);
                    char const* key_name = pincKeyboardKeyName(key);
                    
                    bool old_state = state == repeat;
                    printf("Key %s went from state %i to %i\n", key_name, (int)old_state, (int)state);
                    break;
                }
                case PincEventType_cursorMove: {
                    PincWindowHandle move_window = pincEventCursorMoveWindow(i);
                    uint32_t move_x = pincEventCursorMoveX(i);
                    uint32_t move_y = pincEventCursorMoveY(i);
                    uint32_t move_old_x = pincEventCursorMoveOldX(i);
                    uint32_t move_old_y = pincEventCursorMoveOldY(i);

                    printf("Cursor moved from (%i, %i) to (%i, %i) within window %i\n", move_old_x, move_old_y, move_x, move_y, move_window);
                    break;
                }
                case PincEventType_cursorTransition: {
                    PincWindowHandle trans_old_window = pincEventCursorTransitionOldWindow(i);
                    uint32_t old_x = pincEventCursorTransitionOldX(i);
                    uint32_t old_y = pincEventCursorTransitionOldY(i);
                    PincWindowHandle trans_window = pincEventCursorTransitionWindow(i);
                    uint32_t x = pincEventCursorTransitionX(i);
                    uint32_t y = pincEventCursorTransitionY(i);

                    printf("Cursor moved from (%i, %i) in window %i to (%i, %i) in window %i\n", old_x, old_y, trans_old_window, x, y, trans_window);
                    break;
                }
                case PincEventType_textInput: {
                    printf("User typed %c", pincEventTextInputCodepoint(i));
                    break;
                }
                case PincEventType_scroll: {
                    float vertical = pincEventScrollVertical(i);
                    float horizontal = pincEventScrollHorizontal(i);
                    printf("User scrolled %f units vertically and %f units horizontally\n", vertical, horizontal);
                    break;
                }
                case PincEventType_clipboardChanged: {
                    if(pincEventClipboardChangedMediaType(i) == PincMediaType_text) {
                        char const* clipboard = pincEventClipboardChangedData(i);
                        printf("The clipboard text was changed to %s\n", clipboard);
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

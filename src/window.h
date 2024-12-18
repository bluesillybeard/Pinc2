// Pinc window interface.

#include <pinc.h>
#include "pinc_main.h"

typedef struct {
    uint32_t channels;
    uint32_t channel_bits[4];
    uint32_t depth_buffer_bits;
    pinc_color_space color_space;
    uint32_t max_samples;
} FramebufferFormat;

#define PINC_WINDOW_INTERFACE_TEMP_MACRO(type, name, declare)
// This macro calls the "PINC_WINDOW_INTERFACE_TEMP_MACRO" for every window interface field
#define PINC_WINDOW_INTERFACE \
    PINC_WINDOW_INTERFACE_TEMP_MACRO(bool (*) (void), isSupported, bool (*isSupported) (void))

#undef PINC_WINDOW_INTERFACE_TEMP_MACRO

// Classic interfaces in C.
#define PINC_WINDOW_INTERFACE_TEMP_MACRO(type, name, declare) declare;
typedef struct {
    PINC_WINDOW_INTERFACE
} WindowBackend;
#undef PINC_WINDOW_INTERFACE_TEMP_MACRO

#include <pinc.h>
#include "platform/platform.h"
#include "pinc_main.h"

pinc_error_callback pfn_pinc_error_callback = 0;
pinc_error_callback pfn_pinc_panic_callback = 0;
pinc_error_callback pfn_pinc_user_error_callback = 0;

pinc_alloc_callback pfn_pinc_alloc = 0;
pinc_alloc_aligned_callback pfn_pinc_alloc_aligned = 0;
pinc_realloc_callback pfn_pinc_realloc = 0;
pinc_free_callback pfn_pinc_free = 0;

PINC_EXPORT void PINC_CALL pinc_preinit_set_error_callback(pinc_error_callback callback) {
    pfn_pinc_error_callback = callback;
}

PINC_EXPORT void PINC_CALL pinc_preinit_set_panic_callback(pinc_error_callback callback) {
    pfn_pinc_panic_callback = callback;
}

PINC_EXPORT void PINC_CALL pinc_preinit_set_user_error_callback(pinc_error_callback callback) {
    pfn_pinc_user_error_callback = callback;
}

PINC_EXPORT void PINC_CALL pinc_preinit_set_alloc_callbacks(pinc_alloc_callback alloc, pinc_alloc_aligned_callback alloc_aligned, pinc_realloc_callback realloc, pinc_free_callback free) {
    pfn_pinc_alloc = alloc;
    pfn_pinc_alloc_aligned = alloc_aligned;
    pfn_pinc_realloc = realloc;
    pfn_pinc_free = free;
}



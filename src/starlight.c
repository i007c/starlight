
#include "starlight.h"

static const char *STARLIGHT_STATUS_STRING[STARLIGHT_S_LENGTH] = {
    [STARLIGHT_S_SUCCESS] = "success",
    [STARLIGHT_S_UNKNOWN_FORMAT] = "unknown image format",
    [STARLIGHT_S_CORRUPT_DATA] = "data is corrupted",
    [STARLIGHT_S_OUTPUT_DATA_IS_NULL] = "output data is not allocated",
    [STARLIGHT_S_BUFFER_IS_NULL] = "buffer is not allocated",
    [STARLIGHT_S_MALLOC_FAILED] = "malloc faild.",
};

const char *starlight_status_string(starlight_status_t status) {
    if (status >= STARLIGHT_S_LENGTH) return "unknown status";
    return STARLIGHT_STATUS_STRING[status];
}

starlight_status_t starlight_load(Starlight *starlight) {

    starlight->cursor = starlight->data;

    if (starlight_png_check(starlight)) {
        return starlight_png_load_header(starlight);
    } 

    return STARLIGHT_S_UNKNOWN_FORMAT;
}


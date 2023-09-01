
#include "starlight.h"

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


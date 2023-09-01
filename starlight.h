
#ifndef __LIB_STARLIGHT__
#define __LIB_STARLIGHT__

#include <stdint.h>
#include <stdbool.h>

#include <stdio.h> // printf only

typedef enum {
    STARLIGHT_S_SUCCESS = 0,
    STARLIGHT_S_UNKNOWN_FORMAT,
    STARLIGHT_S_CORRUPT_DATA,
    STARLIGHT_S_LENGTH,
} starlight_status_t;

static const char *STARLIGHT_STATUS_STRING[STARLIGHT_S_LENGTH] = {
    [STARLIGHT_S_SUCCESS] = "success",
    [STARLIGHT_S_UNKNOWN_FORMAT] = "unknown image format",
    [STARLIGHT_S_CORRUPT_DATA] = "data is corrupted",
};

typedef enum {
    STARLIGHT_F_PNG = 10,
    STARLIGHT_F_LENGTH,
} starlight_image_format_t;

typedef struct starlight_output_t {
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    uint8_t *data;
} StarlightOutput;

typedef struct starlight_png_detail_t {
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t compression_method;
    uint8_t filter_method;
    uint8_t interlace_method;
} StarlightPngDetail;

typedef struct starlight_t {
    uint8_t *data;
    uint64_t length;
    uint8_t *cursor;

    starlight_image_format_t format;

    StarlightPngDetail png_detail;

    uint32_t width;
    uint32_t height;

    StarlightOutput output;

    starlight_status_t (*loader)(struct starlight_t *starlight);
} Starlight;


/* starlight { */
starlight_status_t starlight_load(Starlight *starlight);
const char *starlight_status_string(starlight_status_t status);
/* } */

/* png { */
bool starlight_png_check(Starlight *starlight);
starlight_status_t starlight_png_load_header(Starlight *starlight);
starlight_status_t starlight_png_loader(Starlight *starlight);
/* } */

#endif // __LIB_STARLIGHT__


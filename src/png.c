
#include "starlight.h"


bool starlight_png_check(Starlight *starlight) {
    starlight->cursor += 8;

    return (
        starlight->data[0] == 0x89 &&
        starlight->data[1] == 0x50 &&
        starlight->data[2] == 0x4E &&
        starlight->data[3] == 0x47 &&
        starlight->data[4] == 0x0D &&
        starlight->data[5] == 0x0A &&
        starlight->data[6] == 0x1A &&
        starlight->data[7] == 0x0A
    );
}


starlight_status_t starlight_png_load_header(Starlight *starlight) {
    uint32_t chunk_length = starlight_u32_be(starlight);
    uint32_t chunk_type = starlight_u32_be(starlight);

    if (chunk_length != 13 || chunk_type != 0x49484452) {
        return STARLIGHT_S_CORRUPT_DATA;
    }

    starlight->format = STARLIGHT_F_PNG;

    starlight->width = starlight_u32_be(starlight);
    starlight->height = starlight_u32_be(starlight);

    uint8_t bit_depth = *starlight->cursor++;
    uint8_t color_type = *starlight->cursor++;

    starlight->png_detail.bit_depth = bit_depth;
    starlight->png_detail.color_type = color_type;

    starlight->png_detail.compression_method = *starlight->cursor++;
    starlight->png_detail.filter_method = *starlight->cursor++;
    starlight->png_detail.interlace_method = *starlight->cursor++;

    switch (color_type) {
        case 2:
        case 4:
        case 6:
            if (bit_depth != 8 && bit_depth != 16) {
                return STARLIGHT_S_CORRUPT_DATA;
            }
        break;

        case 3:
            if (
                bit_depth != 8 && bit_depth != 4 &&
                bit_depth != 2 && bit_depth != 1
            ) return STARLIGHT_S_CORRUPT_DATA;
        break;

        case 0:
            if (
                bit_depth != 8 && bit_depth != 4 &&
                bit_depth != 2 && bit_depth != 1 && bit_depth != 16
            ) return STARLIGHT_S_CORRUPT_DATA;
        break;

        default:
            return STARLIGHT_S_CORRUPT_DATA;
    }

    // 17 = 4 byte chunk type + 13 byte chunk data
    if (
        starlight_calc_crc(starlight->cursor - 17, 17) !=
        starlight_u32_be(starlight)
    ) {
        return STARLIGHT_S_CORRUPT_DATA;
    }

    starlight->output.x = 0;
    starlight->output.y = 0;
    starlight->output.width = starlight->width;
    starlight->output.height = starlight->height;

    starlight->buffer_length = 0;
    starlight->buffer_moved = false;

    uint8_t *cursor_position = starlight->cursor;

    while (starlight->cursor < &starlight->data[starlight->length - 1]) {
        uint32_t chunk_length = starlight_u32_be(starlight);
        uint32_t crc = starlight_calc_crc(starlight->cursor, 4 + chunk_length);
        uint32_t chunk_type = starlight_u32_be(starlight);
        starlight->cursor += chunk_length;

        if (chunk_type == 0x49444154) {
            starlight->buffer_length += chunk_length;
        }

        if (crc != starlight_u32_be(starlight)) {
            return STARLIGHT_S_CORRUPT_DATA;
        }
    }

    starlight->cursor = cursor_position;
    starlight->loader = starlight_png_loader;
    return STARLIGHT_S_SUCCESS;
}

starlight_status_t starlight_png_loader(Starlight *starlight) {
    starlight_status_t status = STARLIGHT_S_SUCCESS;

    if (starlight->output.data == NULL)
        return STARLIGHT_S_OUTPUT_DATA_IS_NULL;

    if (starlight->buffer == NULL)
        return STARLIGHT_S_BUFFER_IS_NULL;

    while (starlight->cursor < &starlight->data[starlight->length - 1]) {
        uint32_t chunk_length = starlight_u32_be(starlight);
        uint32_t chunk_type = starlight_u32_be(starlight);

        switch (chunk_type) {
            case 0x49444154: { // IDAT
                memcpy(starlight->buffer, starlight->cursor, chunk_length);
                starlight->cursor += chunk_length;

            } break;

            case 0x49454E44: { // IEND
                status = starlight_inflate(starlight);
                printf("inflate status: %s\n", starlight_status_string(status));
                return status;
            } break;

            default: {
                uint8_t *cta = (uint8_t *)&chunk_type;
                printf(
                    "ignoring chunk type: 0x%X - %c%c%c%c\n",
                    chunk_type, cta[3], cta[2], cta[1], cta[0]
                );

                if ((cta[3] >> 5) & 1) {
                    // ignore the ancillary chunk and it CRC
                    starlight->cursor += chunk_length;
                } else {
                    return STARLIGHT_S_CORRUPT_DATA;
                }
            } break;
        }

        starlight->cursor += 4;
    }

    return STARLIGHT_S_SUCCESS;
}


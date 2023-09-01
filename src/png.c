
#include "starlight.h"
#include "common.h"


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
    // if (starlight->length - starlight->cursor < 25)
    //     return STARLIGHT_CORRUPT_DATA;

    uint32_t chunk_length = u32_be(starlight);
    uint32_t chunk_type = u32_be(starlight);

    if (chunk_length != 13 || chunk_type != 0x49484452) {
        return STARLIGHT_S_CORRUPT_DATA;
    }

    starlight->format = STARLIGHT_F_PNG;

    starlight->width = u32_be(starlight);
    starlight->height = u32_be(starlight);

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

    uint32_t icrc = calc_crc(starlight->cursor - 13, 13);
    uint32_t ocrc = u32_be(starlight);

    printf();

    if (calc_crc(starlight->cursor - 13, 13) != u32_be(starlight)) {
        return STARLIGHT_S_CORRUPT_DATA;
    }

    starlight->loader = starlight_png_loader;
    return STARLIGHT_S_SUCCESS;
}

starlight_status_t starlight_png_loader(Starlight *starlight) {

    while (starlight->cursor < &starlight->data[starlight->length - 1]) {
        uint32_t chunk_length = u32_be(starlight);
        uint32_t chunk_type = u32_be(starlight);

        switch (chunk_type) {
            default: {
                uint8_t *cta = (uint8_t *)&chunk_type;
                if (true || (cta[3] >> 5) & 1) {
                    // ignore the ancillary chunk and it CRC
                    starlight->cursor += chunk_length + 4;
                    printf(
                        "ignoring chunk type: %x - %c%c%c%c\n",
                        chunk_type, cta[3], cta[2], cta[1], cta[0]
                    );
                    continue;

                } else {
                    return STARLIGHT_S_CORRUPT_DATA;
                }
            } break;
        }

    }

    return STARLIGHT_S_SUCCESS;
}


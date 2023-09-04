
#include "starlight.h"

#include <stdlib.h>
#include <time.h>

static starlight_status_t reconstruct(Starlight *starlight);

static uint32_t starlight_abs(int32_t value) {
    return value < 0 ? -value : value;
}

static uint32_t u32_be(StarlightBuffer *buffer) {
    uint8_t a = *buffer->c++;
    uint8_t b = *buffer->c++;
    uint8_t c = *buffer->c++;
    uint8_t d = *buffer->c++;
    return (a << 24) + (b << 16) + (c << 8) + (d);
}

bool starlight_png_check(Starlight *starlight) {
    starlight->raw.c = starlight->raw.s + 8;

    return (
        starlight->raw.s[0] == 0x89 &&
        starlight->raw.s[1] == 0x50 &&
        starlight->raw.s[2] == 0x4E &&
        starlight->raw.s[3] == 0x47 &&
        starlight->raw.s[4] == 0x0D &&
        starlight->raw.s[5] == 0x0A &&
        starlight->raw.s[6] == 0x1A &&
        starlight->raw.s[7] == 0x0A
    );
}

starlight_status_t starlight_png_load_header(Starlight *starlight) {
    StarlightBuffer *input = &starlight->raw;

    uint32_t chunk_length = u32_be(input);
    uint32_t chunk_type = u32_be(input);

    if (chunk_length != 13 || chunk_type != 0x49484452) {
        return STARLIGHT_S_CORRUPT_DATA;
    }

    starlight->format = STARLIGHT_F_PNG;

    starlight->width = u32_be(input);
    starlight->height = u32_be(input);

    uint8_t bit_depth = *input->c++;
    uint8_t color_type = *input->c++;

    starlight->png.bit_depth = bit_depth;
    starlight->png.color_type = color_type;

    starlight->png.compression_method = *input->c++;
    starlight->png.filter_method = *input->c++;
    starlight->png.interlace_method = *input->c++;

    starlight->png.bpp = 4;
    if (color_type == 2) {
        starlight->png.bpp = 3;
    }

    if (color_type == 0 || color_type == 3 || color_type == 4)
        return STARLIGHT_S_NOT_IMPLEMENTED;

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
        starlight_calc_crc(input->c - 17, 17) !=
        u32_be(input)
    ) {
        return STARLIGHT_S_CORRUPT_DATA;
    }

    starlight->output.x = 0;
    starlight->output.y = 0;
    starlight->output.width = starlight->width;
    starlight->output.height = starlight->height;

    starlight->out.l= (
        (starlight->width * starlight->height * 4) + starlight->height
    );

    printf(
        "bit depth: %d - color type: %d\n",
        starlight->png.bit_depth, starlight->png.color_type
    );

    starlight->cmp.l = 0;
    starlight->buffer_moved = false;

    uint8_t *cursor_position = input->c;

    while (input->c < input->e - 8) {
        uint32_t chunk_length = u32_be(input);
        uint32_t crc = starlight_calc_crc(input->c, 4 + chunk_length);
        uint32_t chunk_type = u32_be(input);
        input->c += chunk_length;

        if (chunk_type == 0x49444154) {
            printf("cmp.l: %ld\n", starlight->cmp.l);
            starlight->cmp.l += chunk_length;
        }

        uint32_t chunk_crc = u32_be(input);
        printf(
            "chunk length: %u - chunk crc: %u - crc: %u\n"
            "data left: \33[32m%ld\33[m\n",
            chunk_length, chunk_crc, crc,
            input->e - input->c
        );

        if (crc != chunk_crc) {
            return STARLIGHT_S_CORRUPT_DATA;
        }

        // if (chunk_type == 0x49454E44) break;
    }


    input->c = cursor_position;
    starlight->loader = starlight_png_loader;
    return STARLIGHT_S_SUCCESS;
}

starlight_status_t starlight_png_loader(Starlight *starlight) {

    clock_t st_all = clock();
    printf("all start: \33[32m%ld\33[m\n", st_all);

    starlight_status_t status = STARLIGHT_S_SUCCESS;
    StarlightBuffer *input = &starlight->raw;

    if (starlight->out.s == NULL)
        return STARLIGHT_S_OUTPUT_DATA_IS_NULL;

    starlight->out.c = starlight->out.s;
    starlight->out.e = starlight->out.s + starlight->out.l;

    if (starlight->cmp.s == NULL)
        return STARLIGHT_S_BUFFER_IS_NULL;

    starlight->cmp.c = starlight->cmp.s;
    starlight->cmp.e = starlight->cmp.s + starlight->cmp.l;

    while (input->c < input->e - 8) {
        uint32_t chunk_length = u32_be(input);
        uint32_t chunk_type = u32_be(input);

        switch (chunk_type) {
            case 0x49444154: { // IDAT
                memcpy(starlight->cmp.c, input->c, chunk_length);
                starlight->cmp.c += chunk_length;
                input->c += chunk_length;

            } break;

            case 0x49454E44: { // IEND
                clock_t st_inflate = clock();
                printf("inflate start: \33[32m%ld\33[m\n", st_inflate);
                status = starlight_inflate(&starlight->cmp, &starlight->out);
                if (status)
                    return status;

                clock_t et_inflate = clock();
                printf(
                    "inflate took: \33[33m%ld\33[m\n",
                    et_inflate - st_inflate
                );

                clock_t st_recon = clock();
                printf("reconstruct start: \33[32m%ld\33[m\n", st_recon);
                if ((status = reconstruct(starlight)))
                    return status;

                clock_t et_recon = clock();
                printf(
                    "reconstruc took: \33[33m%ld\33[m\n",
                    et_recon - st_recon
                );

                // return STARLIGHT_S_SUCCESS;
            } break;

            default: {
                uint8_t *cta = (uint8_t *)&chunk_type;
                printf(
                    "ignoring chunk type: 0x%X - %c%c%c%c\n",
                    chunk_type, cta[3], cta[2], cta[1], cta[0]
                );

                if ((cta[3] >> 5) & 1) {
                    // ignore the ancillary chunk and it CRC
                    input->c += chunk_length;
                } else {
                    return STARLIGHT_S_CORRUPT_DATA;
                }
            } break;
        }

        input->c += 4;
    }

    clock_t et_all = clock();

    printf("all took: \33[33m%ld\33[m\n", et_all - st_all);

    return STARLIGHT_S_SUCCESS;
}

static starlight_status_t reconstruct(Starlight *starlight) {

    uint8_t *data = starlight->out.s;

    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t w = starlight->width;
    uint32_t h = starlight->height;
    uint8_t bpp = starlight->png.bpp;

    uint64_t i = 0;
    uint64_t o = 0;

    uint8_t filter_type = data[0]; i++;
    printf("first filter_type: %d\n", filter_type);

    if (filter_type == 0) {
        for (; x < w * bpp; x++, o++, i++) data[o] = data[i];
    } else if (filter_type == 1) {
        for (; x < bpp; x++, o++, i++) data[o] = data[i];
        for (; x < w * bpp; x++, o++, i++)
            data[o] = data[i] + data[o - bpp];
    } else if (filter_type == 2) {
        for (; x < w * bpp; x++, o++, i++) data[o] = data[i];
    } else if (filter_type == 3) {
        for (; x < bpp; x++, i++, o++) data[o] = data[i];
        for (; x < w * bpp; x++, i++, o++) {
            uint8_t a = data[o - bpp];
            data[o] = data[i] + (a >> 1);
        }
    } else if (filter_type == 4) {
        for (; x < bpp; x++, i++, o++) data[o] = data[i];
        for (; x < w * bpp; x++, i++, o++)
            data[o] = data[i] + data[o - bpp];
    } else {
        return STARLIGHT_S_CORRUPT_DATA;
    }

    for (y = 1; y < h; y++) {
        filter_type = data[i]; i++;
        x = 0;

        if (filter_type == 0) {
            for (; x < w * bpp; x++, i++, o++) data[o] = data[i];
        } else if (filter_type == 1) {
            for (; x < bpp; x++, i++, o++) data[o] = data[i];
            for (; x < w * bpp; x++, i++, o++)
                data[o] = data[i] + data[o - bpp];
        } else if (filter_type == 2) {
            for (; x < w * bpp; x++, i++, o++)
                data[o] = data[i] + data[o - w * bpp];
        } else if (filter_type == 3) {
            for (; x < bpp; x++, i++, o++) {
                uint8_t b = data[o - w * bpp];
                data[o] = data[i] + (b >> 1);
            }
            for (; x < w * bpp; x++, i++, o++) {
                uint8_t a = data[o - bpp];
                uint8_t b = data[o - w * bpp];
                data[o] = data[i] + ((a + b) >> 1);
            }
        } else if (filter_type == 4) {
            for (; x < bpp; x++, i++, o++)
                data[o] = data[i] + data[o - w * bpp];

            for (; x < w * bpp; x++, i++, o++) {
                uint8_t a = data[o - bpp];
                uint8_t b = data[o - w * bpp];
                uint8_t c = data[o - w * bpp - bpp];

                int32_t  p = a + b - c;

                int32_t pa = starlight_abs(p - a);
                int32_t pb = starlight_abs(p - b);
                int32_t pc = starlight_abs(p - c);

                int32_t pr;

                if (pa <= pb && pa <= pc) {
                    pr = a;
                } else if (pb <= pc) {
                    pr = b;
                } else {
                    pr = c;
                }

                data[o] = data[i] + pr;
            }
        } else {
            printf("unknown filter: %d, x: %d, y: %d\n", filter_type, x, y);
            // printf("f: %d - %d - %d\n", *f, *(f + 1), *(f + 2));
            return STARLIGHT_S_CORRUPT_DATA;
        }
    }

    if (bpp == 4) return STARLIGHT_S_SUCCESS;

    clock_t st_reorder = clock();

    uint8_t *temp = malloc(w * h * bpp);

    i = 0;
    o = 0;

    memcpy(temp, data, w * h * bpp);

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            data[o+0] = temp[i+0];
            data[o+1] = temp[i+1];
            data[o+2] = temp[i+2];
            data[o+3] = 255;

            i += 3;
            o += 4;
        }
    }

    free(temp);

    clock_t et_reorder = clock();
    printf(
        "reorder took: \33[33m%ld\33[m\n",
        et_reorder - st_reorder
    );

    return STARLIGHT_S_SUCCESS;
}


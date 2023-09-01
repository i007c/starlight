

#include "starlight.h"

#include <stdlib.h> // mallocing the tree nodes

typedef struct TreeNode {
    struct TreeNode *left;
    struct TreeNode *right;
    int32_t value;
} TreeNode;

static starlight_status_t decode_fixed(Starlight *starlight);
static starlight_status_t decode_dynamic(Starlight *starlight);
static starlight_status_t build_tree(
    uint8_t *code_lengths, uint16_t array_length, TreeNode *root
);
static starlight_status_t parse_data(
    Starlight *starlight, TreeNode *ll_root, TreeNode *dist_root
);

#define log(fmt, ...) printf("L:\33[33m%4d\33[m | "fmt" \33[30m%d\33[m\n", __LINE__, __VA_ARGS__)


starlight_status_t starlight_inflate(Starlight *starlight) {
    starlight_status_t status = STARLIGHT_S_SUCCESS;

    uint8_t *cursor_position = starlight->cursor;
    starlight->cursor = starlight->buffer;

    uint8_t cfm = *starlight->cursor++;
    uint8_t flg = *starlight->cursor++;
    if (flg & 32 || (cfm * 256 + flg) % 31) {
        return STARLIGHT_S_CORRUPT_DATA;
    }

    starlight->png_detail.z_comp_method = cfm & 15;
    starlight->png_detail.z_win_size = 1 << (8 + (cfm >> 4));
    starlight->png_detail.z_comp_level = flg >> 6;

    bool final = false;
    uint8_t type = 0;

    while (!final) {
        final = starlight_read_bit(starlight);
        type = starlight_read_bits_be(starlight, 2);

        log("final: %d - type: %d", final, type, 0);

        if (type == 0) {
            if (starlight->bit_idx) {
                starlight->bit_idx = 0;
                starlight->cursor++;
            }

            uint16_t data_len = starlight_u16_be(starlight);
            uint16_t data_nlen = starlight_u16_be(starlight);
            if (data_len != ((~data_nlen) & 0xffff))
                return STARLIGHT_S_CORRUPT_DATA;

            // uint16_t data_len = (
            //     read_bits_be(starlight, 8) | (read_bits_be(starlight, 8) << 8)
            // );
            // uint16_t data_len = (
            //     read_bits_be(starlight, 8) | (read_bits_be(starlight, 8) << 8)
            // );
            // uint16_t data_nlen = read_bits(8) | (read_bits(8) << 8);

            memcpy(starlight->output.data, starlight->cursor, data_len);
            starlight->output.data += data_len;
            starlight->cursor += data_len;
        } else if (type == 1) {
            if ((status = decode_fixed(starlight)))
                return status;
        } else if (type == 2) {
            if ((status = decode_dynamic(starlight)))
                return status;
        } else {
            return STARLIGHT_S_CORRUPT_DATA;
        }
    }

    starlight->cursor = cursor_position;
    return STARLIGHT_S_SUCCESS;
}


static const uint8_t CL_ORDER[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static const uint16_t LENGTH_BASE[31] = {
   3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 
   15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 
   67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0
};

static const uint8_t LENGTH_EXTRA[31] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4,
    5, 5, 5, 5, 0, 0, 0
};

static const uint32_t DIST_BASE[32] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577, 0, 0
};

static const uint8_t DIST_EXTRA[32] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static uint8_t FIXED_LENGTHS[288] = {
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, 7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8
};

static uint8_t FIXED_DIST[32] = {
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
};


static starlight_status_t decode_fixed(Starlight *starlight) {
    starlight_status_t status = STARLIGHT_S_SUCCESS;

    TreeNode ll_root;
    if ((status = build_tree(FIXED_LENGTHS, 288, &ll_root)))
        return status;

    TreeNode dist_root;
    if ((status = build_tree(FIXED_DIST, 32, &dist_root)))
        return status;

    return parse_data(starlight, &ll_root, &dist_root);
}


static starlight_status_t decode_dynamic(Starlight *starlight) {
    starlight_status_t status = STARLIGHT_S_SUCCESS;

    uint8_t hlit = starlight_read_bits_be(starlight, 5);
    uint16_t num_ll_codes = hlit + 257;

    uint8_t hdist = starlight_read_bits_be(starlight, 5);
    uint8_t num_dist_codes = hdist + 1;

    uint8_t hclen = starlight_read_bits_be(starlight, 4);
    uint8_t num_cl_codes = hclen + 4;

    uint8_t cl_code_lengths[19];
    memset(cl_code_lengths, 0, sizeof(cl_code_lengths));

    for (uint8_t ci = 0; ci < num_cl_codes; ci++) {
        cl_code_lengths[CL_ORDER[ci]] = starlight_read_bits_be(starlight, 3);
    }

    TreeNode cl_root;
    if ((status = build_tree(cl_code_lengths, 19, &cl_root))) {
        log("cl root build tree faild", 0);
        return status;
    }



    uint8_t ll_code_lengths[288];
    uint8_t dist_code_lengths[32];
    memset(ll_code_lengths, 0, sizeof(ll_code_lengths));
    memset(dist_code_lengths, 0, sizeof(dist_code_lengths));

    uint8_t repeat_count = 0;
    uint16_t repeat_symbol = 0;
    int32_t symbol = 0;

    TreeNode *node = &cl_root;

    for (uint16_t ix=0; ix < num_ll_codes + num_dist_codes;) {
        uint8_t bd = starlight_read_bit(starlight);

        node = bd ? node->right : node->left;
        if (node == NULL) {
            log("node is null", 0);
            return STARLIGHT_S_CORRUPT_DATA;
        }

        if (node->value == -1) continue; 

        symbol = node->value;
        node = &cl_root;
        if (symbol < 0 || symbol > 18)
            return STARLIGHT_S_CORRUPT_DATA;
        

        if (symbol <= 15) {
            if (ix >= num_ll_codes) {
                dist_code_lengths[ix - num_ll_codes] = symbol;
            } else {
                ll_code_lengths[ix] = symbol;
            }
            ix++;
            repeat_count = 0;
            repeat_symbol = symbol;
        } else if (symbol == 16) {
            repeat_count = starlight_read_bits_be(starlight, 2) + 3;
        } else if (symbol == 17) {
            repeat_count = starlight_read_bits_be(starlight, 3) + 3;
            repeat_symbol = 0;
        } else if (symbol == 18) {
            repeat_count = starlight_read_bits_be(starlight, 7) + 11;
            repeat_symbol = 0;
        } else {
            return STARLIGHT_S_CORRUPT_DATA;
        }

        for (uint8_t ih=0; ih < repeat_count; ih++) {
            if (ix >= num_ll_codes) {
                dist_code_lengths[ix - num_ll_codes] = repeat_symbol;
            } else {
                ll_code_lengths[ix] = repeat_symbol;
            }
            ix++;
        }
    }

    TreeNode ll_root;
    build_tree(ll_code_lengths, num_ll_codes, &ll_root);

    TreeNode dist_root;
    build_tree(dist_code_lengths, num_dist_codes, &dist_root);

    return parse_data(starlight, &ll_root, &dist_root);
}


static starlight_status_t build_tree(
    uint8_t *code_lengths, uint16_t alen, TreeNode *root
) {
    uint16_t length_frequency[17];
    memset(length_frequency, 0, sizeof(length_frequency));

    uint8_t max_length = 0;

    for (uint16_t i = 0; i < alen; i++) {
        uint8_t code_length = code_lengths[i];
        if (!code_length) continue;
        ++length_frequency[code_length];

        if (code_length > max_length) {
            max_length = code_length;
        }
    }

    length_frequency[0] = 0;

    uint16_t code = 0;
    uint16_t codes[17];

    for (uint8_t i = 1; i < max_length + 1; i++) {
        code = (code + length_frequency[i-1]) << 1;
        codes[i] = code;
    }

    TreeNode *node;

    root->value = -1;
    root->left = NULL;
    root->right = NULL;

    for (uint16_t i = 0; i < alen; i++) {
        uint8_t code_length = code_lengths[i];
        if (!code_length) continue;

        node = root;
        code = codes[code_length];

        for (int8_t j = code_length - 1; j > -1; j--) {
            node->value = -1;

            if (code & (1 << j)) {
                if (node->right == NULL) {
                    node->right = malloc(sizeof(TreeNode));
                    if (node->right == NULL) return STARLIGHT_S_MALLOC_FAILED;

                    node->right->right = NULL;
                    node->right->left = NULL;
                    node->right->value = -1;
                }
                node = node->right;
            } else {
                if (node->left == NULL) {
                    node->left = malloc(sizeof(TreeNode));
                    if (node->left == NULL) return STARLIGHT_S_MALLOC_FAILED;

                    node->left->right = NULL;
                    node->left->left = NULL;
                    node->left->value = -1;
                }
                node = node->left;
            }
        }
        node->value = (int32_t)i;
        codes[code_length]++;
    }

    return STARLIGHT_S_SUCCESS;
}


starlight_status_t parse_data(
    Starlight *starlight, TreeNode *ll_root, TreeNode *dist_root
) {
    TreeNode *node = ll_root;
    int32_t symbol = -1;

    while (true) {
        uint8_t bd = starlight_read_bit(starlight);
        node = bd ? node->right : node->left;

        if (node == NULL) return STARLIGHT_S_CORRUPT_DATA;
        if (node->value == -1) continue; 

        symbol = node->value;
        node = ll_root;

        if (symbol < 0 || symbol > 288) return STARLIGHT_S_CORRUPT_DATA;

        if (symbol < 256) {
            *starlight->output.data++ = (uint8_t)symbol;
        } else if (symbol == 256) {
            break;
        } else {
            symbol -= 257;
            uint16_t len = LENGTH_BASE[symbol];

            if (LENGTH_EXTRA[symbol]) {
                len += starlight_read_bits_be(starlight, LENGTH_EXTRA[symbol]);
            }

            node = dist_root;
            while (true) {
                bd = starlight_read_bit(starlight);
                node = bd ? node->right : node->left;

                if (node == NULL)
                    return STARLIGHT_S_CORRUPT_DATA;

                if (node->value == -1) continue; 

                symbol = node->value;
                node = ll_root;
                if (symbol < 0 || symbol > 30)
                    return STARLIGHT_S_CORRUPT_DATA;
                
                break;
            }


            uint32_t dist = DIST_BASE[symbol];
            if (DIST_EXTRA[symbol]) {
                dist += starlight_read_bits_be(starlight, DIST_EXTRA[symbol]);
            }

            uint8_t *p = starlight->output.data - dist;
            if (dist == 1) {
                uint8_t v = *p;
                if (len) {
                    do {
                        *starlight->output.data++ = v;
                    } while (--len);
                }
            } else {
                if (len) {
                    do {
                        *starlight->output.data++ = *p++;
                    } while (--len);
                }
            }
        }
    }

    return STARLIGHT_S_SUCCESS;
}


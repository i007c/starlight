
#include "starlight.h"

#include <assert.h>
#include <stdlib.h> // mallocing the tree nodes

typedef struct TreeNode {
    struct TreeNode *left;
    struct TreeNode *right;
    int32_t value;
} TreeNode;

typedef struct TreeTable {
    uint8_t min;
    uint8_t max;
    int16_t table[1 << 16];
} TreeTable;


/*
#define log(...)\
printf("\33[92m%s\33[33m:\33[32m%d\33[m ", __FILE__, __LINE__);\
printf(__VA_ARGS__);\
printf("\n")
*/

#define log(...)


static uint16_t u16_be(StarlightBuffer *buffer) {
    uint8_t a = *buffer->c++;
    uint8_t b = *buffer->c++;

    return (a << 8) + (b);
}

static uint8_t read_bit(StarlightBuffer *buffer) {
    uint8_t c = (*buffer->c >> buffer->bdx) & 1;
    buffer->bdx++;
    if (buffer->bdx >= 8) {
        buffer->c++;
        buffer->bdx= 0;
    }
    return c;
}

static uint32_t read_bits_be(StarlightBuffer *buffer, uint8_t count) {
    uint32_t o = 0;
    for (uint8_t ta = 0; ta < count; ta++) {
        o |= read_bit(buffer) << ta;
    }
    return o;
}

inline static int16_t read_node(StarlightBuffer *input, TreeTable *tree) {
    uint8_t n = 0;
    uint16_t code = 1;
    for (; n < tree->min; n++) {
        code = (code << 1) | read_bit(input);
    }

    for (; n < tree->max + 1; n++) {
        int16_t symbol = tree->table[code];
        if (symbol != -1) return symbol;
        code = (code << 1) | read_bit(input);
    }

    return -1;
}

static starlight_status_t decode_fixed(
    StarlightBuffer *input, StarlightBuffer *output
);
static starlight_status_t decode_dynamic(
    StarlightBuffer *input, StarlightBuffer *output
);
static starlight_status_t build_tree(
    uint8_t *code_lengths, uint16_t array_length, TreeTable *tree
);
static starlight_status_t parse_data(
    StarlightBuffer *input, StarlightBuffer *output,
    TreeTable *ll_tree, TreeTable *dist_tree
);




starlight_status_t starlight_inflate(
    StarlightBuffer *input, StarlightBuffer *output
) {
    starlight_status_t status = STARLIGHT_S_SUCCESS;

    input->c = input->s;
    output->c = output->s;

    uint8_t cfm = *input->c++;
    uint8_t flg = *input->c++;
    if (flg & 32 || (cfm * 256 + flg) % 31) {
        log("preset dict: %d", flg & 32);
        return STARLIGHT_S_CORRUPT_DATA;
    }

    // starlight->png.z_comp_method = cfm & 15;
    // starlight->png.z_win_size = 1 << (8 + (cfm >> 4));
    // starlight->png.z_comp_level = flg >> 6;

    // log("comp method: %d", starlight->png.z_comp_method);
    // log("win size: %d", starlight->png.z_win_size);
    // log("comp level: %d", starlight->png.z_comp_level);

    log("comp method: %d", cfm & 15);
    log("win size: %d", 1 << (8 + (cfm >> 4)));
    log("comp level: %d", flg >> 6);

    bool final = false;
    uint8_t type = 0;

    while (!final) {
        final = read_bit(input);
        type = read_bits_be(input, 2);

        log("final: %d - type: %d", final, type);

        if (type == 0) {
            if (input->bdx) {
                input->bdx = 0;
                input->c++;
            }

            uint16_t data_len = u16_be(input);
            uint16_t data_nlen = u16_be(input);
            if (data_len != ((~data_nlen) & 0xffff))
                return STARLIGHT_S_CORRUPT_DATA;

            // uint16_t data_len = (
            //     read_bits_be(starlight, 8) | (read_bits_be(starlight, 8) << 8)
            // );
            // uint16_t data_len = (
            //     read_bits_be(starlight, 8) | (read_bits_be(starlight, 8) << 8)
            // );
            // uint16_t data_nlen = read_bits(8) | (read_bits(8) << 8);

            memcpy(output->c, input->c, data_len);
            output->c += data_len;
            input->c += data_len;
        } else if (type == 1) {
            if ((status = decode_fixed(input, output)))
                return status;
        } else if (type == 2) {
            if ((status = decode_dynamic(input, output)))
                return status;
        } else {
            return STARLIGHT_S_CORRUPT_DATA;
        }
    }

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


static starlight_status_t decode_fixed(
    StarlightBuffer *input, StarlightBuffer *output
) {
    starlight_status_t status = STARLIGHT_S_SUCCESS;

    // TreeNode ll_root;
    TreeTable ll_tree;
    if ((status = build_tree(FIXED_LENGTHS, 288, &ll_tree)))
        return status;

    // TreeNode dist_root;
    TreeTable dist_tree;
    if ((status = build_tree(FIXED_DIST, 32, &dist_tree)))
        return status;

    return parse_data(input, output, &ll_tree, &dist_tree);
}


static starlight_status_t decode_dynamic(
    StarlightBuffer *input, StarlightBuffer *output
) {
    starlight_status_t status = STARLIGHT_S_SUCCESS;

    uint8_t hlit = read_bits_be(input, 5);
    uint16_t num_ll_codes = hlit + 257;

    uint8_t hdist = read_bits_be(input, 5);
    uint8_t num_dist_codes = hdist + 1;

    uint8_t hclen = read_bits_be(input, 4);
    uint8_t num_cl_codes = hclen + 4;

    uint8_t cl_code_lengths[19];
    memset(cl_code_lengths, 0, sizeof(cl_code_lengths));

    for (uint8_t ci = 0; ci < num_cl_codes; ci++) {
        cl_code_lengths[CL_ORDER[ci]] = read_bits_be(input, 3);
    }

    TreeTable cl_tree;

    // TreeNode cl_root;
    if ((status = build_tree(cl_code_lengths, 19, &cl_tree))) {
        log("cl root build tree faild");
        return status;
    }

    uint8_t ll_code_lengths[288];
    uint8_t dist_code_lengths[32];
    memset(ll_code_lengths, 0, sizeof(ll_code_lengths));
    memset(dist_code_lengths, 0, sizeof(dist_code_lengths));

    uint8_t repeat_count = 0;
    uint16_t repeat_symbol = 0;
    int32_t symbol = 0;

    // TreeNode *node = &cl_root;

    for (uint16_t ix=0; ix < num_ll_codes + num_dist_codes;) {
        if ((symbol = read_node(input, &cl_tree)) == -1)
            return STARLIGHT_S_CORRUPT_DATA;

        // symbol = node->value;
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
            repeat_count = read_bits_be(input, 2) + 3;
        } else if (symbol == 17) {
            repeat_count = read_bits_be(input, 3) + 3;
            repeat_symbol = 0;
        } else if (symbol == 18) {
            repeat_count = read_bits_be(input, 7) + 11;
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

    // return STARLIGHT_S_NOT_IMPLEMENTED;

    TreeTable ll_root;
    build_tree(ll_code_lengths, num_ll_codes, &ll_root);

    TreeTable dist_root;
    build_tree(dist_code_lengths, num_dist_codes, &dist_root);

    return parse_data(input, output, &ll_root, &dist_root);
}


static starlight_status_t build_tree(
    uint8_t *code_lengths, uint16_t alen, TreeTable *tree
) {
    uint16_t length_frequency[16];
    memset(length_frequency, 0, sizeof(length_frequency));

    // log("alen: %d", alen);

    tree->max = 0;
    tree->min = 100;
    // uint8_t min_length = 128;
    // uint8_t max_length = 0;

    for (uint16_t i = 0; i < alen; i++) {
        uint8_t code_length = code_lengths[i];
        if (!code_length) continue;
        ++length_frequency[code_length];

        if (code_length > tree->max) {
            tree->max = code_length;
        }
        if (code_length < tree->min) {
            tree->min = code_length;
        }
    }

    length_frequency[0] = 0;

    uint16_t code = 0;
    uint16_t codes[16];

    log("min: %d - max: %d", tree->min, tree->max);

    for (uint8_t i = 1; i < tree->max + 1; i++) {
        code = (code + length_frequency[i-1]) << 1;
        codes[i] = code;
    }

    // TreeNode *node;

    // root->value = -1;
    // root->left = NULL;
    // root->right = NULL;

    // TreeTable tree;
    memset(tree->table, -1, sizeof(tree->table));

    for (uint16_t i = 0; i < alen; i++) {
        uint8_t code_length = code_lengths[i];
        if (!code_length) continue;

        // node = root;
        code = codes[code_length];
        // uint16_t idx = code << (15 - code_length);
        uint16_t idx = code | (1 << code_length);
        // log(
        //     "code len: %d, code: %d, idx: %d, sym: %d",
        //     code_length, code, idx, i
        // );
        tree->table[idx] = i;
        codes[code_length]++;


        // log(
        //     "[%2d] code: \33[93m%5d\33[m -> \33[32m%3d\33[m",
        //     code_length, code, i
        // );

        // printf("[%2d] code: \33[93m%5d\33[m 0b1", code_length, code);

        // for (int8_t j = code_length - 1; j > -1; j--) {
            // node->value = -1;

            // printf("%d", !!(code & (1 << j)));

            // if (code & (1 << j)) {
            //     if (node->right == NULL) {
            //         node->right = malloc(sizeof(TreeNode));
            //         if (node->right == NULL) return STARLIGHT_S_MALLOC_FAILED;
            //
            //         node->right->right = NULL;
            //         node->right->left = NULL;
            //         node->right->value = -1;
            //     }
            //     node = node->right;
            // } else {
            //     if (node->left == NULL) {
            //         node->left = malloc(sizeof(TreeNode));
            //         if (node->left == NULL) return STARLIGHT_S_MALLOC_FAILED;
            //
            //         node->left->right = NULL;
            //         node->left->left = NULL;
            //         node->left->value = -1;
            //     }
            //     node = node->left;
            // }
        // }
        // printf("\33[95m");
        // for (uint8_t j = 0; j < (15 - code_length); j++) {
        //     putchar(' ');
        // }

        // uint16_t idx = code << (15 - code_length);
        // assert(tree.table[idx] == -1);
        // tree.table[idx] = i;
        // printf(" \33[93m%5d ", idx);
        // printf("\33[m -> \33[32m%3d\33[m\n", i);

        // node->value = (int32_t)i;
        // codes[code_length]++;
    }

    return STARLIGHT_S_SUCCESS;
}



static starlight_status_t parse_data(
    StarlightBuffer *input, StarlightBuffer *output,
    TreeTable *ll_tree, TreeTable *dist_tree
) {
    // TreeNode *node;
    int32_t symbol = -1;

    while (true) {
        if ((symbol = read_node(input, ll_tree)) == -1)
            return STARLIGHT_S_CORRUPT_DATA;

        // symbol = node->value;
        if (symbol < 0 || symbol > 288) return STARLIGHT_S_CORRUPT_DATA;

        if (symbol < 256) {
            *output->c++ = (uint8_t)symbol;
        } else if (symbol == 256) {
            break;
        } else {
            symbol -= 257;
            uint16_t len = LENGTH_BASE[symbol];

            if (LENGTH_EXTRA[symbol]) {
                len += read_bits_be(input, LENGTH_EXTRA[symbol]);
            }

            if ((symbol = read_node(input, dist_tree)) == -1)
                return STARLIGHT_S_CORRUPT_DATA;

            // symbol = node->value;
            if (symbol < 0 || symbol > 30) return STARLIGHT_S_CORRUPT_DATA;
            
            uint32_t dist = DIST_BASE[symbol];
            if (DIST_EXTRA[symbol]) {
                dist += read_bits_be(input, DIST_EXTRA[symbol]);
            }

            uint8_t *p = output->c - dist;
            if (dist == 1) {
                uint8_t v = *p;
                if (len) {
                    do {
                        *output->c++ = v;
                    } while (--len);
                }
            } else {
                if (len) {
                    do {
                        *output->c++ = *p++;
                    } while (--len);
                }
            }
        }
    }

    return STARLIGHT_S_SUCCESS;
}


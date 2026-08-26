#include "GIFEncoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PDSIM_GIF_LZW_MINIMUM_CODE_SIZE 2
#define PDSIM_GIF_LZW_CLEAR_CODE 4
#define PDSIM_GIF_LZW_END_CODE 5
#define PDSIM_GIF_LZW_FIRST_DICTIONARY_CODE 6
#define PDSIM_GIF_LZW_MAXIMUM_CODE 4095
#define PDSIM_GIF_LZW_HASH_CAPACITY 5003
#define PDSIM_GIF_SUB_BLOCK_CAPACITY 255

struct pdsim_gif_encoder {
    FILE *stream;
    uint16_t width;
    uint16_t height;
};

typedef struct {
    pdsim_gif_encoder *encoder;
    uint8_t block[PDSIM_GIF_SUB_BLOCK_CAPACITY];
    size_t block_length;
    uint32_t pending_bits;
    unsigned int pending_bit_count;
} pdsim_lzw_output;

typedef struct {
    int16_t codes[PDSIM_GIF_LZW_HASH_CAPACITY];
    uint16_t prefixes[PDSIM_GIF_LZW_HASH_CAPACITY];
    uint8_t suffixes[PDSIM_GIF_LZW_HASH_CAPACITY];
} pdsim_lzw_dictionary;

static bool pdsim_write(
    pdsim_gif_encoder *encoder,
    const void *bytes,
    size_t length
) {
    return length == 0 || fwrite(bytes, 1, length, encoder->stream) == length;
}

static bool pdsim_write_byte(pdsim_gif_encoder *encoder, uint8_t byte) {
    return pdsim_write(encoder, &byte, sizeof(byte));
}

static bool pdsim_write_uint16(pdsim_gif_encoder *encoder, uint16_t value) {
    uint8_t bytes[] = {
        (uint8_t)(value & 0xFFU),
        (uint8_t)(value >> 8U),
    };
    return pdsim_write(encoder, bytes, sizeof(bytes));
}

static bool pdsim_write_header(pdsim_gif_encoder *encoder) {
    static const uint8_t signature[] = {'G', 'I', 'F', '8', '9', 'a'};
    static const uint8_t screen_fields[] = {
        0x80, // Global two-entry color table, one bit per primary color.
        0x00, // Black background palette index.
        0x00, // No pixel aspect-ratio information.
    };
    static const uint8_t color_table[] = {
        0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF,
    };
    static const uint8_t loop_extension[] = {
        0x21, 0xFF, 0x0B,
        'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0',
        0x03, 0x01, 0x00, 0x00, 0x00,
    };

    return pdsim_write(encoder, signature, sizeof(signature))
        && pdsim_write_uint16(encoder, encoder->width)
        && pdsim_write_uint16(encoder, encoder->height)
        && pdsim_write(encoder, screen_fields, sizeof(screen_fields))
        && pdsim_write(encoder, color_table, sizeof(color_table))
        && pdsim_write(encoder, loop_extension, sizeof(loop_extension));
}

static bool pdsim_flush_sub_block(pdsim_lzw_output *output) {
    if (output->block_length == 0) {
        return true;
    }

    bool wrote = pdsim_write_byte(
        output->encoder,
        (uint8_t)output->block_length
    ) && pdsim_write(
        output->encoder,
        output->block,
        output->block_length
    );
    output->block_length = 0;
    return wrote;
}

static bool pdsim_append_sub_block_byte(
    pdsim_lzw_output *output,
    uint8_t byte
) {
    output->block[output->block_length++] = byte;
    return output->block_length < sizeof(output->block)
        || pdsim_flush_sub_block(output);
}

static bool pdsim_write_lzw_code(
    pdsim_lzw_output *output,
    uint16_t code,
    unsigned int code_width
) {
    output->pending_bits |= (uint32_t)code << output->pending_bit_count;
    output->pending_bit_count += code_width;

    while (output->pending_bit_count >= 8) {
        if (!pdsim_append_sub_block_byte(output, (uint8_t)output->pending_bits)) {
            return false;
        }
        output->pending_bits >>= 8U;
        output->pending_bit_count -= 8;
    }
    return true;
}

static bool pdsim_finish_lzw_output(pdsim_lzw_output *output) {
    if (
        output->pending_bit_count > 0
        && !pdsim_append_sub_block_byte(output, (uint8_t)output->pending_bits)
    ) {
        return false;
    }
    return pdsim_flush_sub_block(output)
        && pdsim_write_byte(output->encoder, 0x00);
}

static void pdsim_reset_dictionary(pdsim_lzw_dictionary *dictionary) {
    memset(dictionary->codes, 0xFF, sizeof(dictionary->codes));
}

static size_t pdsim_dictionary_slot(
    const pdsim_lzw_dictionary *dictionary,
    uint16_t prefix,
    uint8_t suffix,
    bool *found
) {
    size_t slot = ((size_t)prefix * 257U + suffix) % PDSIM_GIF_LZW_HASH_CAPACITY;
    for (;;) {
        if (dictionary->codes[slot] < 0) {
            *found = false;
            return slot;
        }
        if (
            dictionary->prefixes[slot] == prefix
            && dictionary->suffixes[slot] == suffix
        ) {
            *found = true;
            return slot;
        }
        slot = (slot + 1) % PDSIM_GIF_LZW_HASH_CAPACITY;
    }
}

static uint8_t pdsim_frame_index(
    const uint8_t *frame,
    size_t bytes_per_row,
    size_t pixel_index,
    uint16_t width
) {
    size_t row = pixel_index / width;
    size_t column = pixel_index % width;
    uint8_t byte = frame[row * bytes_per_row + column / 8];
    return (byte & (uint8_t)(0x80U >> (column % 8))) == 0 ? 0 : 1;
}

static bool pdsim_write_lzw_frame(
    pdsim_gif_encoder *encoder,
    const uint8_t *frame,
    size_t bytes_per_row
) {
    pdsim_lzw_output output = {.encoder = encoder};
    pdsim_lzw_dictionary dictionary;
    pdsim_reset_dictionary(&dictionary);

    unsigned int code_width = PDSIM_GIF_LZW_MINIMUM_CODE_SIZE + 1;
    uint16_t next_code = PDSIM_GIF_LZW_FIRST_DICTIONARY_CODE;
    if (!pdsim_write_lzw_code(&output, PDSIM_GIF_LZW_CLEAR_CODE, code_width)) {
        return false;
    }

    size_t pixel_count = (size_t)encoder->width * encoder->height;
    uint16_t prefix = pdsim_frame_index(
        frame,
        bytes_per_row,
        0,
        encoder->width
    );
    for (size_t pixel_index = 1; pixel_index < pixel_count; pixel_index++) {
        uint8_t suffix = pdsim_frame_index(
            frame,
            bytes_per_row,
            pixel_index,
            encoder->width
        );
        bool found = false;
        size_t slot = pdsim_dictionary_slot(
            &dictionary,
            prefix,
            suffix,
            &found
        );
        if (found) {
            prefix = (uint16_t)dictionary.codes[slot];
            continue;
        }

        if (!pdsim_write_lzw_code(&output, prefix, code_width)) {
            return false;
        }
        if (next_code <= PDSIM_GIF_LZW_MAXIMUM_CODE) {
            dictionary.codes[slot] = (int16_t)next_code;
            dictionary.prefixes[slot] = prefix;
            dictionary.suffixes[slot] = suffix;
            next_code++;
            if (
                code_width < 12
                && next_code == (uint16_t)((1U << code_width) + 1U)
            ) {
                code_width++;
            }
        } else {
            if (!pdsim_write_lzw_code(&output, PDSIM_GIF_LZW_CLEAR_CODE, code_width)) {
                return false;
            }
            pdsim_reset_dictionary(&dictionary);
            code_width = PDSIM_GIF_LZW_MINIMUM_CODE_SIZE + 1;
            next_code = PDSIM_GIF_LZW_FIRST_DICTIONARY_CODE;
        }
        prefix = suffix;
    }

    return pdsim_write_lzw_code(&output, prefix, code_width)
        && pdsim_write_lzw_code(&output, PDSIM_GIF_LZW_END_CODE, code_width)
        && pdsim_finish_lzw_output(&output);
}

pdsim_gif_encoder *pdsim_gif_encoder_create(
    int output_descriptor,
    uint16_t width,
    uint16_t height
) {
    if (output_descriptor < 0 || width == 0 || height == 0) {
        if (output_descriptor >= 0) {
            close(output_descriptor);
        }
        return NULL;
    }

    FILE *stream = fdopen(output_descriptor, "wb");
    if (stream == NULL) {
        close(output_descriptor);
        return NULL;
    }

    pdsim_gif_encoder *encoder = calloc(1, sizeof(*encoder));
    if (encoder == NULL) {
        fclose(stream);
        return NULL;
    }
    encoder->stream = stream;
    encoder->width = width;
    encoder->height = height;

    if (!pdsim_write_header(encoder) || fflush(stream) != 0) {
        pdsim_gif_encoder_destroy(encoder);
        return NULL;
    }
    return encoder;
}

bool pdsim_gif_encoder_add_frame(
    pdsim_gif_encoder *encoder,
    const uint8_t *frame,
    size_t bytes_per_row,
    uint16_t delay_centiseconds
) {
    if (
        encoder == NULL || encoder->stream == NULL || frame == NULL
        || bytes_per_row < ((size_t)encoder->width + 7) / 8
        || delay_centiseconds == 0
    ) {
        return false;
    }

    static const uint8_t graphic_control_prefix[] = {
        0x21, 0xF9, 0x04, 0x00,
    };
    static const uint8_t image_descriptor_prefix[] = {
        0x00, 0x00, 0x00, 0x00, // Left and top positions.
    };
    bool wrote = pdsim_write(
        encoder,
        graphic_control_prefix,
        sizeof(graphic_control_prefix)
    ) && pdsim_write_uint16(encoder, delay_centiseconds)
        && pdsim_write_byte(encoder, 0x00) // Transparent color index, unused.
        && pdsim_write_byte(encoder, 0x00) // Graphic control terminator.
        && pdsim_write_byte(encoder, 0x2C) // Image descriptor marker.
        && pdsim_write(
            encoder,
            image_descriptor_prefix,
            sizeof(image_descriptor_prefix)
        ) && pdsim_write_uint16(encoder, encoder->width)
        && pdsim_write_uint16(encoder, encoder->height)
        && pdsim_write_byte(encoder, 0x00) // Use the global color table.
        && pdsim_write_byte(encoder, PDSIM_GIF_LZW_MINIMUM_CODE_SIZE)
        && pdsim_write_lzw_frame(encoder, frame, bytes_per_row)
        && fflush(encoder->stream) == 0;
    return wrote;
}

bool pdsim_gif_encoder_finish(pdsim_gif_encoder *encoder) {
    if (encoder == NULL || encoder->stream == NULL) {
        return false;
    }

    bool wrote = pdsim_write_byte(encoder, 0x3B)
        && fflush(encoder->stream) == 0;
    FILE *stream = encoder->stream;
    encoder->stream = NULL;
    return fclose(stream) == 0 && wrote;
}

void pdsim_gif_encoder_destroy(pdsim_gif_encoder *encoder) {
    if (encoder == NULL) {
        return;
    }
    if (encoder->stream != NULL) {
        fclose(encoder->stream);
    }
    free(encoder);
}

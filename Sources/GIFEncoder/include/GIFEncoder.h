#ifndef GIF_ENCODER_H
#define GIF_ENCODER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct pdsim_gif_encoder pdsim_gif_encoder;

// Takes ownership of output_descriptor. Frames are packed most-significant-bit
// first, with one palette index per bit and any row padding ignored.
pdsim_gif_encoder *pdsim_gif_encoder_create(
    int output_descriptor,
    uint16_t width,
    uint16_t height
);

bool pdsim_gif_encoder_add_frame(
    pdsim_gif_encoder *encoder,
    const uint8_t *frame,
    size_t bytes_per_row,
    uint16_t delay_centiseconds
);

// Writes the GIF trailer and closes the owned output descriptor.
bool pdsim_gif_encoder_finish(pdsim_gif_encoder *encoder);

// Closes unfinished output before releasing the encoder.
void pdsim_gif_encoder_destroy(pdsim_gif_encoder *encoder);

#endif

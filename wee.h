// wee.h
// 31-Dec-15  Markku-Juhani O. Saarinen <mjos@iki.fi>

#ifndef WEE_H
#define WEE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Range buffer structure
typedef struct {
    uint8_t *buf;                       // output buffer
    size_t ptr, max;                    // pointer and maximum size (bytes)
    int bit;                            // bit index
    uint32_t byt;                       // partial byte
    uint32_t b, l, v;                   // range indicators, optional input
} aric_rb_t;

// == aric.c ==

// Initialize frequencies to "balanced".
void aric_freqinit(uint32_t freq[][2], size_t bits);

// Divide frequencies in half, "de-emphasizing past".
void aric_freqhalf(uint32_t freq[][2], size_t bits);

// Update a frequency distribution for "bits"-sized word x.
void aric_addfreq(uint32_t freq[][2], size_t bits, uint32_t x);

// Initialize a range buffer.
void aric_init_rb(aric_rb_t *rb, void *buf, size_t len, int dec);

// Encode a "bits"-sized word to output stream.
int aric_enc(aric_rb_t *rb,             // output range buffer
    uint32_t iwrd,                      // input word to be encoded
    uint32_t freq[][2], size_t bits);   // binary frequencies

// Finalize output range buffer.
size_t aric_final_out(aric_rb_t *rb);

// Decode input word.
uint32_t aric_dec(aric_rb_t *rb,        // input range buffer
    uint32_t freq[][2], size_t bits);   // binary frequencies

// == weef.c ==

// Compress fin to fout. Return output size or 0 in case of error.
size_t wee_file_enc(FILE *fin, FILE *fout, int verb);

// Decompress fin to fout. Return output size or 0 in case of error.
size_t wee_file_dec(FILE *fin, FILE *fout, int verb);

#endif

